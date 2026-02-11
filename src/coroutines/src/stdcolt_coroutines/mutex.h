#ifndef __HG_STDCOLT_COROUTINES_MUTEX
#define __HG_STDCOLT_COROUTINES_MUTEX

#include <coroutine>
#include <atomic>
#include <cstdint>
#include <stdcolt_contracts/contracts.h>

namespace stdcolt::coroutines
{
  /// @brief Mutex suitable for usage with coroutines.
  /// @warning Waiting coroutines must never be destroyed while waiting.
  class AsyncMutex
  {
    /// @brief Linked list of waiters.
    /// Each node lives inside the waiting coroutine frame,
    /// thus there are no allocations.
    struct Waiter
    {
      /// @brief The waiting coroutine handle
      std::coroutine_handle<> handle;
      /// @brief The next waiter in the list
      Waiter* next = nullptr;
    };

    /// @brief The state of the mutex.
    /// If bit 0 of state == 0, then unlocked.
    /// If bit 0 of state == 1, then locked.
    /// The rest of the bits are the pointer to the head of waiter stack.
    std::atomic<uintptr_t> _state{0};

    /// @brief The lock bit
    static constexpr uintptr_t LOCKED_BIT = 1;

    /// @brief Check if a state represents a locked mutex
    /// @param s The state
    /// @return True if locked
    static bool is_locked(uintptr_t s) noexcept { return (s & LOCKED_BIT) != 0; }
    /// @brief Obtain the encoded waiter from a state
    /// @param s The state
    /// @return The waiter pointer
    static Waiter* head_from_state(uintptr_t s) noexcept
    {
      return reinterpret_cast<Waiter*>(s & ~LOCKED_BIT);
    }
    /// @brief Creates an encoded state
    /// @param head The pointer
    /// @param locked True if locked
    /// @return Encoded state
    static uintptr_t make_state(Waiter* head, bool locked) noexcept
    {
      return (reinterpret_cast<uintptr_t>(head) & ~LOCKED_BIT)
             | (locked ? LOCKED_BIT : 0);
    }

  public:
    AsyncMutex() noexcept = default;
    ~AsyncMutex() noexcept
    {
      // Cannot destroy while locked or with waiters.
      STDCOLT_debug_pre(
          _state.load(std::memory_order_relaxed) == 0,
          "AsyncMutex must not be destroyed while locked or with waiters");
    }

    AsyncMutex(const AsyncMutex&)            = delete;
    AsyncMutex& operator=(const AsyncMutex&) = delete;
    AsyncMutex(AsyncMutex&&)                 = delete;
    AsyncMutex& operator=(AsyncMutex&&)      = delete;

    /// @brief Try to lock the mutex
    /// @return True if lock was successful
    bool try_lock() noexcept
    {
      uintptr_t s = _state.load(std::memory_order_relaxed);
      if (is_locked(s))
        return false;

      Waiter* head      = head_from_state(s);
      uintptr_t desired = make_state(head, true);

      return _state.compare_exchange_strong(
          s, desired, std::memory_order_acquire, std::memory_order_relaxed);
    }

    /// @brief Unlock the mutex
    void unlock() noexcept
    {
      for (;;)
      {
        auto s = _state.load(std::memory_order_acquire);

        STDCOLT_debug_pre(
            is_locked(s), "AsyncMutex::unlock() called while not locked");

        Waiter* h = head_from_state(s);

        if (!h)
        {
          if (_state.compare_exchange_weak(
                  s, make_state(nullptr, false), std::memory_order_release,
                  std::memory_order_relaxed))
            return;
        }
        else
        {
          Waiter* next = h->next;
          auto desired = make_state(next, true);

          if (_state.compare_exchange_weak(
                  s, desired, std::memory_order_release, std::memory_order_relaxed))
          {
            auto to_resume = h->handle;
            STDCOLT_debug_pre(
                to_resume && to_resume != std::noop_coroutine(),
                "AsyncMutex waiter handle must be valid");
            to_resume.resume();
            return;
          }
        }
      }
    }

    /// @brief Returns a lock awaitable
    /// @return Lock awaitable
    auto lock() noexcept
    {
      struct awaiter
      {
        AsyncMutex& m;
        Waiter node;

        explicit awaiter(AsyncMutex& mm) noexcept
            : m(mm)
        {
          node.handle = std::noop_coroutine();
          node.next   = nullptr;
        }
        bool await_ready() noexcept { return m.try_lock(); }
        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
          STDCOLT_debug_pre(
              h && h != std::noop_coroutine(),
              "AsyncMutex await_suspend requires a real coroutine handle");

          node.handle = h;

          for (;;)
          {
            auto s = m._state.load(std::memory_order_relaxed);

            if (!is_locked(s))
            {
              Waiter* head = head_from_state(s);
              auto desired = make_state(head, true);
              if (m._state.compare_exchange_weak(
                      s, desired, std::memory_order_acq_rel,
                      std::memory_order_relaxed))
                return false;
              continue;
            }

            Waiter* head = head_from_state(s);
            node.next    = head;
            auto desired = make_state(&node, true);

            if (m._state.compare_exchange_weak(
                    s, desired, std::memory_order_release,
                    std::memory_order_relaxed))
              return true;
          }
        }
        void await_resume() noexcept
        {
          // synchronize with unlock() release
          (void)m._state.load(std::memory_order_acquire);
        }
      };

      return awaiter{*this};
    }

    /// @brief Await the lock awaiter (shorthand for `co_await mutex.lock()`)
    /// @return Lock awaiter
    auto operator co_await() noexcept { return lock(); }

    /// @brief RAII lock guard
    class guard
    {
      AsyncMutex* _m = nullptr;

    public:
      /// @brief Creates an empty guard
      guard() noexcept               = default;
      guard(const guard&)            = delete;
      guard& operator=(const guard&) = delete;
      /// @brief Constructor
      /// @param m The mutex that must be already locked
      explicit guard(AsyncMutex& m) noexcept
          : _m(&m)
      {
      }
      /// @brief Move constructor
      /// @param other The value to move from
      guard(guard&& other) noexcept
          : _m(other._m)
      {
        other._m = nullptr;
      }
      /// @brief Move assignment operator
      /// @param other The value to move from
      /// @return This
      guard& operator=(guard&& other) noexcept
      {
        if (this == &other)
          return *this;
        std::swap(other._m, _m);
        return *this;
      }
      /// @brief Releases the lock if owned
      ~guard() noexcept
      {
        if (_m)
          _m->unlock();
      }

      /// @brief Releases the lock and marks the guard empty
      void unlock() noexcept
      {
        STDCOLT_debug_pre(_m != nullptr, "guard::unlock() on empty guard");
        _m->unlock();
        _m = nullptr;
      }
    };

    /// @brief Scoped lock/unlock guard
    /// @return Awaitable
    auto lock_guard() noexcept
    {
      struct guard_awaiter
      {
        AsyncMutex& m;
        decltype(m.lock()) inner;

        explicit guard_awaiter(AsyncMutex& mm) noexcept
            : m(mm)
            , inner(mm.lock())
        {
        }
        bool await_ready() noexcept { return inner.await_ready(); }
        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
          return inner.await_suspend(h);
        }
        guard await_resume() noexcept
        {
          inner.await_resume();
          return guard{m};
        }
      };
      return guard_awaiter{*this};
    }
  };
} // namespace stdcolt::coroutines

#endif // !__HG_STDCOLT_COROUTINES_MUTEX
