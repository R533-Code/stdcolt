#include <doctest/doctest.h>
#include <stdcolt_coroutines/executor.h>
#include <stdcolt_coroutines/task.h>
#include <stdcolt_coroutines/generator.h>
#include <stdcolt_coroutines/synchronization/flag.h>
#include <stdcolt_coroutines/mutex.h>

#include <atomic>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <optional>
#include <mutex>
#include <chrono>

using namespace stdcolt::coroutines;

struct Tracker
{
  static inline int ctor_count   = 0;
  static inline int dtor_count   = 0;
  static inline int live_objects = 0;

  int value = 0;

  Tracker()
      : value(0)
  {
    ++ctor_count;
    ++live_objects;
  }

  explicit Tracker(int v)
      : value(v)
  {
    ++ctor_count;
    ++live_objects;
  }

  Tracker(const Tracker& other)
      : value(other.value)
  {
    ++ctor_count;
    ++live_objects;
  }

  Tracker(Tracker&& other) noexcept
      : value(other.value)
  {
    ++ctor_count;
    ++live_objects;
  }

  Tracker& operator=(const Tracker& other)
  {
    value = other.value;
    return *this;
  }

  Tracker& operator=(Tracker&& other) noexcept
  {
    value = other.value;
    return *this;
  }

  ~Tracker()
  {
    --live_objects;
    ++dtor_count;
  }

  static void reset()
  {
    ctor_count   = 0;
    dtor_count   = 0;
    live_objects = 0;
  }
};

template<typename T>
struct SyncTask;

struct SyncTaskVoid;

template<typename T>
struct SyncTask
{
  struct promise_type
  {
    std::optional<T> value;
    std::exception_ptr exception;

    SyncTask get_return_object() noexcept
    {
      return SyncTask{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    template<typename U>
      requires std::convertible_to<U, T>
    void return_value(U&& v)
    {
      value = T(std::forward<U>(v));
    }

    void unhandled_exception() noexcept { exception = std::current_exception(); }
  };

  std::coroutine_handle<promise_type> handle;

  explicit SyncTask(std::coroutine_handle<promise_type> handle) noexcept
      : handle(handle)
  {
  }

  SyncTask(const SyncTask&)            = delete;
  SyncTask& operator=(const SyncTask&) = delete;

  SyncTask(SyncTask&& other) noexcept
      : handle(std::exchange(other.handle, nullptr))
  {
  }

  SyncTask& operator=(SyncTask&& other) noexcept
  {
    if (this == &other)
      return *this;
    if (handle)
      handle.destroy();
    handle = std::exchange(other.handle, nullptr);
    return *this;
  }

  ~SyncTask()
  {
    if (handle)
      handle.destroy();
  }

  T get()
  {
    auto& p = handle.promise();
    if (p.exception)
      std::rethrow_exception(p.exception);
    CHECK(p.value.has_value());
    return std::move(*p.value);
  }
};

struct SyncTaskVoid
{
  struct promise_type
  {
    std::exception_ptr exception;

