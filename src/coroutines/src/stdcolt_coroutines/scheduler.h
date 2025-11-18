/*****************************************************************/ /**
 * @file   scheduler.h
 * @brief  Contains `ThreadPoolScheduler`, for scheduling coroutines.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_COROUTINES_SCHEDULER
#define __HG_STDCOLT_COROUTINES_SCHEDULER

#include <coroutine>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <type_traits>
#include <concurrentqueue.h>
#include <stdcolt_coroutines/task.h>
#include <stdcolt_coroutines/synchronization/flag.h>

namespace stdcolt::coroutines
{
  // forward declarations
  template<typename T>
  class ScheduledTask;
  class ThreadPoolScheduler;

  /// @brief Promise for use by ScheduledTask
  struct ScheduledTaskPromiseBase
  {
    std::atomic<size_t> refcount{0};

    /// @brief Awaitable that starts the continuation.
    struct final_awaitable
    {
      bool await_ready() const noexcept { return false; }

      template<typename PROMISE>
      std::coroutine_handle<> await_suspend(
          std::coroutine_handle<PROMISE> handle) const noexcept;

      void await_resume() const noexcept {}
    };

    /// @brief Initially, suspend the coroutine
    auto initial_suspend() const noexcept { return std::suspend_always{}; }
    /// @brief Final suspension point
    auto final_suspend() const noexcept { return final_awaitable{}; }

    /// @brief The coroutine to start executing after the final suspend
    std::coroutine_handle<> continuation{};
    /// @brief The owning thread pool
    ThreadPoolScheduler* scheduler = nullptr;
    /// @brief The Node* of the thread pool
    void* node = nullptr;
    /// @brief The worker ID (index into the array of threads)
    size_t worker_id = 0;
  };

  /// @brief For ScheduledTaskPromiseBase, a ScheduledTask<T> is returned
  template<>
  struct return_object_for_promise_base<ScheduledTaskPromiseBase>
  {
    template<typename T>
    using type = ScheduledTask<T>;
  };

  template<typename Promise>
  inline void intrusive_add_ref(std::coroutine_handle<Promise> h) noexcept
  {
    static_assert(
        std::is_base_of_v<ScheduledTaskPromiseBase, Promise>,
        "Promise must derive from ScheduledTaskPromiseBase");
    auto& base = static_cast<ScheduledTaskPromiseBase&>(h.promise());
    base.refcount.fetch_add(1, std::memory_order_relaxed);
  }

  template<typename Promise>
  inline void intrusive_release_ref(std::coroutine_handle<Promise> h) noexcept
  {
    static_assert(
        std::is_base_of_v<ScheduledTaskPromiseBase, Promise>,
        "Promise must derive from ScheduledTaskPromiseBase");
    auto& base = static_cast<ScheduledTaskPromiseBase&>(h.promise());
    if (base.refcount.fetch_sub(1, std::memory_order_acq_rel) == 1)
      h.destroy();
  }

  template<typename T>
  class ScheduledTask
  {
  public:
    using promise_type = generic_task_promise<T, ScheduledTaskPromiseBase>;
    using value_type   = T;

  private:
    std::coroutine_handle<promise_type> handle = nullptr;

    template<bool Move>
    struct awaitable_base
    {
      std::coroutine_handle<promise_type> h;

      bool await_ready() const noexcept { return !h || h.done(); }

      bool await_suspend(std::coroutine_handle<> cont) noexcept
      {
        if (!h || h.done())
          return false;

        h.promise().continuation = cont;
        return true;
      }

      decltype(auto) await_resume()
      {
        STDCOLT_pre(h, "ScheduledTask was empty");

        // void result
        if constexpr (std::is_void_v<T>)
        {
          if constexpr (Move)
            (void)std::move(h.promise()).result();
          else
            h.promise().result();
          // no return
        }
        // reference result: T is U&
        else if constexpr (std::is_reference_v<T>)
        {
          // promise().result() already returns U&
          return h.promise().result();
        }
        // value result: T is non-void, non-reference
        else
        {
          if constexpr (Move)
            return std::move(h.promise()).result();
          else
            return h.promise().result();
        }
      }
    };

  public:
    ScheduledTask() noexcept = default;

    explicit ScheduledTask(std::coroutine_handle<promise_type> h) noexcept
        : handle(h)
    {
      if (handle)
        intrusive_add_ref(handle);
    }

    ScheduledTask(const ScheduledTask& other) noexcept
        : handle(other.handle)
    {
      if (handle)
        intrusive_add_ref(handle);
    }

    ScheduledTask& operator=(const ScheduledTask& other) noexcept
    {
      if (this == &other)
        return *this;
      if (handle)
        intrusive_release_ref(handle);
      handle = other.handle;
      if (handle)
        intrusive_add_ref(handle);
      return *this;
    }

    ScheduledTask(ScheduledTask&& other) noexcept
        : handle(other.handle)
    {
      other.handle = nullptr;
    }

    ScheduledTask& operator=(ScheduledTask&& other) noexcept
    {
      if (this == &other)
        return *this;
      if (handle)
        intrusive_release_ref(handle);
      handle       = other.handle;
      other.handle = nullptr;
      return *this;
    }

    ~ScheduledTask()
    {
      if (handle)
        intrusive_release_ref(handle);
    }

    bool valid() const noexcept { return static_cast<bool>(handle); }
    bool is_ready() const noexcept { return !handle || handle.done(); }

    auto get_handle() const noexcept { return handle; }

    auto operator co_await() const& noexcept
    {
      return awaitable_base<false>{handle};
    }
    auto operator co_await() const&& noexcept
    {
      return awaitable_base<true>{handle};
    }
    auto when_ready() const noexcept
    {
      struct when_ready_awaitable
      {
        std::coroutine_handle<promise_type> h;

        bool await_ready() const noexcept { return !h || h.done(); }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> cont) noexcept
        {
          if (!h || h.done())
            return std::noop_coroutine();
          h.promise().continuation = cont;
          return h;
        }

        void await_resume() const noexcept {}
      };

      return when_ready_awaitable{handle};
    }
  };

  /// @brief Multithreaded coroutine scheduler.
  class ThreadPoolScheduler
  {
    struct Node
    {
      std::coroutine_handle<> h;
      void (*release_fn)(std::coroutine_handle<>) = nullptr;
    };

    /// @brief Worker state
    struct Worker
    {
      std::thread thread;
      moodycamel::ConcurrentQueue<Node*> queue{};
    };

    /// @brief The next worker to use when adopting a coroutine
    std::atomic<size_t> _next_worker{0};
    /// @brief The workers
    std::vector<Worker> _workers;
    /// @brief If true, then the workers must stop.
    std::atomic<bool> _stop{false};
    /// @brief The number of active tasks (adopted and not removed)
    std::atomic<size_t> _active_tasks{0};

    /// @brief Mutex for `wait_idle`
    std::mutex _wait_mutex;
    /// @brief Condition variable for `wait_idle`
    std::condition_variable _wait_cv;

    /// @brief Work epoch to efficiently sleep when no work exist
    std::atomic<uint32_t> _work_epoch{};

  public:
    ThreadPoolScheduler()                                      = delete;
    ThreadPoolScheduler(ThreadPoolScheduler&&)                 = delete;
    ThreadPoolScheduler(const ThreadPoolScheduler&)            = delete;
    ThreadPoolScheduler& operator=(ThreadPoolScheduler&&)      = delete;
    ThreadPoolScheduler& operator=(const ThreadPoolScheduler&) = delete;

    /// @brief Constructs a scheduler with a specific number of threads
    /// @param thread_count The number of threads, max(thread_count, 1)
    explicit ThreadPoolScheduler(size_t thread_count)
    {
      if (thread_count == 0)
        thread_count = 1;

      _workers.resize(thread_count);
      for (size_t i = 0; i < thread_count; ++i)
        _workers[i].thread = std::thread([this, i] { run_worker(i); });
    }

    /// @brief Destroys the scheduler, waiting for all the tasks to be done
    ~ThreadPoolScheduler()
    {
      _stop.store(true, std::memory_order_release);
      // wake up all the workers that are sleeping
      _work_epoch.fetch_add(1, std::memory_order_release);
      _work_epoch.notify_all();

      for (auto& w : _workers)
      {
        if (w.thread.joinable())
          w.thread.join();
      }
    }

    /// @brief Waits for all the currently scheduled tasks to be done
    void wait_idle()
    {
      std::unique_lock lk(_wait_mutex);
      _wait_cv.wait(
          lk, [this] { return _active_tasks.load(std::memory_order_acquire) == 0; });
    }

    /// @brief Returns the number of worker threads
    /// @return The number of worker threads (>= 1)
    size_t worker_count() const noexcept { return _workers.size(); }

    /// @brief Adopts a coroutine and schedules it
    /// @tparam T The produced type
    /// @param h The coroutine handle
    /// @return ScheduledTask
    template<typename T>
    ScheduledTask<T> adopt(
        std::coroutine_handle<typename ScheduledTask<T>::promise_type> h)
    {
      using promise_type = typename ScheduledTask<T>::promise_type;

      auto* node = new Node{
          h, [](std::coroutine_handle<> uh)
          {
            auto th =
                std::coroutine_handle<promise_type>::from_address(uh.address());
            intrusive_release_ref(th);
          }};

      intrusive_add_ref(h);
      _active_tasks.fetch_add(1, std::memory_order_acq_rel);

      const size_t idx =
          _next_worker.fetch_add(1, std::memory_order_relaxed) % _workers.size();

      auto& p     = h.promise();
      p.scheduler = this;
      p.node      = node;
      p.worker_id = idx;

      enqueue_node(node, idx);
      return ScheduledTask<T>{h};
    }

    struct yield_awaiter
    {
      ThreadPoolScheduler* sched;

      bool await_ready() const noexcept { return false; }

      template<typename Promise>
      void await_suspend(std::coroutine_handle<Promise> h) const noexcept
      {
        auto& p    = h.promise();
        auto* s    = p.scheduler ? p.scheduler : sched;
        auto* node = static_cast<Node*>(p.node);
        auto idx   = p.worker_id;

        if (s && node && idx < s->_workers.size())
          s->enqueue_node(node, idx);
      }

      void await_resume() const noexcept {}
    };

    auto yield() noexcept { return yield_awaiter{this}; }

  private:
    friend struct ScheduledTaskPromiseBase::final_awaitable;

    void enqueue_node(Node* node, size_t idx) noexcept
    {
      _workers[idx].queue.enqueue(node);
      _work_epoch.fetch_add(1, std::memory_order_release);
      _work_epoch.notify_one();
    }

    void run_worker(size_t index)
    {
      Worker& self = _workers[index];

      while (!_stop.load(std::memory_order_acquire))
      {
        Node* node = nullptr;

        if (self.queue.try_dequeue(node))
        {
          if (node)
            resume_or_destroy(node);
          continue;
        }

        // try stealing from others
        bool stolen = false;
        for (size_t i = 0; i < _workers.size(); ++i)
        {
          if (i == index)
            continue;
          if (_workers[i].queue.try_dequeue(node))
          {
            stolen = true;
            if (node)
              resume_or_destroy(node);
            break;
          }
        }

        if (stolen)
          continue;

        auto expected = _work_epoch.load(std::memory_order_acquire);
        if (_stop.load(std::memory_order_acquire))
          break;
        _work_epoch.wait(expected, std::memory_order_relaxed);
      }

      // drain remaining work on shutdown
      Node* node = nullptr;
      while (self.queue.try_dequeue(node))
      {
        if (node)
          resume_or_destroy(node);
      }
    }

    void mark_one_as_done()
    {
      const auto prev = _active_tasks.fetch_sub(1, std::memory_order_acq_rel);
      if (prev == 1)
      {
        std::lock_guard lk(_wait_mutex);
        _wait_cv.notify_all();
      }
    }

    void resume_or_destroy(Node* node)
    {
      auto finalize = [this, node]()
      {
        if (auto h = node->h; h)
        {
          if (node->release_fn)
            node->release_fn(h);
        }
        delete node;
      };

      auto h = node->h;
      if (!h)
      {
        finalize();
        return;
      }
      if (h.done())
      {
        finalize();
        return;
      }
      h.resume();
      if (h.done())
        finalize();
    }
  };

  /// @brief Converts a Task to a ScheduledTask.
  /// As ScheduledTask is non-owning, it must be passed to a function
  /// that will take ownership, else the coroutine will be leaked.
  /// @tparam T The produced type
  /// @param t The task to convert
  /// @return ScheduledTask
  template<typename T>
  ScheduledTask<T> task_to_scheduled_task(Task<T> t)
  {
    co_return co_await std::move(t);
  }

  /// @brief Schedules a ScheduledTask in a ThreadPoolScheduler.
  /// The scheduler takes ownership of the ScheduledTask.
  /// @tparam T The produced type
  /// @param sched The scheduler
  /// @param st The task to schedule
  /// @return The scheduled task
  template<typename T>
  ScheduledTask<T> co_spawn(ThreadPoolScheduler& sched, ScheduledTask<T> st)
  {
    using promise_type = typename ScheduledTask<T>::promise_type;
    auto h             = st.get_handle();
    if (!h)
      return ScheduledTask<T>{};
    return sched.adopt<T>(h);
  }

  /// @brief Schedules a Task in a ThreadPoolScheduler.
  /// The scheduler takes ownership of the Task.
  /// @tparam T The produced type
  /// @param sched The scheduler
  /// @param task The task to schedule
  /// @return The scheduled task
  template<typename T>
  ScheduledTask<T> co_spawn(ThreadPoolScheduler& sched, Task<T>&& task)
  {
    ScheduledTask<T> st = task_to_scheduled_task(std::move(task));
    using promise_type  = typename ScheduledTask<T>::promise_type;
    auto h              = st.get_handle();
    if (!h)
      return ScheduledTask<T>{};
    return sched.adopt<T>(h);
  }

  template<typename PROMISE>
  std::coroutine_handle<> ScheduledTaskPromiseBase::final_awaitable::await_suspend(
      std::coroutine_handle<PROMISE> handle) const noexcept
  {
    auto ret = handle.promise().continuation;
    handle.promise().scheduler->mark_one_as_done();
    return ret ? ret : std::noop_coroutine();
  }
} // namespace stdcolt::coroutines

#endif // !__HG_STDCOLT_COROUTINES_SCHEDULER
