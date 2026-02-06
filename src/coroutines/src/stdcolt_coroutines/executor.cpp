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

    PostStatus post(WorkItem, time_point) noexcept override
    {
      return PostStatus::POST_FAIL_NOT_IMPLEMENTED;
    }

    /// @brief Post a work item to evaluate in the thread pool
    /// @param item The work item
    /// @return PostStatus on success
    PostStatus post(WorkItem item) noexcept final
    {
      if (_stopping.load(std::memory_order_acquire) != 0)
        return PostStatus::POST_FAIL_STOPPED;

      _outstanding.fetch_add(1, std::memory_order_relaxed);

      bool res = false;
      if (tls_executor_info.executor == this
          && tls_executor_info.worker_id != invalid_index)
        res = _workers[tls_executor_info.worker_id].queue.enqueue(item);
      else
        res = _global_queue.enqueue(item);

      if (!res)
      {
        _outstanding.fetch_sub(1, std::memory_order_relaxed);
        return PostStatus::POST_FAIL_MEMORY;
      }

      _work_epoch.fetch_add(1);
      _work_epoch.notify_one();
      return PostStatus::POST_SUCCESS;
    }

    /// @brief Stops the executor. Work is drained.
    /// This method is thread safe and idempotent.
    /// @warning This function must never be called on one of the worker threads.
    void stop() noexcept override
    {
      // transition 0 -> 1 (draining)
      if (_stopping.load(std::memory_order_acquire) == 2)
        return;

      int32_t expected = 0;
      if (!_stopping.compare_exchange_strong(
              expected, 1, std::memory_order_acq_rel, std::memory_order_acquire))
      {
        if (expected == 1)
          _stopping.wait(1, std::memory_order_acquire);
        return;
      }

      // wake all workers so they can start draining
      _work_epoch.fetch_add(1);
      _work_epoch.notify_all();

      for (auto& [thread, _] : _workers)
        if (thread.joinable())
          thread.join();

      _stopping.store(2, std::memory_order_release);
      _stopping.notify_all();
    }

  protected:
    /// @brief Worker, must have a stable address
    struct Worker
    {
      /// @brief The actual worker thread
      std::thread thread;
      /// @brief The local queue.
      /// This queue is concurrent to allow safe work stealing.
      moodycamel::ConcurrentQueue<WorkItem> queue;
    };

    /// @brief Work epoch for efficient waiting
    /// It is preferable to use 32 bit integers for a direct lowering to futexes.
    std::atomic<uint32_t> _work_epoch{0};
    /// @brief Array of workers, size constant throughout lifetime
    std::vector<Worker> _workers;
    /// @brief Global queue from which worker may pop work
    moodycamel::ConcurrentQueue<WorkItem> _global_queue;
    /// @brief If 0, stop was not requested, if 1, stop requested, if 2 stopped
    /// It is preferable to use 32 bit integers for a direct lowering to futexes.
    std::atomic<int32_t> _stopping{0};
    /// @brief Work queued and in progress
    std::atomic<uint64_t> _outstanding{0};

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

      auto consume_one = [&]() noexcept
      {
        const auto prev = _outstanding.fetch_sub(1, std::memory_order_relaxed);
        STDCOLT_assert(prev > 0, "outstanding underflow");

        if (prev == 1 && _stopping.load(std::memory_order_relaxed) != 0)
        {
          _work_epoch.fetch_add(1, std::memory_order_relaxed);
          _work_epoch.notify_all();
        }
      };

      auto run_one = [&](WorkItem item) noexcept
      {
        if (item.run != nullptr)
          item.run(item.ctx); // noexcept

        consume_one();
      };

      for (;;)
      {
        WorkItem item;

        if (self_queue.try_dequeue(item) || _global_queue.try_dequeue(item))
        {
          run_one(item);
          continue;
        }

        bool got = false;
        for (size_t i = 0; i < worker_count; ++i)
        {
          if (i == index)
            continue;
          if (_workers[i].queue.try_dequeue(item))
          {
            got = true;
            run_one(item);
            break;
          }
        }
        if (got)
          continue;

        if (_stopping.load(std::memory_order_relaxed) != 0)
        {
          if (_outstanding.load(std::memory_order_relaxed) == 0)
            break;
        }

        const auto expected = _work_epoch.load(std::memory_order_acquire);
        _work_epoch.wait(expected, std::memory_order_relaxed);
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

    PostStatus post(WorkItem item, time_point when) noexcept override
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

          _scheduled.emplace(ScheduledItem{
              when, item, _next_id.fetch_add(1, std::memory_order_relaxed)});
          _outstanding.fetch_add(1, std::memory_order_relaxed);
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

      {
        // notify_all does not guarantee that threads beginning to wait
        // later will observe the notification...
        std::lock_guard<std::mutex> lk(_timer_mutex);
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
      WorkItem item;
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
      std::unique_lock lk{_timer_mutex};

      auto consume_one_scheduled = [&]() noexcept
      {
        const auto prev = _outstanding.fetch_sub(1, std::memory_order_relaxed);
        STDCOLT_assert(prev > 0, "outstanding underflow (scheduled)");

        // if draining and we just hit 0, wake pool sleepers so they can exit.
        if (prev == 1 && _stopping.load(std::memory_order_relaxed) != 0)
        {
          _work_epoch.fetch_add(1, std::memory_order_relaxed);
          _work_epoch.notify_all();
        }
      };

      for (;;)
      {
        if (_timer_state.load(std::memory_order_acquire) != 0 && _scheduled.empty())
          break;

        if (_scheduled.empty())
        {
          while (_scheduled.empty()
                 && _timer_state.load(std::memory_order_acquire) == 0)
            _timer_cv.wait(lk);
          continue;
        }

        const auto next_when = _scheduled.top().when;

        // Wake on stop requested or new earlier deadline
        _timer_cv.wait_until(
            lk, next_when,
            [this, next_when]
            {
              if (_timer_state.load(std::memory_order_acquire) != 0)
                return true;
              return !_scheduled.empty() && _scheduled.top().when < next_when;
            });

        // if new earlier deadline, restart
        if (!_scheduled.empty() && _scheduled.top().when < next_when)
          continue;

        // dispatch all due items
        const auto now = clock::now();
        while (!_scheduled.empty() && _scheduled.top().when <= now)
        {
          ScheduledItem item = _scheduled.top();
          _scheduled.pop();

          lk.unlock();

          consume_one_scheduled();

          if (item.item.run != nullptr)
          {
            const auto ps = ThreadPoolExecutor::post(item.item);
            if (ps != PostStatus::POST_SUCCESS)
            {
              // fallback: execute on timer thread so work is not lost.
              // NOTE: Work items must not block here.
              item.item.run(item.item.ctx); // noexcept
            }
          }

          lk.lock();
        }
      }
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
