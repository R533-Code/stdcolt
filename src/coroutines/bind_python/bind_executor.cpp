#include <nanobind/nanobind.h>
#include <nanobind/stl/tuple.h>

#include <coroutine>
#include <memory>
#include <utility>
#include <stdcolt_coroutines/executor.h>

namespace nb = nanobind;
using namespace stdcolt::coroutines;

struct PyCoroutineState;

struct PyAwaitable
{
  struct Concept
  {
    virtual ~Concept()                                       = default;
    virtual void start(std::shared_ptr<PyCoroutineState> st) = 0;
  };

  std::shared_ptr<Concept> impl;

  PyAwaitable() = default;

  template<typename Impl>
  explicit PyAwaitable(std::shared_ptr<Impl> p)
      : impl(std::move(p))
  {
  }

  void start(std::shared_ptr<PyCoroutineState> st) { impl->start(std::move(st)); }
};

struct PyAwaitIter
{
  nb::object aw;
  bool done = false;
};

struct BindingDetachedTask
{
  struct promise_type
  {
    BindingDetachedTask get_return_object() noexcept
    {
      return BindingDetachedTask{
          std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_never initial_suspend() const noexcept { return {}; }

    struct final_awaitable
    {
      bool await_ready() const noexcept { return false; }
      void await_suspend(std::coroutine_handle<> h) const noexcept { h.destroy(); }
      void await_resume() const noexcept {}
    };

    auto final_suspend() const noexcept { return final_awaitable{}; }
    void return_void() const noexcept {}
    void unhandled_exception() const noexcept
    {
      std::terminate();
    }
  };

  std::coroutine_handle<promise_type> handle{};

  BindingDetachedTask() noexcept = default;

  explicit BindingDetachedTask(std::coroutine_handle<promise_type> h) noexcept
      : handle(h)
  {
  }

  BindingDetachedTask(BindingDetachedTask&& other) noexcept
      : handle(std::exchange(other.handle, std::coroutine_handle<promise_type>{}))
  {
  }

  BindingDetachedTask& operator=(BindingDetachedTask&& other) noexcept
  {
    if (this != &other)
    {
      if (handle)
        handle.destroy();
      handle = std::exchange(other.handle, std::coroutine_handle<promise_type>{});
    }
    return *this;
  }

  ~BindingDetachedTask()
  {
    if (handle)
      handle.destroy();
  }

  BindingDetachedTask(const BindingDetachedTask&)            = delete;
  BindingDetachedTask& operator=(const BindingDetachedTask&) = delete;
};

struct PyCoroutineStateDeleter
{
  void operator()(PyCoroutineState* p) const noexcept
  {
    if (!p)
      return;
    // ensure nb::object members are destroyed with GIL
    nb::gil_scoped_acquire gil;
    delete p;
  }
};

struct PyCoroutineState
{
  // executor driving this coroutine
  ThreadPoolExecutor* ex = nullptr;
  // Python coroutine object (fn(*args))
  nb::object coro;
  // coro.__await__() iterator
  nb::object await_iter;
  // optional return value (unused for now)
  nb::object result;
  std::exception_ptr error;
  bool done = false;
  // C++ coroutine awaiting us
  ThreadPoolExecutor::handle_t handle{};
};

static void step_py_coroutine(std::shared_ptr<PyCoroutineState> st);

static BindingDetachedTask start_step(
    ThreadPoolExecutor* ex, std::shared_ptr<PyCoroutineState> st)
{
  co_await ex->schedule();
  step_py_coroutine(std::move(st));
  co_return;
}

struct PyCoroutineTask
{
  std::shared_ptr<PyCoroutineState> st;

  bool await_ready() const noexcept { return st->done; }

  void await_suspend(ThreadPoolExecutor::handle_t h)
  {
    st->handle             = h;
    BindingDetachedTask dt = start_step(st->ex, st);
    dt.handle              = nullptr;
  }

