/*****************************************************************/ /**
 * @file   executor.h
 * @brief  Contains `ThreadPoolExecutor` and `AsyncScope`.
 * `ThreadPoolExecutor` is a non-owning coroutine executor.
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
#include <stdcolt_coroutines/task.h>
#include <stdcolt_coroutines/atomic_coroutine_handle.h>
#include <stdcolt_coroutines_export.h>

namespace stdcolt::coroutines
{
  /// @brief Non-owning coroutine executor
  class ThreadPoolExecutor
  {
  public:
    /// @brief The handle type (coroutine handle)
    using handle_t = std::coroutine_handle<>;

    /// @brief Constructor
    /// @param thread_count The number of threads the executor may use, max(1, thread_count)
    explicit ThreadPoolExecutor(
        size_t thread_count = std::thread::hardware_concurrency())
    {
      if (thread_count == 0)
        thread_count = 1;
      // must resize not reserve so that no reallocation may occur
      _workers.resize(thread_count);
      for (size_t i = 0; i < thread_count; ++i)
        _workers[i].thread = std::thread([this, i] { worker_loop(i); });
    }
    /// @brief Destructor, drops the remaining work
    ~ThreadPoolExecutor() { stop(); }

    ThreadPoolExecutor(ThreadPoolExecutor&&)                 = delete;
    ThreadPoolExecutor(const ThreadPoolExecutor&)            = delete;
    ThreadPoolExecutor& operator=(ThreadPoolExecutor&&)      = delete;
    ThreadPoolExecutor& operator=(const ThreadPoolExecutor&) = delete;

    /// @brief Post a coroutine to evaluate in the thread pool
    /// @param h The coroutine to evaluate
    void post(handle_t h) noexcept
    {
      // if we are in a specific worker thread, do not go through
      // the global queue, instead directly enqueue in that thread.
      if (tls_executor_info.executor == this
          && tls_executor_info.worker_id != invalid_index)
        _workers[tls_executor_info.worker_id].queue.enqueue(h);
      else
        _global_queue.enqueue(h);

      _work_epoch.fetch_add(1, std::memory_order_release);
      _work_epoch.notify_one();
      // ^^^ as the pool does work stealing, only a single one is needed.
    }

    /// @brief Awaiter, schedules a coroutine to run through this executor
    /// @note This is exactly the same as `yield`
    /// @return Awaiter
    auto schedule() noexcept
    {
      struct awaiter
      {
        ThreadPoolExecutor* ex;
        bool await_ready() const noexcept { return false; }
        void await_suspend(handle_t h) const noexcept { ex->post(h); }
        void await_resume() const noexcept {}
      };
      return awaiter{this};
    }
    /// @brief Awaiter, schedules a coroutine to run through this executor.
    /// @note This is exactly the same as `schedule`
    /// @return Awaiter
    auto yield() noexcept { return schedule(); }

    /// @brief Stops the executor. Work is not drained.
    /// This method is thread safe and idempotent.
    /// @warning This function must never be called on one of the worker threads.
    void stop() noexcept
    {
      if (_stopping.load(std::memory_order_acquire) == 2)
        return;

      // acquire on failure as if expected == 2 we return directly
      if (int32_t expected = 0; !_stopping.compare_exchange_strong(
              expected, 1, std::memory_order_acq_rel, std::memory_order_acquire))
      {
        // if expected is not 1, then it is 2 and we can avoid calling .wait
        if (expected == 1)
          _stopping.wait(1, std::memory_order_acquire);
        return;
      }
      _work_epoch.fetch_add(1, std::memory_order_release);
      _work_epoch.notify_all();

      // there is only a single thread that can be here vvv
      for (auto& [thread, _] : _workers)
      {
        if (thread.joinable())
          thread.join();
      }
      _stopping.store(2, std::memory_order_release);
      _stopping.notify_all();
      // ^^^ wake up all threads waiting
    }

  private:
    /// @brief Worker, must have a stable address
    struct Worker
    {
      /// @brief The actual worker thread
      std::thread thread;
      /// @brief The local queue.
      /// This queue is concurrent to allow safe work stealing.
      moodycamel::ConcurrentQueue<handle_t> queue;
    };

    /// @brief Work epoch for efficient waiting
    /// It is preferable to use 32 bit integers for a direct lowering to futexes.
    std::atomic<uint32_t> _work_epoch{0};
    /// @brief Array of workers, size constant throughout lifetime
    std::vector<Worker> _workers;
    /// @brief Global queue from which worker may pop work
    moodycamel::ConcurrentQueue<handle_t> _global_queue;
    /// @brief If 0, stop was not requested, if 1, stop requested, if 2 stopped
    /// It is preferable to use 32 bit integers for a direct lowering to futexes.
    std::atomic<int32_t> _stopping{0};

    /// @brief Invalid index to represent an invalid worker ID
    static constexpr size_t invalid_index = (size_t)-1;
    /// @brief Information that describes an executor's worker
    struct ThreadPoolExecutorInfo
    {
      /// @brief The actual executor (has a stable address)
      ThreadPoolExecutor* executor = nullptr;
      /// @brief The worker ID (index into the vector of that executor)
      size_t worker_id = invalid_index;
    };
    /// @brief Thread local executor info for enqueuing directly in the same worker
    static thread_local ThreadPoolExecutorInfo tls_executor_info;

    /// @brief Worker function
    /// @param index The index into the vector of workers
    void worker_loop(size_t index)
    {
      tls_executor_info = {this, index};

      auto& self_queue          = _workers[index].queue;
      const size_t worker_count = _workers.size();

      while (!_stopping.load(std::memory_order_acquire))
      {
        handle_t h;

        // dequeue from the local or on empty local queue, from the global
        if (self_queue.try_dequeue(h) || _global_queue.try_dequeue(h))
        {
          if (!h.done())
            h.resume();
          continue;
        }

        // steal from other workers
        bool stolen = false;
        for (size_t i = 0; i < worker_count; ++i)
        {
          if (i == index)
            continue;
          if (_workers[i].queue.try_dequeue(h))
          {
            stolen = true;
            if (!h.done())
              h.resume();
            break;
          }
        }
        if (stolen)
          continue;

        // if there is no work to be done, sleep efficiently
        auto expected = _work_epoch.load(std::memory_order_acquire);
        if (_stopping.load(std::memory_order_acquire))
          break;
        _work_epoch.wait(expected, std::memory_order_acquire);
      }

      tls_executor_info = {nullptr, invalid_index};
    }
  };

  /// @brief Initialized to empty info
  inline thread_local ThreadPoolExecutor::ThreadPoolExecutorInfo
      ThreadPoolExecutor::tls_executor_info = {};

  /// @brief Coroutine owner that schedules task to an executor
  class AsyncScope
  {
  public:
    /// @brief Constructor
    /// @param ex The executor
    explicit AsyncScope(ThreadPoolExecutor& ex) noexcept
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

  private:
    /// @brief The thread pool executor
    ThreadPoolExecutor& _executor;
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
    explicit BlockingAsyncScope(ThreadPoolExecutor& ex)
        : _scope(ex)
    {
    }
    /// @brief Destructor, waits for all tasks to be done
    ~BlockingAsyncScope() { _scope.wait_fence(); }

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
