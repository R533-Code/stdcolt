/*****************************************************************/ /**
 * @file   flag.h
 * @brief  Contains awaitable `Flag`.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_COROUTINES_FLAG
#define __HG_STDCOLT_COROUTINES_FLAG

#include <coroutine>
#include <atomic>
#include <cstdint>
#include <stdcolt_contracts/contracts.h>

namespace stdcolt::coroutines
{
  /// @brief Flag for single producer, single consumer.
  /// This flag should be used as an awaitable for `co_await`.
  /// It is only correct to use for a single producer and single consumer.
  class FlagSPSC
  {
    /// @brief The state of the flag.
    /// If state == 0, then the flag is not set.
    /// If state == 1, then the flag is set.
    /// If state != 0 && state != 1, the flag is set to the address of the coroutine.
    /// This does assume that the coroutine will not be at address 1, but due
    /// to alignment, it most likely will never be.
    std::atomic<uintptr_t> _state = 0;

  public:
    /// @brief Construct the flag.
    /// @param initially_set If true, the flag starts in the set state.
    explicit FlagSPSC(bool initially_set = false) noexcept
        : _state(initially_set ? uintptr_t{1} : uintptr_t{0})
    {
    }
    /// @brief Destructor
    ~FlagSPSC() noexcept
    {
      STDCOLT_debug_pre(
          _state.load(std::memory_order_relaxed) <= 1,
          "flag must not be destroyed if there is a waiter");
    }
    FlagSPSC(FlagSPSC&&)                 = delete;
    FlagSPSC(const FlagSPSC&)            = delete;
    FlagSPSC& operator=(FlagSPSC&&)      = delete;
    FlagSPSC& operator=(const FlagSPSC&) = delete;

    /// @brief Check if the flag is set.
    /// @return True if the flag is set.
    bool is_set() const noexcept
    {
      return _state.load(std::memory_order_relaxed) == 1;
    }

    /// @brief Sets the flag.
    /// If a consumer is already waiting, it is executed immediately.
    /// After this, the flag remains set until reset().
    void set()
    {
      auto old = _state.exchange(1, std::memory_order_release);
      if (old > 1)
      {
        auto coroutine = std::coroutine_handle<>::from_address((void*)old);
        coroutine.resume();
      }
    }

    /// @brief Resets the flag.
    /// If the flag is already reset, does nothing.
    /// This function should (usually only) be called by the consumer.
    void reset() noexcept
    {
      uintptr_t old = 1;
      _state.compare_exchange_strong(
          old, 0, std::memory_order_relaxed, std::memory_order_relaxed);
    }

    /// @brief Awaits the flag.
    /// If the flag is set, then the coroutine resumes directly.
    /// If not, it is suspended and executed directly when `set` is called.
    /// @return Awaitable
    auto operator co_await() noexcept
    {
      struct awaiter
      {
        FlagSPSC& flag;

        bool await_ready() const noexcept { return flag.is_set(); }
        bool await_suspend(std::coroutine_handle<> awaiter)
        {
          uintptr_t old = 0;
          // consumer does not need to synchronize its data with
          // producer, so only acquire.
          return flag._state.compare_exchange_strong(
              old, (uintptr_t)awaiter.address(), std::memory_order_relaxed,
              std::memory_order_relaxed);
        }
        void await_resume() const noexcept
        {
          // synchronization point with `set`
          (void)flag._state.load(std::memory_order_acquire);
        }
      };
      return awaiter{*this};
    }
  };

  /// @brief Flag for multiple producers, multiple consumers.
  /// This flag should be used as an awaitable for `co_await`.
  /// @warning Waiter coroutines must outlive the wait.
  class FlagMPMC
  {
    /// @brief List of waiters
    struct Waiter
    {
      /// @brief The coroutine that is waiting
      std::coroutine_handle<> handle;
      /// @brief The next waiter node
      Waiter* next = nullptr;
    };

    /// @brief The state of the flag.
    /// If bit 0 of state == 0, then the flag is not set.
    /// If bit 0 of state == 1, then the flag is set.
    /// The rest of the bits are the pointer to the head of waiter stack.
    std::atomic<uintptr_t> _state{0};

    static constexpr uintptr_t SET_BIT = 1;
    /// @brief Check if a state is set
    /// @param s The state to set
    /// @return True if the state is marked as set
    static bool is_set_state(uintptr_t s) noexcept { return (s & SET_BIT) != 0; }

    /// @brief Returns the head from an encoded state
    /// @param s The state
    /// @return The head
    static Waiter* head_from_state(uintptr_t s) noexcept
    {
      return reinterpret_cast<Waiter*>(s & ~SET_BIT);
    }
    /// @brief Creates an encoded state
    /// @param head The head
    /// @param set True if set
    /// @return The encoded state
    static uintptr_t make_state(Waiter* head, bool set) noexcept
    {
      return (reinterpret_cast<uintptr_t>(head) & ~SET_BIT) | (set ? SET_BIT : 0);
    }

  public:
    /// @brief Construct the flag.
    /// @param initially_set If true, the flag starts in the set state.
    explicit FlagMPMC(bool initially_set = false) noexcept
        : _state(make_state(nullptr, initially_set))
    {
    }
    /// @brief Destructor
    ~FlagMPMC()
    {
      STDCOLT_debug_pre(
          head_from_state(_state.load(std::memory_order_relaxed)) == nullptr,
          "flag must not be destroyed if there is a waiter");
    }
    FlagMPMC(FlagMPMC&&)                 = delete;
    FlagMPMC(const FlagMPMC&)            = delete;
    FlagMPMC& operator=(FlagMPMC&&)      = delete;
    FlagMPMC& operator=(const FlagMPMC&) = delete;

    /// @brief Check if the flag is set.
    /// @return True if the flag is set.
    bool is_set() const noexcept
    {
      auto s = _state.load(std::memory_order_relaxed);
      return is_set_state(s);
    }

    /// @brief Sets the flag and resumes all current waiters.
    /// After this, the flag remains set until reset().
    void set()
    {
      for (;;)
      {
        uintptr_t s  = _state.load(std::memory_order_relaxed);
        Waiter* head = head_from_state(s);
        if (is_set_state(s) && head == nullptr)
          return;

        // we want to transition to set == true, no waiters.
        uintptr_t desired = make_state(nullptr, true);

        if (_state.compare_exchange_weak(
                s, desired, std::memory_order_release, std::memory_order_relaxed))
        {
          // we now exclusively own the whole list: resume all coroutines
          while (head)
          {
            Waiter* next = head->next;
            auto h       = head->handle;
            h.resume();
            head = next;
          }
          return;
        }
      }
    }

    /// @brief Resets the flag.
    /// If the flag is already reset, does nothing.
    /// This function should (usually only) be called by the consumer.
    void reset() noexcept
    {
      for (;;)
      {
        uintptr_t s = _state.load(std::memory_order_relaxed);
        // if not set, do nothing
        if (!is_set_state(s))
          return;

        // we want to clear the set bit but keep the head
        uintptr_t desired = s & ~SET_BIT;
        if (_state.compare_exchange_weak(
                s, desired, std::memory_order_relaxed, std::memory_order_relaxed))
          return;
      }
    }

    auto operator co_await() noexcept
    {
      struct awaiter
      {
        FlagMPMC& flag;
        Waiter node;

        explicit awaiter(FlagMPMC& f) noexcept
            : flag(f)
        {
          node.handle = std::noop_coroutine();
          node.next   = nullptr;
        }

        bool await_ready() const noexcept
        {
          auto s = flag._state.load(std::memory_order_relaxed);
          return is_set_state(s);
        }

        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
          node.handle = h;

          for (;;)
          {
            uintptr_t s = flag._state.load(std::memory_order_relaxed);

            // if flag is already set, do not suspend
            if (is_set_state(s))
              return false;

            Waiter* head      = head_from_state(s);
            node.next         = head;
            uintptr_t desired = make_state(&node, false);

            if (flag._state.compare_exchange_weak(
                    s, desired, std::memory_order_relaxed,
                    std::memory_order_relaxed))
            {
              // successfully enqueued, suspend
              return true;
            }
          }
        }

        void await_resume() const noexcept
        {
          // synchronization point with `set`
          (void)flag._state.load(std::memory_order_acquire);
        }

      private:
        static bool is_set_state(uintptr_t s) noexcept
        {
          return FlagMPMC::is_set_state(s);
        }
        static Waiter* head_from_state(uintptr_t s) noexcept
        {
          return FlagMPMC::head_from_state(s);
        }
        static uintptr_t make_state(Waiter* head, bool set) noexcept
        {
          return FlagMPMC::make_state(head, set);
        }
      };

      return awaiter{*this};
    }
  };
} // namespace stdcolt::coroutines

#endif // !__HG_STDCOLT_COROUTINES_FLAG