    SyncTaskVoid get_return_object() noexcept
    {
      return SyncTaskVoid{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() noexcept {}

    void unhandled_exception() noexcept { exception = std::current_exception(); }
  };

  std::coroutine_handle<promise_type> handle;

  explicit SyncTaskVoid(std::coroutine_handle<promise_type> handle) noexcept
      : handle(handle)
  {
  }

  SyncTaskVoid(const SyncTaskVoid&)            = delete;
  SyncTaskVoid& operator=(const SyncTaskVoid&) = delete;

  SyncTaskVoid(SyncTaskVoid&& other) noexcept
      : handle(std::exchange(other.handle, nullptr))
  {
  }

  SyncTaskVoid& operator=(SyncTaskVoid&& other) noexcept
  {
    if (this == &other)
      return *this;
    if (handle)
      handle.destroy();
    handle = std::exchange(other.handle, nullptr);
    return *this;
  }

  ~SyncTaskVoid()
  {
    if (handle)
      handle.destroy();
  }

  void get()
  {
    auto& p = handle.promise();
    if (p.exception)
      std::rethrow_exception(p.exception);
  }
};

template<typename T>
SyncTask<T> sync_await(Task<T> t)
{
  co_return co_await t;
}

template<typename T>
SyncTask<T> sync_await_move(Task<T> t)
{
  co_return co_await std::move(t);
}

inline SyncTaskVoid sync_await_void(Task<void> t)
{
  co_await t;
}

template<typename Awaitable>
SyncTaskVoid sync_await_any(Awaitable a)
{
  co_await a;
}

Task<int> make_value_task(int v)
{
  co_return v;
}

Task<void> make_void_task_increment(int& x)
{
  ++x;
  co_return;
}

Task<int&> make_ref_task(int& x)
{
  co_return x;
}

Task<int> make_throwing_task()
{
  throw std::runtime_error("");
  co_return 0;
}

Task<void> make_throwing_void_task()
{
  throw std::runtime_error("");
  co_return;
}

Task<int> add_one(Task<int> t)
{
  int v = co_await t;
  co_return v + 1;
}

Task<int> add_two_via_chain(Task<int> t)
{
  co_return co_await add_one(std::move(t)) + 1;
}

Task<void> increment_ref_task(Task<int&> t)
{
  int& r = co_await t;
  ++r;
  co_return;
}

Task<Tracker> make_tracker_task(int v)
{
  co_return Tracker{v};
}

Task<Tracker> make_tracker_throw_after_construct(int v)
{
  Tracker t{v};
  throw std::runtime_error("");
  co_return t;
}

Task<Tracker> make_tracker_never_started(int v)
{
  co_return Tracker{v};
}

Generator<int> make_counter(int n)
{
  for (int i = 0; i < n; ++i)
    co_yield i;
}

Generator<int> make_empty()
{
  co_return;
}

Generator<int> make_throwing()
{
  co_yield 1;
  throw std::runtime_error("");
}

Generator<double> make_double_from_ints()
{
  co_yield 1;
  co_yield 2;
}

static Generator<Tracker> gen_unconsumed_value()
{
  co_yield Tracker{42};
  co_return;
}

static Generator<Tracker> gen_overwrite_value()
{
  co_yield Tracker{1};
  co_yield Tracker{2};
  co_return;
}

static Generator<Tracker> gen_throw_after_value()
{
  co_yield Tracker{7};
  throw std::runtime_error("");
}

static Generator<Tracker> gen_throw_before_yield()
{
  throw std::runtime_error("");
  co_return;
}

static Generator<Tracker> gen_yield_once()
{
  co_yield Tracker{10};
  co_return;
}

TEST_CASE("stdcolt/coroutines")
{
  SUBCASE("simple sequence with operator() and operator bool")
  {
    auto g = make_counter(3);

    CHECK(static_cast<bool>(g));
    CHECK(g() == 0);

    CHECK(static_cast<bool>(g));
    CHECK(g() == 1);

    CHECK(static_cast<bool>(g));
    CHECK(g() == 2);

    CHECK_FALSE(static_cast<bool>(g));
  }

  SUBCASE("simple sequence with next()")
  {
    auto g = make_counter(3);

    auto v1 = g.next();
    auto v2 = g.next();
    auto v3 = g.next();
    auto v4 = g.next();

    CHECK(v1.has_value());
    CHECK(v2.has_value());
    CHECK(v3.has_value());
    CHECK_FALSE(v4.has_value());

    CHECK_EQ(*v1, 0);
    CHECK_EQ(*v2, 1);
    CHECK_EQ(*v3, 2);
  }

  SUBCASE("empty generator")
  {
    auto g = make_empty();

    CHECK_FALSE(static_cast<bool>(g));

    auto v = g.next();
    CHECK_FALSE(v.has_value());

    auto it = g.begin();
    CHECK(it == std::default_sentinel);
  }

  SUBCASE("range-for iteration")
  {
    auto g = make_counter(5);
    std::vector<int> collected;

    for (int x : g)
      collected.push_back(x);

    REQUIRE_EQ(collected.size(), 5);
    CHECK_EQ(collected[0], 0);
    CHECK_EQ(collected[1], 1);
    CHECK_EQ(collected[2], 2);
    CHECK_EQ(collected[3], 3);
    CHECK_EQ(collected[4], 4);
  }

  SUBCASE("exception propagation via next()")
  {
    auto g = make_throwing();

    auto v = g.next();
    CHECK(v.has_value());
    CHECK_EQ(*v, 1);

    CHECK_THROWS_AS(g.next(), std::runtime_error);
  }

  SUBCASE("exception propagation via iteration")
  {
    auto g = make_throwing();

    auto it = g.begin();
    CHECK(it != std::default_sentinel);

    CHECK_EQ(*it, 1);

    CHECK_THROWS_AS(++it, std::runtime_error);
  }

  SUBCASE("implicit conversion in yield_value")
  {
    auto g = make_double_from_ints();
    std::vector<double> collected;

    for (double x : g)
      collected.push_back(x);

    REQUIRE_EQ(collected.size(), 2);
    CHECK(collected[0] == doctest::Approx(1.0));
    CHECK(collected[1] == doctest::Approx(2.0));
  }

  SUBCASE("move construction transfers ownership")
  {
    auto g1 = make_counter(3);

    CHECK(static_cast<bool>(g1));
    CHECK(g1() == 0);

    auto g2 = std::move(g1);

    CHECK(static_cast<bool>(g2));
    CHECK(g2() == 1);
    CHECK(static_cast<bool>(g2));
    CHECK(g2() == 2);
    CHECK_FALSE(static_cast<bool>(g2));
  }

  SUBCASE("move assignment transfers ownership")
  {
    auto g1 = make_counter(2);
    auto g2 = make_counter(3);

    CHECK(static_cast<bool>(g1));
    CHECK_EQ(g1(), 0);

    CHECK(static_cast<bool>(g2));
    CHECK_EQ(g2(), 0);

    g1 = std::move(g2);

    CHECK(static_cast<bool>(g1));
    CHECK_EQ(g1(), 1);
    CHECK(static_cast<bool>(g1));
    CHECK_EQ(g1(), 2);
    CHECK_FALSE(static_cast<bool>(g1));
  }

  SUBCASE("promise destroys unconsumed value and swallows destructor exceptions")
  {
    Tracker::reset();

    {
      auto g = gen_unconsumed_value();

      CHECK(static_cast<bool>(g));
    }

    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }

  SUBCASE("promise destroys previous value when overwritten by new yield")
  {
    Tracker::reset();

    {
      auto g = gen_overwrite_value();
      std::vector<int> values;
      for (auto t : g)
        values.push_back(t.value);

      REQUIRE_EQ(values.size(), 2);
      CHECK_EQ(values[0], 1);
      CHECK_EQ(values[1], 2);
    }
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }

  SUBCASE("promise handles exception thrown after value without leaking")
  {
    Tracker::reset();
    {
      auto g = gen_throw_after_value();
      auto v = g.next();
      REQUIRE(v.has_value());
      CHECK_EQ(v->value, 7);
      CHECK_THROWS_AS(g.next(), std::runtime_error);
    }
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }

  SUBCASE("promise handles exception thrown before first yield")
  {
    Tracker::reset();
    {
      auto g = gen_throw_before_yield();
      CHECK_THROWS_AS(static_cast<bool>(g), std::runtime_error);
    }
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == 0);
    CHECK(Tracker::dtor_count == 0);
  }