  nb::object await_resume()
  {
    if (st->error)
      std::rethrow_exception(st->error);
    return st->result; // currently None by default
  }
};

static BindingDetachedTask resume_on_executor(
    ThreadPoolExecutor* ex, ThreadPoolExecutor::handle_t h)
{
  co_await ex->schedule();
  if (h && !h.done())
    h.resume();
  co_return;
}

static void resume_caller(const std::shared_ptr<PyCoroutineState>& st)
{
  auto h                 = st->handle;
  BindingDetachedTask dt = resume_on_executor(st->ex, h);
  dt.handle              = nullptr; // detach
}

static void step_py_coroutine(std::shared_ptr<PyCoroutineState> st)
{
  nb::gil_scoped_acquire gil;

  try
  {
    if (!st->await_iter.is_valid())
      st->await_iter = st->coro.attr("__await__")();

    PyObject* next_obj = PyIter_Next(st->await_iter.ptr());
    if (!next_obj)
    {
      if (PyErr_Occurred())
      {
        if (PyErr_ExceptionMatches(PyExc_StopIteration))
        {
          PyErr_Clear();
          st->done = true;
          resume_caller(st);
          return;
        }
        else
        {
          st->error = std::current_exception();
          PyErr_Clear();
          st->done = true;
          resume_caller(st);
          return;
        }
      }

      st->done = true;
      resume_caller(st);
      return;
    }

    nb::object yielded = nb::steal<nb::object>(next_obj);
    try
    {
      auto& pyaw = nb::cast<PyAwaitable&>(yielded);
      pyaw.start(std::move(st));
      return;
    }
    catch (const nb::cast_error&)
    {
      st->error = std::make_exception_ptr(
          std::runtime_error("Unsupported object yielded from Python coroutine"));
      st->done = true;
      resume_caller(st);
    }
  }
  catch (...)
  {
    st->error = std::current_exception();
    st->done  = true;
    resume_caller(st);
  }
}

template<typename Awaitable>
struct AwaitableModel : PyAwaitable::Concept
{
  ThreadPoolExecutor* ex;
  Awaitable aw;

  AwaitableModel(ThreadPoolExecutor* ex_, Awaitable&& a)
      : ex(ex_)
      , aw(std::move(a))
  {
  }

  static BindingDetachedTask run(
      ThreadPoolExecutor* ex, Awaitable aw, std::shared_ptr<PyCoroutineState> st)
  {
    co_await ex->schedule();
    try
    {
      co_await std::move(aw);
    }
    catch (...)
    {
      // Optionally store into st->error if you want to propagate
      // For now, swallow; Python will just see the coroutine continue
    }
    step_py_coroutine(std::move(st));
    co_return;
  }

  void start(std::shared_ptr<PyCoroutineState> st) override
  {
    BindingDetachedTask dt = run(ex, std::move(aw), std::move(st));
    dt.handle              = nullptr;
  }
};

template<typename Awaitable>
PyAwaitable make_py_awaitable(ThreadPoolExecutor& ex, Awaitable&& aw)
{
  using Model = AwaitableModel<std::decay_t<Awaitable>>;
  auto impl   = std::make_shared<Model>(&ex, std::forward<Awaitable>(aw));
  return PyAwaitable{std::move(impl)};
}

void bind_python_bind_executor(nanobind::module_& m)
{
  nb::class_<PyAwaitIter>(m, "PyAwaitIter")
      .def("__iter__", [](PyAwaitIter& self) -> PyAwaitIter& { return self; })
      .def(
          "__next__",
          [](PyAwaitIter& self) -> nb::object
          {
            if (self.done)
              throw nb::stop_iteration();
            self.done = true;
            return self.aw;
          });

  nb::class_<PyAwaitable>(m, "PyAwaitable")
      .def(
          "__await__",
          [](PyAwaitable& self)
          {
            nb::object o = nb::cast(&self, nb::rv_policy::reference);
            return PyAwaitIter{std::move(o)};
          });

  nb::class_<ThreadPoolExecutor>(m, "ThreadPoolExecutor")
      .def(
          nb::init<size_t>(),
          nb::arg("thread_count") = std::thread::hardware_concurrency())
      .def(
          "stop",
          [](ThreadPoolExecutor& ex)
          {
            nb::gil_scoped_release release;
            ex.stop();
          })
      .def(
          "yield_now", [](ThreadPoolExecutor& ex)
          { return make_py_awaitable(ex, ex.schedule()); });

  nb::class_<AsyncScope>(m, "AsyncScope")
      .def(
          nb::init<ThreadPoolExecutor&>(), nb::arg("executor"),
          nb::rv_policy::reference)
      .def(
          "wait_fence",
          [](AsyncScope& scope)
          {
            nb::gil_scoped_release release;
            scope.wait_fence();
          })
      .def(
          "spawn",
          [](AsyncScope& scope, nb::object fn, nb::args args)
          {
            nb::gil_scoped_acquire gil;
            nb::object coro = fn(*args);

            auto st = std::shared_ptr<PyCoroutineState>(
                new PyCoroutineState(), PyCoroutineStateDeleter{});
            st->ex   = &scope.executor();
            st->coro = std::move(coro);

            PyCoroutineTask task{st};
            scope.spawn(std::move(task));
          });
}
