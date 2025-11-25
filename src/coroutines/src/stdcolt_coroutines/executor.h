/*****************************************************************/ /**
 * @file   executor.h
 * @brief  Contains `Executor` and `AsyncScope`.
 * Use `make_executor` to create a multithreaded executor.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_COROUTINES_EXECUTOR
#define __HG_STDCOLT_COROUTINES_EXECUTOR

#include <concurrentqueue.h>
#include <coroutine>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <utility>
#include <type_traits>
#include <stdcolt_contracts/contracts.h>
#include <stdcolt_coroutines/task.h>
#include <stdcolt_coroutines/atomic_coroutine_handle.h>
#include <stdcolt_coroutines_export.h>
#include <memory>

namespace stdcolt::coroutines
{
  /// @brief Base class for all executors
  class Executor
  {
  public:
    /// @brief The handle type (coroutine handle)
    using handle = std::coroutine_handle<>;
    /// @brief The clock used by executors
    using clock = std::chrono::steady_clock;
    /// @brief The time point used for scheduling
    using time_point = clock::time_point;
    /// @brief The duration used for scheduling
    using duration = clock::duration;
    /// @brief Destructor
    virtual ~Executor() = default;

    /// @brief The error code when calling `post`
    enum class PostStatus : uint8_t
    {
      /// @brief Successfully posted coroutine
      POST_SUCCESS = 0,
      /// @brief Failed due to missing memory
      POST_FAIL_MEMORY = 1,
      /// @brief Failed as executor is (being) stopped
      POST_FAIL_STOPPED = 2,
      /// @brief Failed as executor does not provide this feature
      POST_FAIL_NOT_IMPLEMENTED = 3,
      /// @brief Failed as requested time point is in the past.
      /// This may only be returned by the `post` overload that takes a time point.
      POST_FAIL_DEADLINE_PASSED = 4,
    };

    /// @brief The result of awaiting
    enum class DelayStatus : uint8_t
    {
      /// @brief Success, coroutine woke up at (approximately) the requested time
      DELAY_SUCCESS = 0,
      /// @brief Success, coroutine woke up, but noticeably later than requested
      DELAY_SUCCESS_LATE = 1,
      /// @brief Success, coroutine woke up, but noticeably earlier than requested
      DELAY_SUCCESS_EARLY = 2,

      /// @brief The requested time point was already in the past
      DELAY_FAIL_DEADLINE_PASSED = 3,
      /// @brief The underlying post failed, out of memory
      DELAY_FAIL_MEMORY = 4,
      /// @brief The underlying post failed, executor is stopped/stopping
      DELAY_FAIL_STOPPED = 5,
      /// @brief The executor does not support scheduling in the future
      DELAY_FAIL_NOT_IMPLEMENTED = 6,
    };

    /// @brief Converts a `PostStatus` representing an error to a `DelayStatus` representing an error
    /// @param ps The `PostStatus` (!= `POST_SUCCESS`)
    /// @return DelayStatus equivalent
    static constexpr DelayStatus to_delay_failure(PostStatus ps) noexcept
    {
      switch_no_default(ps)
      {
      case PostStatus::POST_FAIL_DEADLINE_PASSED:
        return DelayStatus::DELAY_FAIL_DEADLINE_PASSED;
      case PostStatus::POST_FAIL_MEMORY:
        return DelayStatus::DELAY_FAIL_MEMORY;
      case PostStatus::POST_FAIL_STOPPED:
        return DelayStatus::DELAY_FAIL_STOPPED;
      case PostStatus::POST_FAIL_NOT_IMPLEMENTED:
        return DelayStatus::DELAY_FAIL_NOT_IMPLEMENTED;
      }
    }

    /// @brief Posts a coroutine to be executed immediately.
    /// @param h The coroutine handle
    /// @return True on success, false on failure
    virtual PostStatus post(handle h) noexcept = 0;
    /// @brief Posts a coroutine to be executed at a specific time point.
    /// @param h The coroutine handle
    /// @param when The time point when to execute the coroutine
    /// @return True on success, false on failure
    virtual PostStatus post(handle h, time_point when) noexcept = 0;
    /// @brief Stops the executor, dropping any work remaining
    /// This method is thread safe and idempotent.
    /// @warning This function must never be called on one of the worker threads.
    virtual void stop() noexcept = 0;

    /// @brief Awaiter, schedules a coroutine to run through this executor
    /// @note This is exactly the same as `yield`
    /// @return Awaiter
    auto schedule() noexcept
    {
      struct awaiter
      {
        Executor* ex;
        PostStatus status{};
        bool await_ready() const noexcept { return false; }
        void await_suspend(handle h) noexcept { status = ex->post(h); }
        PostStatus await_resume() const noexcept { return status; }
      };
      return awaiter{this};
    }
    /// @brief Awaiter, schedules a coroutine to run through this executor.
    /// @note This is exactly the same as `schedule`
    /// @return Awaiter
    auto yield() noexcept { return schedule(); }

    /// @brief Schedule a coroutine to resume at a specific time point
    /// @param when When to resume the coroutine
    /// @param tolerance The tolerance (for EARLY/LATE successes)
    /// @return Awaitable that yields a DelayStatus
    auto schedule_at(time_point when, duration tolerance = duration::zero()) noexcept
    {
      tolerance = tolerance < duration::zero() ? -tolerance : tolerance;

      struct awaiter
      {
        Executor* ex;
        time_point when;
        duration tolerance;
        PostStatus post_status = PostStatus::POST_SUCCESS;
        time_point expected{};

        bool await_ready() noexcept
        {
          expected = when;
          if (auto now = clock::now(); now >= when)
          {
            post_status = PostStatus::POST_FAIL_DEADLINE_PASSED;
            return true;
          }
          return false;
        }

        bool await_suspend(handle h) noexcept
        {
          if (post_status == PostStatus::POST_SUCCESS)
            post_status = ex->post(h, when);
          return post_status == PostStatus::POST_SUCCESS;
        }

        DelayStatus await_resume() const noexcept
        {
          using enum DelayStatus;
          if (post_status != PostStatus::POST_SUCCESS)
            return to_delay_failure(post_status);

          // post() succeeded, we actually resumed
          if (tolerance > duration::zero())
          {
            auto now  = clock::now();
            auto diff = now - expected;
            if (diff > tolerance)
              return DELAY_SUCCESS_LATE;
            if (diff < -tolerance)
              return DELAY_SUCCESS_EARLY;
            return DELAY_SUCCESS;
          }
          // if there is no tolerance, then always return success
          return DELAY_SUCCESS;
        }
      };
      return awaiter{this, when, tolerance};
    }

    /// @brief Schedule a coroutine to resume after a relative duration
    /// @param d Delay duration
    /// @param tolerance Allowed deviation before classifying as EARLY/LATE
    /// @return Awaiter that yields a DelayStatus
    auto schedule_after(duration d, duration tolerance = duration::zero()) noexcept
    {
      return schedule_at(clock::now() + d, tolerance);
    }
  };

  /// @brief Creates a multithreaded executor.
  /// For less overhead, if an executor is needed only for its `post(handle)`
  /// overload, not `post(handle, time_point)`, set `with_scheduler` to false.
  /// If `with_scheduler` is false, any call that schedules in the future
  /// will return a 'not implemented' error code.
  /// @param thread_count The number of threads of the executor
  /// @param with_scheduler If true, add support for scheduling coroutines
  /// @return Valid executor on success
  STDCOLT_COROUTINES_EXPORT
  std::unique_ptr<Executor> make_executor(
      size_t thread_count = std::thread::hardware_concurrency(),
      bool with_scheduler = true) noexcept;

  /// @brief Coroutine owner that schedules task to an executor
  class AsyncScope
  {
  public:
    /// @brief Constructor
    /// @param ex The executor
    explicit AsyncScope(Executor& ex) noexcept
        : _executor(ex)
    {
    }

    AsyncScope(const AsyncScope&)            = delete;
    AsyncScope& operator=(const AsyncScope&) = delete;
    /// @brief Destructor
    /// @pre All task must be done, by using `wait_fence` or any other means
    ~AsyncScope()
    {
      STDCOLT_assert(
          _pending.load(std::memory_order_acquire) == 0,
          "AsyncScope destroyed while operations are still pending");
    }

    /// @brief Submits an awaitable to execute in the executor
    /// @tparam Awaitable The awaitable type
    /// @param aw The awaitable
    template<typename Awaitable>
    void spawn(Awaitable&& aw)
    {
      _pending.fetch_add(1, std::memory_order_relaxed);
      // create a detached coroutine bound to this scope
      auto dt = make_detached(*this, std::forward<Awaitable>(aw));

      // detach: the coroutine will self-destroy in final_suspend().
      // setting handle = nullptr ensures detached_task's destructor does nothing.
      dt.handle = nullptr;
    }

    /// @brief Awaitable to efficiently waits for all the spawned tasks to be done.
    /// For non-coroutine code use `wait_fence`.
    /// @return Awaitable
    auto wait_idle() noexcept
    {
      struct awaiter
      {
        AsyncScope* scope;

        bool await_ready() const noexcept
        {
          // if everything already finished, resume immediately
          return scope->_pending.load(std::memory_order_acquire) == 0;
        }
        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
          scope->_waiter.store(h, std::memory_order_release);

          // if everything already finished, resume immediately
          if (scope->_pending.load(std::memory_order_acquire) == 0)
          {
            auto old = scope->_waiter.exchange(
                std::coroutine_handle<>{}, std::memory_order_acq_rel);
            if (old)
              old.resume();
            return false;
          }
          return true;
        }
        void await_resume() const noexcept {}
      };

      return awaiter{this};
    }

    /// @brief Efficiently waits for all the spawned tasks to be done.
    /// For a coroutine awaitable, use `wait_idle`.
    void wait_fence() const noexcept
    {
      for (;;)
      {
        auto expected = _pending.load(std::memory_order_acquire);
        if (expected == 0)
          break;

        _pending.wait(expected, std::memory_order_acquire);
      }
    }

    /// @brief Returns the executor
    /// @return The executor
    Executor& executor() noexcept { return _executor; }
    /// @brief Returns the executor
    /// @return The executor
    const Executor& executor() const noexcept { return _executor; }

  private:
    /// @brief The thread pool executor
    Executor& _executor;
    /// @brief The number of pending tasks
    std::atomic<size_t> _pending{0};
    /// @brief Coroutine waiting for the scope to no longer have any tasks
    atomic_coroutine_handle<> _waiter{std::coroutine_handle<>{}};

    /// @brief Detached task, the promise type of the parent coroutine
    struct detached_task
    {
      /// @brief Promise type
      struct promise_type
      {
        /// @brief Creates the detach task from the promise
        detached_task get_return_object() noexcept
        {
          return detached_task{
              std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        /// @brief Start executing directly so that the `.schedule()` happens.
        /// This is needed to immediately pass the coroutine to the executor.
        std::suspend_never initial_suspend() const noexcept { return {}; }

        /// @brief Final awaitable, let the coroutine destroy itself
        struct final_awaitable
        {
          bool await_ready() const noexcept { return false; }
          void await_suspend(std::coroutine_handle<> h) const noexcept
          {
            h.destroy();
          }
          void await_resume() const noexcept {}
        };
        /// @brief Destroy the coroutine
        auto final_suspend() const noexcept { return final_awaitable{}; }
        /// @brief Does nothing
        void return_void() const noexcept {}
        /// @brief On unhandled exceptions, terminate
        void unhandled_exception() const noexcept
        {
          // no exception should happen here as make_detached already
          // catches them all, but as a safety, terminate
          std::terminate();
        }
      };
      /// @brief The coroutine handle
      std::coroutine_handle<promise_type> handle{};

      /// @brief Constructor
      detached_task() noexcept = default;
      /// @brief Constructor
      /// @param h The underlying handle
      explicit detached_task(std::coroutine_handle<promise_type> h) noexcept
          : handle(h)
      {
      }
      /// @brief Move constructor
      /// @param other The other
      detached_task(detached_task&& other) noexcept
          : handle(std::exchange(other.handle, nullptr))
      {
      }
      /// @brief Move assignment operator
      /// @param other Other
      /// @return this
      detached_task& operator=(detached_task&& other) noexcept
      {
        if (this != &other)
        {
          if (handle)
            handle.destroy();
          handle = std::exchange(other.handle, nullptr);
        }
        return *this;
      }
      /// @brief Destructor, destroys the handle if it is valid
      ~detached_task()
      {
        if (handle)
          handle.destroy();
      }
      detached_task(const detached_task&)            = delete;
      detached_task& operator=(const detached_task&) = delete;
    };

    /// @brief Creates a detached task from an awaitable.
    /// Any exception thrown by that awaitable are swallowed
    /// @tparam Awaitable The awaitable type
    /// @param scope The scope whose pending task to decrement
    /// @param aw The awaitable to execute
    /// @return Detached task
    template<typename Awaitable>
    static detached_task make_detached(AsyncScope& scope, Awaitable aw)
    {
      // ensures on_task_done() is called exactly once vvv
      struct guard
      {
        AsyncScope* s;
        ~guard()
        {
          if (s)
            s->on_task_done();
        }
      };
      guard g{&scope};

      // move onto executor threads
      co_await scope._executor.schedule();
      try
      {
        co_await std::move(aw);
      }
      catch (...)
      {
        // swallow any exception
      }
      co_return;
    }

    /// @brief Marks a task as done, and notifies waiters (`wait_fence` and `wait_idle`)
    void on_task_done() noexcept
    {
      const auto remaining = _pending.fetch_sub(1, std::memory_order_acq_rel) - 1;

      if (remaining == 0)
      {
        // wake blocking waiters
        _pending.notify_all();
        // wake coroutine waiter
        auto h =
            _waiter.exchange(std::coroutine_handle<>{}, std::memory_order_acq_rel);
        if (h)
          h.resume();
      }
    }
  };

  /// @brief AsyncScope that calls `wait_fence` automatically in the destructor.
  struct BlockingAsyncScope
  {
    /// @brief Constructor
    /// @param ex The executor to use when spawning tasks
    explicit BlockingAsyncScope(Executor& ex)
        : _scope(ex)
    {
    }
    /// @brief Destructor, waits for all tasks to be done
    ~BlockingAsyncScope() { _scope.wait_fence(); }

    /// @brief Returns the executor
    /// @return The executor
    Executor& executor() noexcept { return _scope.executor(); }
    /// @brief Returns the executor
    /// @return The executor
    const Executor& executor() const noexcept { return _scope.executor(); }

    /// @brief Spawns a task
    /// @tparam Awaitable The awaitable
    /// @param aw The awaitable to spawn in the thread pool executor
    template<typename Awaitable>
    void spawn(Awaitable&& aw)
    {
      _scope.spawn(std::forward<Awaitable>(aw));
    }

  private:
    /// @brief Underlying AsyncScope
    AsyncScope _scope;
  };

} // namespace stdcolt::coroutines

#endif // !__HG_STDCOLT_COROUTINES_EXECUTOR