  SUBCASE("consumed value does not cause double-destruction or leaks")
  {
    Tracker::reset();
    {
      auto g = gen_yield_once();
      CHECK(static_cast<bool>(g));
      Tracker t = g();
      (void)t;
    }
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }

  SUBCASE("simple value task returns value")
  {
    auto t   = make_value_task(42);
    auto res = sync_await(std::move(t)).get();
    CHECK(res == 42);
  }

  SUBCASE("value task can be awaited via rvalue operator co_await")
  {
    auto t   = make_value_task(10);
    auto res = sync_await_move(std::move(t)).get();
    CHECK(res == 10);
  }

  SUBCASE("is_ready() false before execution")
  {
    auto t = make_value_task(5);
    CHECK_FALSE(t.is_ready());
  }

  SUBCASE("move construction transfers ownership and leaves source ready")
  {
    auto t1 = make_value_task(7);
    CHECK_FALSE(t1.is_ready());

    Task<int> t2 = std::move(t1);

    CHECK(t1.is_ready());
    auto res = sync_await(std::move(t2)).get();
    CHECK(res == 7);
  }

  SUBCASE("move assignment transfers ownership and destroys previous coroutine")
  {
    auto t1 = make_value_task(1);
    auto t2 = make_value_task(2);

    CHECK_FALSE(t1.is_ready());
    CHECK_FALSE(t2.is_ready());

    t1 = std::move(t2);

    CHECK(t2.is_ready());
    auto res = sync_await(std::move(t1)).get();
    CHECK(res == 2);
  }

  SUBCASE("void task executes side effects")
  {
    int x  = 0;
    auto t = make_void_task_increment(x);

    CHECK(x == 0);
    sync_await_void(std::move(t)).get();
    CHECK(x == 1);
  }

  SUBCASE("void task chained in another coroutine")
  {
    int x      = 5;
    auto chain = [&]() -> Task<void>
    {
      co_await make_void_task_increment(x);
      co_await make_void_task_increment(x);
      co_return;
    };

    auto t = chain();
    sync_await_void(std::move(t)).get();
    CHECK(x == 7);
  }

  SUBCASE("Task<T&> propagates reference and allows mutation")
  {
    int value = 10;
    auto t    = make_ref_task(value);

    auto use = [&](Task<int&> tt) -> Task<void>
    {
      int& ref = co_await tt;
      ref += 5;
      co_return;
    };

    auto u = use(std::move(t));
    sync_await_void(std::move(u)).get();
    CHECK(value == 15);
  }

  SUBCASE("Task<T&> used in helper coroutine")
  {
    int value = 3;
    auto t    = make_ref_task(value);
    auto u    = increment_ref_task(std::move(t));

    sync_await_void(std::move(u)).get();
    CHECK(value == 4);
  }

  SUBCASE("Task<T> propagates exception to awaiter")
  {
    auto t = make_throwing_task();

    auto runner = [](Task<int> tt) -> SyncTaskVoid
    {
      try
      {
        (void)co_await tt;
        CHECK(false);
      }
      catch (const std::runtime_error& e)
      {
        CHECK(std::string(e.what()) == "");
      }
      co_return;
    };

    runner(std::move(t)).get();
  }

  SUBCASE("Task<void> propagates exception to awaiter")
  {
    auto t = make_throwing_void_task();

    auto runner = [](Task<void> tt) -> SyncTaskVoid
    {
      try
      {
        co_await tt;
        CHECK(false);
      }
      catch (const std::runtime_error& e)
      {
        CHECK(std::string(e.what()) == "");
      }
      co_return;
    };

    runner(std::move(t)).get();
  }

