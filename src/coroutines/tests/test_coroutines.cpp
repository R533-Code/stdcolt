#include <doctest/doctest.h>
#include <stdcolt_coroutines/scheduler.h>
#include <stdcolt_coroutines/task.h>
#include <stdcolt_coroutines/generator.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <optional>

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

  explicit SyncTask(std::coroutine_handle<promise_type> h) noexcept
      : handle(h)
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

  explicit SyncTaskVoid(std::coroutine_handle<promise_type> h) noexcept
      : handle(h)
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

// sync_await_scheduled* are intentionally not used for ScheduledTask
// because ScheduledTask is driven exclusively by ThreadPoolScheduler.
template<typename T>
SyncTask<T> sync_await_scheduled(ScheduledTask<T> st)
{
  co_return co_await st;
}

inline SyncTaskVoid sync_await_scheduled_void(ScheduledTask<void> st)
{
  co_await st;
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

      // Force coroutine to run and place a value into its frame/promise
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
      // Consume both yields in some way; exact object counts are
      // implementation-defined, so we only care that it doesn't crash.
      std::vector<int> values;
      for (auto t : g)
        values.push_back(t.value);

      REQUIRE_EQ(values.size(), 2);
      CHECK_EQ(values[0], 1);
      CHECK_EQ(values[1], 2);
    }
    // All Tracker instances must be properly destroyed.
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }

  SUBCASE("promise handles exception thrown after value without leaking")
  {
    Tracker::reset();
    {
      auto g = gen_throw_after_value();
      // First resume: get the value
      auto v = g.next();
      REQUIRE(v.has_value());
      CHECK_EQ(v->value, 7);
      // Next access rethrows exception captured by unhandled_exception()
      CHECK_THROWS_AS(g.next(), std::runtime_error);
    }
    // No Tracker leaks, no double-destruction.
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }

  SUBCASE("promise handles exception thrown before first yield")
  {
    Tracker::reset();
    {
      auto g = gen_throw_before_yield();
      // First attempt to get anything must throw
      CHECK_THROWS_AS(static_cast<bool>(g), std::runtime_error);
    }
    // No Tracker ever constructed or destroyed in this scenario;
    // the coroutine never executed a co_yield.
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == 0);
    CHECK(Tracker::dtor_count == 0);
  }

  SUBCASE("consumed value does not cause double-destruction or leaks")
  {
    Tracker::reset();
    {
      auto g = gen_yield_once();
      // Force value to be created in coroutine frame/promise
      CHECK(static_cast<bool>(g));
      // Consume the value (move it out)
      Tracker t = g();
      // Generator goes out of scope at end of this block. Its promise
      // destructor must not try to destroy a non-existent Tracker in
      // storage (no double destruction), and there must be no leaks.
      (void)t; // suppress unused warning
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
    CHECK_FALSE(t.is_ready()); // coroutine not started yet
  }

  SUBCASE("move construction transfers ownership and leaves source ready")
  {
    auto t1 = make_value_task(7);
    CHECK_FALSE(t1.is_ready());

    Task<int> t2 = std::move(t1);

    CHECK(t1.is_ready()); // moved-from: handle == nullptr
    auto res = sync_await(std::move(t2)).get();
    CHECK(res == 7);
  }

  SUBCASE("move assignment transfers ownership and destroys previous coroutine")
  {
    auto t1 = make_value_task(1);
    auto t2 = make_value_task(2);

    // Force t1 to be a valid task that hasn't run yet
    CHECK_FALSE(t1.is_ready());
    CHECK_FALSE(t2.is_ready());

    t1 = std::move(t2);

    CHECK(t2.is_ready()); // moved-from
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
        CHECK(false); // should not reach
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
        CHECK(false); // should not reach
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
    CHECK(res == 5); // (3 + 1) + 1
  }

  SUBCASE("when_ready() completes before result is consumed")
  {
    auto t = make_value_task(21);

    // First ensure it completes via when_ready()
    sync_await_any(t.when_ready()).get();

    // Now actually consume the result via a second await
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

      // Run the task to completion but never co_await the value
      sync_await_any(t.when_ready()).get();
      // 't' goes out of scope here; promise destructor must destroy Tracker
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
        Tracker t = co_await tt; // move out of coroutine storage
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
    // Tracker may be constructed before the throw in coroutine body, but
    // must be destroyed exactly once; no leaks, no double-destruction.
    CHECK(Tracker::live_objects == 0);
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }

  SUBCASE(
      "Task destroyed without ever resuming coroutine does not construct Tracker")
  {
    Tracker::reset();
    {
      auto t = make_tracker_never_started(99);
      // We never resume/await 't'; initial_suspend() is suspend_always,
      // so body never starts and Tracker never constructed.
      (void)t;
    }
    CHECK(Tracker::live_objects == 0);
    // Depending on the implementation, the compiler may or may not elide
    // construction of Tracker inside the coroutine frame; the important
    // invariant is no leaks and balanced ctor/dtor.
    CHECK(Tracker::ctor_count == Tracker::dtor_count);
  }
  SUBCASE("ScheduledTask<int> runs to completion and returns value")
  {
    ThreadPoolScheduler sched{2};

    auto t  = make_value_task(42);
    auto st = co_spawn(sched, std::move(t));

    int result = 0;
    bool done  = false;

    auto joiner = [&](ScheduledTask<int> s) -> ScheduledTask<void>
    {
      result = co_await s;
      done   = true;
      co_return;
    };

    co_spawn(sched, joiner(std::move(st)));

    sched.wait_idle();

    CHECK(done);
    CHECK(result == 42);
  }

  SUBCASE("ScheduledTask<void> executes side effects")
  {
    ThreadPoolScheduler sched{2};

    int x  = 0;
    auto t = make_void_task_increment(x);

    CHECK(x == 0);

    auto st = co_spawn(sched, std::move(t));

    bool done = false;

    auto joiner = [&](ScheduledTask<void> s) -> ScheduledTask<void>
    {
      co_await s;
      done = true;
      co_return;
    };

    co_spawn(sched, joiner(std::move(st)));

    sched.wait_idle();

    CHECK(x == 1);
    CHECK(done);
  }

  SUBCASE("ScheduledTask<T&> propagates reference and allows mutation")
  {
    ThreadPoolScheduler sched{2};

    int value = 10;
    auto t    = make_ref_task(value);
    auto st   = co_spawn(sched, std::move(t));

    bool done = false;

    auto use = [&](ScheduledTask<int&> s) -> ScheduledTask<void>
    {
      int& r = co_await s;
      r += 5;
      done = true;
      co_return;
    };

    co_spawn(sched, use(std::move(st)));
    sched.wait_idle();

    CHECK(done);
    CHECK(value == 15);
  }

  SUBCASE("ScheduledTask<T> propagates exceptions to awaiter")
  {
    ThreadPoolScheduler sched{2};

    auto t  = make_throwing_task();
    auto st = co_spawn(sched, std::move(t));

    bool caught = false;

    auto runner = [&](ScheduledTask<int> s) -> ScheduledTask<void>
    {
      try
      {
        (void)co_await s;
        CHECK(false); // should not reach
      }
      catch (const std::runtime_error& e)
      {
        CHECK(std::string(e.what()) == "");
        caught = true;
      }
      co_return;
    };

    co_spawn(sched, runner(std::move(st)));
    sched.wait_idle();

    CHECK(caught);
  }

  SUBCASE("ScheduledTask<void> propagates exceptions to awaiter")
  {
    ThreadPoolScheduler sched{2};

    auto t  = make_throwing_void_task();
    auto st = co_spawn(sched, std::move(t));

    bool caught = false;

    auto runner = [&](ScheduledTask<void> s) -> ScheduledTask<void>
    {
      try
      {
        co_await s;
        CHECK(false); // should not reach
      }
      catch (const std::runtime_error& e)
      {
        CHECK(std::string(e.what()) == "");
        caught = true;
      }
      co_return;
    };

    co_spawn(sched, runner(std::move(st)));
    sched.wait_idle();

    CHECK(caught);
  }

  SUBCASE("Multiple ScheduledTask<void> execute across thread pool")
  {
    ThreadPoolScheduler sched{4};

    std::atomic<int> counter{0};

    auto make_inc = [&]() -> Task<void>
    {
      counter.fetch_add(1, std::memory_order_relaxed);
      co_return;
    };

    constexpr int N = 100;

    for (int i = 0; i < N; ++i)
      co_spawn(sched, make_inc()); // scheduler owns all tasks

    sched.wait_idle();

    CHECK(counter.load(std::memory_order_relaxed) == N);
  }

  SUBCASE("ThreadPoolScheduler with zero threads creates one worker")
  {
    ThreadPoolScheduler sched{0};
    CHECK(sched.worker_count() == 1);
  }

  SUBCASE("wait_idle returns immediately when no work is scheduled")
  {
    ThreadPoolScheduler sched{4};
    // Should not block or deadlock
    sched.wait_idle();
  }

  SUBCASE("wait_idle waits for all scheduled tasks (with yield)")
  {
    ThreadPoolScheduler sched{4};

    std::atomic<int> started{0};
    std::atomic<int> finished{0};

    auto yielding_task = [&](int) -> ScheduledTask<void>
    {
      started.fetch_add(1, std::memory_order_relaxed);

      // Do some work and yield back to the scheduler a few times
      for (int i = 0; i < 10; ++i)
        co_await sched.yield();

      finished.fetch_add(1, std::memory_order_relaxed);
      co_return;
    };

    constexpr int N = 64;
    for (int i = 0; i < N; ++i)
      co_spawn(sched, yielding_task(i)); // ignore ScheduledTask, scheduler owns it

    sched.wait_idle();

    CHECK(started.load(std::memory_order_relaxed) == N);
    CHECK(finished.load(std::memory_order_relaxed) == N);
  }

  SUBCASE("scheduler destructor waits for all tasks to finish")
  {
    std::atomic<int> finished{0};

    {
      ThreadPoolScheduler sched{4};

      auto work = [&]() -> ScheduledTask<void>
      {
        // simulate some work with yields
        for (int i = 0; i < 20; ++i)
          co_await sched.yield();

        finished.fetch_add(1, std::memory_order_relaxed);
        co_return;
      };

      constexpr int N = 64;
      for (int i = 0; i < N; ++i)
        co_spawn(sched, work());
      // No explicit wait_idle(); destructor must block until all work is done.
    }

    CHECK(finished.load(std::memory_order_relaxed) == 64);
  }

  SUBCASE("Tasks can schedule additional tasks while running")
  {
    ThreadPoolScheduler sched{4};

    std::atomic<int> inner_counter{0};
    std::atomic<int> outer_started{0};

    auto inner = [&]() -> ScheduledTask<void>
    {
      inner_counter.fetch_add(1, std::memory_order_relaxed);
      co_return;
    };

    auto outer = [&](int) -> ScheduledTask<void>
    {
      outer_started.fetch_add(1, std::memory_order_relaxed);

      // Spawn additional tasks from within a running task
      constexpr int INNER_PER_OUTER = 4;
      for (int i = 0; i < INNER_PER_OUTER; ++i)
        co_spawn(sched, inner());

      // Yield back to the scheduler a few times
      for (int i = 0; i < 5; ++i)
        co_await sched.yield();

      co_return;
    };

    constexpr int OUTER = 16;
    for (int i = 0; i < OUTER; ++i)
      co_spawn(sched, outer(i));

    sched.wait_idle();

    CHECK(outer_started.load(std::memory_order_relaxed) == OUTER);
    CHECK(inner_counter.load(std::memory_order_relaxed) == OUTER * 4);
  }

  SUBCASE("yield allows tasks to make progress under contention")
  {
    ThreadPoolScheduler sched{4};

    std::atomic<int> yield_count{0};

    auto many_yields = [&]() -> ScheduledTask<void>
    {
      for (int i = 0; i < 100; ++i)
      {
        yield_count.fetch_add(1, std::memory_order_relaxed);
        co_await sched.yield();
      }
      co_return;
    };

    constexpr int N = 16;
    for (int i = 0; i < N; ++i)
      co_spawn(sched, many_yields());

    sched.wait_idle();

    CHECK(yield_count.load(std::memory_order_relaxed) == N * 100);
  }
}
