/*****************************************************************/ /**
 * @file   executor.cpp
 * @brief  Contains the implementation of `executor.h`.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <stdcolt_coroutines/executor.h>

namespace stdcolt::coroutines
{
  /// @brief Non-owning coroutine executor
  class ThreadPoolExecutor : public Executor
  {
  public:
    /// @brief Constructor
    /// @param thread_count The number of threads the executor may use, max(1, thread_count)
    explicit ThreadPoolExecutor(size_t thread_count)
    {
      if (thread_count == 0)
        thread_count = 1;
      // must resize not reserve so that no reallocation may occur
      _workers.resize(thread_count);
      for (size_t i = 0; i < thread_count; ++i)
        _workers[i].thread = std::thread([this, i] { worker_loop(i); });
    }
    /// @brief Destructor, drops the remaining work
    ~ThreadPoolExecutor() override { stop(); }

    ThreadPoolExecutor(ThreadPoolExecutor&&)                 = delete;
    ThreadPoolExecutor(const ThreadPoolExecutor&)            = delete;
    ThreadPoolExecutor& operator=(ThreadPoolExecutor&&)      = delete;
    ThreadPoolExecutor& operator=(const ThreadPoolExecutor&) = delete;

    PostStatus post(handle h, time_point) noexcept override
    {
      return PostStatus::POST_FAIL_NOT_IMPLEMENTED;
    }

    /// @brief Post a coroutine to evaluate in the thread pool
    /// @param h The coroutine to evaluate
    /// @return True on success
    PostStatus post(handle h) noexcept final
    {
      if (_stopping.load(std::memory_order_relaxed) != 0)
        return PostStatus::POST_FAIL_STOPPED;

      bool res = false;
      // if we are in a specific worker thread, do not go through
      // the global queue, instead directly enqueue in that thread.
      if (tls_executor_info.executor == this
          && tls_executor_info.worker_id != invalid_index)
        res = _workers[tls_executor_info.worker_id].queue.enqueue(h);
      else
        res = _global_queue.enqueue(h);

      if (res)
      {
        _work_epoch.fetch_add(1, std::memory_order_release);
        _work_epoch.notify_one();
        // ^^^ as the pool does work stealing, only a single one is needed.
        return PostStatus::POST_SUCCESS;
      }
      return PostStatus::POST_FAIL_MEMORY;
    }

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
      moodycamel::ConcurrentQueue<handle> queue;
    };

    /// @brief Work epoch for efficient waiting
    /// It is preferable to use 32 bit integers for a direct lowering to futexes.
    std::atomic<uint32_t> _work_epoch{0};
    /// @brief Array of workers, size constant throughout lifetime
    std::vector<Worker> _workers;
    /// @brief Global queue from which worker may pop work
    moodycamel::ConcurrentQueue<handle> _global_queue;
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
        handle h;
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
  thread_local ThreadPoolExecutor::ThreadPoolExecutorInfo
      ThreadPoolExecutor::tls_executor_info = {};

  class ScheduledThreadPoolExecutor : public ThreadPoolExecutor
  {
  public:
    explicit ScheduledThreadPoolExecutor(size_t thread_count)
        : ThreadPoolExecutor(thread_count)
    {
      _timer_thread = std::thread([this] { timer_loop(); });
    }

    ~ScheduledThreadPoolExecutor() override { stop(); }

    PostStatus post(handle h, time_point when) noexcept override
    {
      using enum PostStatus;

      if (clock::now() >= when)
        return POST_FAIL_DEADLINE_PASSED;

      if (_timer_state.load(std::memory_order_acquire) != 0)
        return POST_FAIL_STOPPED;

      try
      {
        {
          std::lock_guard<std::mutex> lk(_timer_mutex);
          if (_timer_state.load(std::memory_order_acquire) != 0)
            return POST_FAIL_STOPPED;

          _scheduled.emplace(
              when, h, _next_id.fetch_add(1, std::memory_order_relaxed));
        }
        _timer_cv.notify_one();
        return POST_SUCCESS;
      }
      catch (...)
      {
        return POST_FAIL_MEMORY;
      }
    }

    void stop() noexcept override
    {
      if (_timer_state.load(std::memory_order_acquire) == 2)
      {
        ThreadPoolExecutor::stop();
        return;
      }

      if (int32_t expected = 0; !_timer_state.compare_exchange_strong(
              expected, 1, std::memory_order_acq_rel, std::memory_order_acquire))
      {
        if (expected == 1)
          _timer_state.wait(1, std::memory_order_acquire);

        ThreadPoolExecutor::stop();
        return;
      }

      _timer_cv.notify_all();
      if (_timer_thread.joinable())
        _timer_thread.join();

      _timer_state.store(2, std::memory_order_release);
      _timer_state.notify_all();

      // now stop the underlying thread pool (idempotent, thread-safe)
      ThreadPoolExecutor::stop();
    }

  private:
    struct ScheduledItem
    {
      time_point when;
      handle h;
      uint64_t id; // tie-breaker to keep ordering stable
    };

    struct ScheduledItemCompare
    {
      bool operator()(const ScheduledItem& a, const ScheduledItem& b) const noexcept
      {
        // earlier id has higher priority
        if (a.when == b.when)
          return a.id > b.id;
        // min-heap by time
        return a.when > b.when;
      }
    };

    // Timer state: 0 = running, 1 = stopping, 2 = stopped
    std::atomic<int32_t> _timer_state{0};

    // Priority queue of scheduled items (earliest deadline on top)
    std::priority_queue<
        ScheduledItem, std::vector<ScheduledItem>, ScheduledItemCompare>
        _scheduled;

    std::mutex _timer_mutex;
    std::condition_variable _timer_cv;
    std::thread _timer_thread;
    std::atomic<uint64_t> _next_id{0};

    void timer_loop()
    {
      auto lk = std::unique_lock{_timer_mutex};

      for (;;)
      {
        // Check for stop
        if (_timer_state.load(std::memory_order_acquire) != 0)
          break;

        if (_scheduled.empty())
        {
          _timer_cv.wait(
              lk,
              [this]
              {
                return _timer_state.load(std::memory_order_acquire) != 0
                       || !_scheduled.empty();
              });
          continue;
        }

        auto next_when = _scheduled.top().when;

        // wait until next deadline or stop
        _timer_cv.wait_until(
            lk, next_when,
            [this] { return _timer_state.load(std::memory_order_acquire) != 0; });

        if (_timer_state.load(std::memory_order_acquire) != 0)
          break;

        if (auto now = clock::now(); now < next_when)
          continue;

        // time reached (or passed) for the top item
        ScheduledItem item = _scheduled.top();
        _scheduled.pop();

        lk.unlock();
        if (!item.h.done())
          (void)ThreadPoolExecutor::post(item.h);

        lk.lock();
      }

      // drop remaining scheduled work
      while (!_scheduled.empty())
        _scheduled.pop();
    }
  };

  std::unique_ptr<Executor> make_executor(
      size_t thread_count, bool with_scheduler) noexcept
  {
    try
    {
      if (with_scheduler)
        return std::make_unique<ScheduledThreadPoolExecutor>(thread_count);
      else
        return std::make_unique<ThreadPoolExecutor>(thread_count);
    }
    catch (...)
    {
      return {};
    }
  }
} // namespace stdcolt::coroutines