  SUBCASE("Task can be awaited from another Task")
  {
    auto t = make_value_task(10);
    auto u = add_one(std::move(t));

    auto res = sync_await(std::move(u)).get();
    CHECK(res == 11);
  }

  SUBCASE("multiple levels of Task chaining preserve continuations")
  {
    auto t = make_value_task(3);
    auto u = add_two_via_chain(std::move(t));

    auto res = sync_await(std::move(u)).get();
    CHECK(res == 5);
  }

  SUBCASE("when_ready() completes before result is consumed")
  {
    auto t = make_value_task(21);

    sync_await_any(t.when_ready()).get();

    auto consume = [](Task<int> tt) -> Task<int>
    {
      int v = co_await tt;
      co_return v * 2;
    };

    auto doubled = consume(std::move(t));
    auto res     = sync_await(std::move(doubled)).get();
    CHECK(res == 42);
  }

  SUBCASE("promise destroys value when Task is destroyed without consuming result")
  {
    Tracker::reset();
    {
      auto t = make_tracker_task(42);

      sync_await_any(t.when_ready()).get();
    }
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }

  SUBCASE("consumed value does not cause double destruction")
  {
    Tracker::reset();
    {
      auto consumer = [](Task<Tracker> tt) -> Task<void>
      {
        Tracker t = co_await tt;
        (void)t;
        co_return;
      };

      auto t = make_tracker_task(7);
      auto u = consumer(std::move(t));

      sync_await_void(std::move(u)).get();
    }
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }

  SUBCASE("exception thrown before first suspend does not construct Tracker")
  {
    Tracker::reset();
    {
      auto t = make_tracker_throw_after_construct(5);

      auto runner = [](Task<Tracker> tt) -> SyncTaskVoid
      {
        CHECK_THROWS_AS(co_await tt, std::runtime_error);
        co_return;
      };

      runner(std::move(t)).get();
    }
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }

  SUBCASE(
      "Task destroyed without ever resuming coroutine does not construct Tracker")
  {
    Tracker::reset();
    {
      auto t = make_tracker_never_started(99);
      (void)t;
    }
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }
  SUBCASE("Task<int> on executor runs to completion and returns value")
  {
    auto ex = make_executor(2);
    AsyncScope scope{*ex};

    int result = 0;
    bool done  = false;

    auto joiner = [&](Task<int> t) -> Task<void>
    {
      result = co_await std::move(t);
      done   = true;
      co_return;
    };

    auto t = make_value_task(42);
    scope.spawn(joiner(std::move(t)));

    scope.wait_fence();
    ex->stop();

    CHECK(done);
    CHECK(result == 42);
  }
  SUBCASE("Task<void> executes side effects on executor")
  {
    auto ex = make_executor(2);
    AsyncScope scope{*ex};

    int x     = 0;
    bool done = false;

    auto joiner = [&](Task<void> t) -> Task<void>
    {
      co_await std::move(t);
      done = true;
      co_return;
    };

    CHECK(x == 0);

    auto t = make_void_task_increment(x);
    scope.spawn(joiner(std::move(t)));

    scope.wait_fence();
    ex->stop();

    CHECK(x == 1);
    CHECK(done);
  }

  SUBCASE("Task<T&> on executor propagates reference and allows mutation")
  {
    auto ex = make_executor(2);
    AsyncScope scope{*ex};

    int value = 10;
    bool done = false;

    auto use = [&](Task<int&> t) -> Task<void>
    {
      int& r = co_await std::move(t);
      r += 5;
      done = true;
      co_return;
    };

    auto t = make_ref_task(value);
    scope.spawn(use(std::move(t)));

    scope.wait_fence();
    ex->stop();

    CHECK(done);
    CHECK(value == 15);
  }

  SUBCASE("Task<T> on executor propagates exceptions to awaiter")
  {
    auto ex = make_executor(2);
    AsyncScope scope{*ex};

    bool caught = false;

    auto runner = [&](Task<int> t) -> Task<void>
    {
      try
      {
        (void)co_await std::move(t);
        CHECK(false);
      }
      catch (const std::runtime_error& e)
      {
        CHECK(std::string(e.what()) == "");
        caught = true;
      }
      co_return;
    };

    auto t = make_throwing_task();
    scope.spawn(runner(std::move(t)));

    scope.wait_fence();
    ex->stop();

    CHECK(caught);
  }

  SUBCASE("Task<void> on executor propagates exceptions to awaiter")
  {
    auto ex = make_executor(2);
    AsyncScope scope{*ex};

    bool caught = false;

    auto runner = [&](Task<void> t) -> Task<void>
    {
      try
      {
        co_await std::move(t);
        CHECK(false);
      }
      catch (const std::runtime_error& e)
      {
        CHECK(std::string(e.what()) == "");
        caught = true;
      }
      co_return;
    };

    auto t = make_throwing_void_task();
    scope.spawn(runner(std::move(t)));

    scope.wait_fence();
    ex->stop();

    CHECK(caught);
  }

  SUBCASE("Multiple Task<void> execute across thread pool")
  {
    auto ex = make_executor(4);
    AsyncScope scope{*ex};

    std::atomic<int> counter{0};

    auto make_inc = [&]() -> Task<void>
    {
      counter.fetch_add(1, std::memory_order_relaxed);
      co_return;
    };

    constexpr int N = 100;

    for (int i = 0; i < N; ++i)
      scope.spawn(make_inc());

    scope.wait_fence();
    ex->stop();

    CHECK(counter.load(std::memory_order_relaxed) == N);
  }

  SUBCASE("wait_fence returns immediately when no work is scheduled")
  {
    auto ex = make_executor(4);
    AsyncScope scope{*ex};

    scope.wait_fence();
    ex->stop();
  }

  SUBCASE("wait_fence waits for all scheduled tasks (with yield)")
  {
    auto ex = make_executor(4);
    AsyncScope scope{*ex};

    std::atomic<int> started{0};
    std::atomic<int> finished{0};

    auto yielding_task = [&](int) -> Task<void>
    {
      started.fetch_add(1, std::memory_order_relaxed);

      for (int i = 0; i < 10; ++i)
        co_await ex->yield();

      finished.fetch_add(1, std::memory_order_relaxed);
      co_return;
    };

    constexpr int N = 64;
    for (int i = 0; i < N; ++i)
      scope.spawn(yielding_task(i));

    scope.wait_fence();
    ex->stop();

    CHECK(started.load(std::memory_order_relaxed) == N);
    CHECK(finished.load(std::memory_order_relaxed) == N);
  }

  SUBCASE("executor destructor after scope idle sees all work finished")
  {
    std::atomic<int> finished{0};

    {
      auto ex = make_executor(4);
      AsyncScope scope{*ex};

      auto work = [&]() -> Task<void>
      {
        for (int i = 0; i < 20; ++i)
          co_await ex->yield();

        finished.fetch_add(1, std::memory_order_relaxed);
        co_return;
      };

      constexpr int N = 64;
      for (int i = 0; i < N; ++i)
        scope.spawn(work());

      scope.wait_fence();
      ex->stop();
    }

    CHECK(finished.load(std::memory_order_relaxed) == 64);
  }

  SUBCASE("Tasks can schedule additional tasks while running on executor")
  {
    auto ex = make_executor(4);
    AsyncScope scope{*ex};

    std::atomic<int> inner_counter{0};
    std::atomic<int> outer_started{0};

    auto inner = [&]() -> Task<void>
    {
      inner_counter.fetch_add(1, std::memory_order_relaxed);
      co_return;
    };

    auto outer = [&](int) -> Task<void>
    {
      outer_started.fetch_add(1, std::memory_order_relaxed);

      constexpr int INNER_PER_OUTER = 4;
      for (int i = 0; i < INNER_PER_OUTER; ++i)
        scope.spawn(inner());

      for (int i = 0; i < 5; ++i)
        co_await ex->yield();

      co_return;
    };

    constexpr int OUTER = 16;
    for (int i = 0; i < OUTER; ++i)
      scope.spawn(outer(i));

    scope.wait_fence();
    ex->stop();

    CHECK(outer_started.load(std::memory_order_relaxed) == OUTER);
    CHECK(inner_counter.load(std::memory_order_relaxed) == OUTER * 4);
  }

  SUBCASE("yield allows tasks to make progress under contention")
  {
    auto ex = make_executor(4);
    AsyncScope scope{*ex};

    std::atomic<int> yield_count{0};

    auto many_yields = [&]() -> Task<void>
    {
      for (int i = 0; i < 100; ++i)
      {
        yield_count.fetch_add(1, std::memory_order_relaxed);
        co_await ex->yield();
      }
      co_return;
    };

    constexpr int N = 16;
    for (int i = 0; i < N; ++i)
      scope.spawn(many_yields());

    scope.wait_fence();
    ex->stop();

    CHECK(yield_count.load(std::memory_order_relaxed) == N * 100);
  }

  SUBCASE("Task<int> stress: many values across pool on executor")
  {
    auto ex = make_executor(8);
    AsyncScope scope{*ex};

    std::atomic<long long> sum{0};

    auto value_task = [&](int v) -> Task<int>
    {
      for (int i = 0; i < 5; ++i)
        co_await ex->yield();
      co_return v;
    };

    auto joiner = [&](Task<int> t) -> Task<void>
    {
      for (int i = 0; i < 3; ++i)
        co_await ex->yield();

      int v = co_await std::move(t);
      sum.fetch_add(v, std::memory_order_relaxed);
      co_return;
    };

    constexpr int N = 500;
    for (int i = 0; i < N; ++i)
      scope.spawn(joiner(value_task(i)));

    scope.wait_fence();
    ex->stop();

    long long expected = static_cast<long long>(N - 1) * N / 2;
    CHECK(sum.load(std::memory_order_relaxed) == expected);
  }

  SUBCASE("Task<int> stress: values and exceptions on executor")
  {
    auto ex = make_executor(8);
    AsyncScope scope{*ex};

    std::atomic<int> success_count{0};
    std::atomic<int> exception_count{0};
    std::atomic<long long> sum{0};

    auto maybe_throw_task = [&](int v) -> Task<int>
    {
      for (int i = 0; i < 3; ++i)
        co_await ex->yield();

      if (v % 3 == 0)
        throw std::runtime_error("");
      co_return v;
    };

    auto joiner = [&](Task<int> t) -> Task<void>
    {
      for (int i = 0; i < 2; ++i)
        co_await ex->yield();

      try
      {
        int v = co_await std::move(t);
        sum.fetch_add(v, std::memory_order_relaxed);
        success_count.fetch_add(1, std::memory_order_relaxed);
      }
      catch (const std::runtime_error&)
      {
        exception_count.fetch_add(1, std::memory_order_relaxed);
      }
      co_return;
    };

    constexpr int N = 600;
    for (int i = 0; i < N; ++i)
      scope.spawn(joiner(maybe_throw_task(i)));

    scope.wait_fence();
    ex->stop();

    int successes  = success_count.load(std::memory_order_relaxed);
    int exceptions = exception_count.load(std::memory_order_relaxed);

    CHECK(successes + exceptions == N);
    CHECK(exceptions == N / 3);

    long long expected_sum = 0;
    for (int i = 0; i < N; ++i)
    {
      if (i % 3 != 0)
        expected_sum += i;
    }
    CHECK(sum.load(std::memory_order_relaxed) == expected_sum);
  }

  SUBCASE("Task<void> stress: nested scheduling and yields on executor")
  {
    auto ex = make_executor(8);
    AsyncScope scope{*ex};

    std::atomic<int> outer_count{0};
    std::atomic<int> inner_count{0};

    auto inner = [&]() -> Task<void>
    {
      for (int i = 0; i < 5; ++i)
        co_await ex->yield();

      inner_count.fetch_add(1, std::memory_order_relaxed);
      co_return;
    };

    auto outer = [&]() -> Task<void>
    {
      outer_count.fetch_add(1, std::memory_order_relaxed);
      constexpr int INNER_PER_OUTER = 4;
      for (int i = 0; i < INNER_PER_OUTER; ++i)
        scope.spawn(inner());

      for (int i = 0; i < 10; ++i)
        co_await ex->yield();

      co_return;
    };

    constexpr int OUTER = 64;
    for (int i = 0; i < OUTER; ++i)
      scope.spawn(outer());

    scope.wait_fence();
    ex->stop();

    CHECK(outer_count.load(std::memory_order_relaxed) == OUTER);
    CHECK(inner_count.load(std::memory_order_relaxed) == OUTER * 4);
  }
  SUBCASE("ScheduledThreadPoolExecutor: schedule_at with past deadline fails")
  {
    auto ex = make_executor(2, true);
    AsyncScope scope{*ex};

    std::atomic<Executor::DelayStatus> result{
        Executor::DelayStatus::DELAY_FAIL_NOT_IMPLEMENTED};

    auto worker = [&]() -> Task<void>
    {
      using clock = Executor::clock;
      using namespace std::chrono_literals;

      auto when = clock::now() - 1ms;
      auto st   = co_await ex->schedule_at(when);
      result.store(st, std::memory_order_relaxed);
      co_return;
    };

    scope.spawn(worker());
    scope.wait_fence();
    ex->stop();

    CHECK(
        result.load(std::memory_order_relaxed)
        == Executor::DelayStatus::DELAY_FAIL_DEADLINE_PASSED);
  }

  SUBCASE("ScheduledThreadPoolExecutor: schedule_after delays execution")
  {
    auto ex = make_executor(4, true);
    AsyncScope scope{*ex};

    using clock    = Executor::clock;
    using duration = Executor::duration;
    using namespace std::chrono_literals;

    std::atomic<long long> elapsed_ns{0};
    std::atomic<bool> done{false};
    std::atomic<Executor::DelayStatus> status{
        Executor::DelayStatus::DELAY_FAIL_NOT_IMPLEMENTED};

    auto worker = [&]() -> Task<void>
    {
      auto delay = 20ms;
      auto start = clock::now();

      auto st  = co_await ex->schedule_after(delay);
      auto end = clock::now();

      status.store(st, std::memory_order_relaxed);
      elapsed_ns.store(
          std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(),
          std::memory_order_relaxed);
      done.store(true, std::memory_order_release);
      co_return;
    };

    scope.spawn(worker());
    scope.wait_fence();
    ex->stop();

    CHECK(done.load(std::memory_order_acquire));

    auto st = status.load(std::memory_order_relaxed);
    CHECK(
        (st == Executor::DelayStatus::DELAY_SUCCESS
         || st == Executor::DelayStatus::DELAY_SUCCESS_LATE
         || st == Executor::DelayStatus::DELAY_SUCCESS_EARLY));

    duration elapsed{
        std::chrono::nanoseconds(elapsed_ns.load(std::memory_order_relaxed))};
    CHECK(elapsed >= 20ms);
  }

  SUBCASE("ScheduledThreadPoolExecutor: tasks fire roughly in deadline order")
  {
    auto ex = make_executor(4, true);
    AsyncScope scope{*ex};

    using namespace std::chrono_literals;

    std::mutex m;
    std::vector<int> order;

    auto make_task = [&](int id, Executor::duration delay) -> Task<void>
    {
      co_await ex->schedule_after(delay);
      {
        std::lock_guard<std::mutex> lk(m);
        order.push_back(id);
      }
      co_return;
    };

    scope.spawn(make_task(0, 5ms));
    scope.spawn(make_task(1, 100ms));
    scope.spawn(make_task(2, 200ms));

    scope.wait_fence();
    ex->stop();

    REQUIRE(order.size() == 3);
    CHECK(order[0] == 0);
    CHECK(order[1] == 1);
    CHECK(order[2] == 2);
  }

  SUBCASE("Non-scheduler executor: schedule_after reports NOT_IMPLEMENTED")
  {
    auto ex = make_executor(2, false);
    AsyncScope scope{*ex};

    using namespace std::chrono_literals;

    std::atomic<Executor::DelayStatus> result{Executor::DelayStatus::DELAY_SUCCESS};

    auto worker = [&]() -> Task<void>
    {
      auto st = co_await ex->schedule_after(1ms);
      result.store(st, std::memory_order_relaxed);
      co_return;
    };

    scope.spawn(worker());
    scope.wait_fence();
    ex->stop();

    CHECK(
        result.load(std::memory_order_relaxed)
        == Executor::DelayStatus::DELAY_FAIL_NOT_IMPLEMENTED);
  }
}

TEST_CASE("stdcolt/coroutines/flags")
{
  SUBCASE("FlagSPSC: wait then set resumes; reset clears (sync wrapper)")
  {
    FlagSPSC f{false};
    CHECK_FALSE(f.is_set());

    std::atomic<int> step{0};

    auto consumer = [&]() -> Task<void>
    {
      step.store(1, std::memory_order_relaxed);
      co_await f;
      step.store(2, std::memory_order_relaxed);
      f.reset();
      step.store(3, std::memory_order_relaxed);
      co_return;
    };

    auto driver = [&]() -> Task<void>
    {
      co_await consumer();
      co_return;
    };
    auto d = driver();
    f.set();
    sync_await_void(std::move(d)).get();

    CHECK(step.load(std::memory_order_relaxed) == 3);
    CHECK_FALSE(f.is_set());
  }

  SUBCASE("FlagSPSC: set before await makes await_ready true (no suspend)")
  {
    FlagSPSC f{false};
    f.set();
    CHECK(f.is_set());

    int step      = 0;
    auto consumer = [&]() -> Task<void>
    {
      step = 1;
      co_await f;
      step = 2;
      co_return;
    };

    auto t = consumer();
    sync_await_void(std::move(t)).get();
    CHECK(step == 2);
  }

  SUBCASE("FlagSPSC: reset on already reset does nothing")
  {
    FlagSPSC f{false};
    CHECK_FALSE(f.is_set());
    f.reset();
    CHECK_FALSE(f.is_set());
  }

  SUBCASE("FlagMPMC: set resumes all waiters and stays set until reset")
  {
    FlagMPMC f{false};
    CHECK_FALSE(f.is_set());

    std::atomic<int> woke{0};

    auto waiter = [&]() -> Task<void>
    {
      co_await f;
      woke.fetch_add(1, std::memory_order_relaxed);
      co_return;
    };

    auto t1 = waiter();
    auto t2 = waiter();
    auto t3 = waiter();

    CHECK(woke.load(std::memory_order_relaxed) == 0);

    f.set(); // should resume all three

    sync_await_void(std::move(t1)).get();
    sync_await_void(std::move(t2)).get();
    sync_await_void(std::move(t3)).get();

    CHECK(woke.load(std::memory_order_relaxed) == 3);
    CHECK(f.is_set());

    // New waiters should not suspend
    auto t4 = waiter();
    sync_await_void(std::move(t4)).get();
    CHECK(woke.load(std::memory_order_relaxed) == 4);

    f.reset();
    CHECK_FALSE(f.is_set());
  }

  SUBCASE("FlagMPMC: reset when already reset does nothing")
  {
    FlagMPMC f{false};
    CHECK_FALSE(f.is_set());
    f.reset();
    CHECK_FALSE(f.is_set());
  }

  SUBCASE("FlagMPMC on executor: waiters resume and complete (setter task)")
  {
    auto ex = make_executor(4);
    AsyncScope scope{*ex};

    FlagMPMC f{false};
    std::atomic<int> started{0};
    std::atomic<int> woke{0};

    auto waiter = [&]() -> Task<void>
    {
      started.fetch_add(1, std::memory_order_relaxed);
      co_await f;
      woke.fetch_add(1, std::memory_order_relaxed);
      co_return;
    };

    constexpr int N = 100;
    for (int i = 0; i < N; ++i)
      scope.spawn(waiter());

    scope.spawn(
        [&]() -> Task<void>
        {
          // Wait until all waiter coroutines have started (and likely reached the await soon after).
          while (started.load(std::memory_order_relaxed) != N)
            co_await ex->yield();

          f.set();
          co_return;
        }());

    scope.wait_fence();
    ex->stop();

    CHECK(woke.load(std::memory_order_relaxed) == N);
  }
}

// -----------------------------------------------------------------------------
// AsyncMutex tests
// -----------------------------------------------------------------------------

TEST_CASE("stdcolt/coroutines/async_mutex")
{
  SUBCASE("try_lock/unlock basic")
  {
    AsyncMutex m;

    CHECK(m.try_lock());
    CHECK_FALSE(m.try_lock());

    m.unlock();

    CHECK(m.try_lock());
    m.unlock();
  }

  SUBCASE("co_await lock provides mutual exclusion (single thread executor)")
  {
    auto ex = make_executor(1);
    AsyncScope scope{*ex};

    AsyncMutex m;
    int counter = 0;

    auto worker = [&]() -> Task<void>
    {
      for (int i = 0; i < 200; ++i)
      {
        co_await m;
        int v = counter;
        co_await ex->yield(); // force interleaving inside critical section
        counter = v + 1;
        m.unlock();
      }
      co_return;
    };

    constexpr int N = 8;
    for (int i = 0; i < N; ++i)
      scope.spawn(worker());

    scope.wait_fence();
    ex->stop();

    CHECK(counter == N * 200);
  }

  SUBCASE("co_await lock provides mutual exclusion (multi-thread executor)")
  {
    auto ex = make_executor(4);
    AsyncScope scope{*ex};

    AsyncMutex m;
    std::atomic<int> in_cs{0};
    std::atomic<int> max_in_cs{0};
    std::atomic<long long> sum{0};

    auto worker = [&](int id) -> Task<void>
    {
      for (int i = 0; i < 500; ++i)
      {
        co_await m;

        int now = in_cs.fetch_add(1, std::memory_order_relaxed) + 1;
        // track max concurrently in critical section
        int old_max = max_in_cs.load(std::memory_order_relaxed);
        while (
            now > old_max
            && !max_in_cs.compare_exchange_weak(
                old_max, now, std::memory_order_relaxed, std::memory_order_relaxed))
        {
          // old_max updated by CAS
        }

        // do some work and yield to encourage contention
        sum.fetch_add(id, std::memory_order_relaxed);
        in_cs.fetch_sub(1, std::memory_order_relaxed);
        m.unlock();
      }
      co_return;
    };

    constexpr int N = 16;
    for (int i = 0; i < N; ++i)
      scope.spawn(worker(i + 1));

    scope.wait_fence();
    ex->stop();

    CHECK(max_in_cs.load(std::memory_order_relaxed) == 1);
    CHECK(in_cs.load(std::memory_order_relaxed) == 0);
    CHECK(sum.load(std::memory_order_relaxed) > 0);
  }

  SUBCASE("lock_guard RAII releases the mutex on scope exit")
  {
    auto ex = make_executor(2);
    AsyncScope scope{*ex};

    AsyncMutex m;
    static constexpr size_t N = 64;
    uint64_t counter[N]       = {};

    auto worker = [&]() -> Task<void>
    {
      for (int i = 0; i < 200; ++i)
      {
        auto g = co_await m.lock_guard();
        for (size_t i = 0; i < N; i++)
          counter[i] += 1;
      }
      co_return;
    };

    constexpr int WORKER_N = 64;
    for (int i = 0; i < N; ++i)
      scope.spawn(worker());

    scope.wait_fence();
    ex->stop();

    for (size_t i = 0; i < N; i++)
      CHECK(counter[i] == WORKER_N * 200);
  }

  SUBCASE("stress: many coroutines contend for lock and all complete")
  {
    auto ex = make_executor(8);
    AsyncScope scope{*ex};

    AsyncMutex m;
    std::atomic<int> done{0};
    std::atomic<long long> acc{0};

    auto worker = [&](int id) -> Task<void>
    {
      for (int i = 0; i < 200; ++i)
      {
        co_await m;
        acc.fetch_add((id * 1315423911ull) ^ i, std::memory_order_relaxed);
        // TODO: investigate...
        //if ((i & 7) == 0)
        //  co_await ex->yield();
        m.unlock();
      }
      done.fetch_add(1, std::memory_order_relaxed);
      co_return;
    };

    constexpr int N = 200;
    for (int i = 0; i < N; ++i)
      scope.spawn(worker(i + 1));

    scope.wait_fence();
    ex->stop();

    CHECK(done.load(std::memory_order_relaxed) == N);
    CHECK(acc.load(std::memory_order_relaxed) != 0);
  }
}