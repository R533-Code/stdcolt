/*****************************************************************/ /**
 * @file   atomic_coroutine_handle.h
 * @brief  Contains `atomic_coroutine_handle`.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_COROUTINE_ATOMIC_COROUTINE_HANDLE
#define __HG_STDCOLT_COROUTINE_ATOMIC_COROUTINE_HANDLE

#include <atomic>
#include <coroutine>

namespace stdcolt::coroutines
{
  /// @brief atomic coroutine handle.
  /// This is a wrapper over a `void*` storing the coroutine handle's address.
  /// @tparam T The promise type
  template<typename T = void>
  class atomic_coroutine_handle
  {
    /// @brief The underlying atomic storage
    std::atomic<void*> _handle = nullptr;

  public:
    /// @brief Constructor from nullptr
    atomic_coroutine_handle() noexcept = default;
    /// @brief Constructor from handle
    /// @param handle The initial handle
    explicit atomic_coroutine_handle(std::coroutine_handle<T> handle) noexcept
        : _handle(handle.address())
    {
    }

    atomic_coroutine_handle(atomic_coroutine_handle&&)                 = delete;
    atomic_coroutine_handle(const atomic_coroutine_handle&)            = delete;
    atomic_coroutine_handle& operator=(atomic_coroutine_handle&&)      = delete;
    atomic_coroutine_handle& operator=(const atomic_coroutine_handle&) = delete;

    /// @brief Loads a coroutine handle atomically
    /// @param order The memory ordering
    /// @return The loaded coroutine handle
    std::coroutine_handle<T> load(
        const std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
      return std::coroutine_handle<T>::from_address(_handle.load(order));
    }
    /// @brief Stores a coroutine handle atomically
    /// @param handle The handle to store
    /// @param order The memory ordering
    void store(
        std::coroutine_handle<T> handle,
        const std::memory_order order = std::memory_order_seq_cst) noexcept
    {
      _handle.store(handle.address(), order);
    }
    /// @brief Exchanges atomically a coroutine handle
    /// @param handle The handle to exchange with
    /// @param order The memory ordering
    /// @return The previous coroutine
    std::coroutine_handle<T> exchange(
        std::coroutine_handle<T> handle,
        const std::memory_order order = std::memory_order_seq_cst) noexcept
    {
      void* old = _handle.exchange(handle.address(), order);
      return std::coroutine_handle<T>::from_address(old);
    }
    /// @brief Compare exchange strong
    /// @param expected The expected coroutine, modified on failure
    /// @param desired The desired coroutine
    /// @param success The success memory ordering
    /// @param failure The failure memory ordering
    /// @return True on success
    bool compare_exchange_strong(
        std::coroutine_handle<T>& expected, std::coroutine_handle<T> desired,
        const std::memory_order success = std::memory_order_seq_cst,
        const std::memory_order failure = std::memory_order_seq_cst) noexcept
    {
      void* address = expected.address();
      if (_handle.compare_exchange_strong(
              address, desired.address(), success, failure))
        return true;
      expected = std::coroutine_handle<T>::from_address(address);
      return false;
    }
    /// @brief Compare exchange weak
    /// @param expected The expected coroutine, modified on failure
    /// @param desired The desired coroutine
    /// @param success The success memory ordering
    /// @param failure The failure memory ordering
    /// @return True on success
    bool compare_exchange_weak(
        std::coroutine_handle<T>& expected, std::coroutine_handle<T> desired,
        const std::memory_order success = std::memory_order_seq_cst,
        const std::memory_order failure = std::memory_order_seq_cst) noexcept
    {
      void* address = expected.address();
      if (_handle.compare_exchange_weak(
              address, desired.address(), success, failure))
        return true;
      expected = std::coroutine_handle<T>::from_address(address);
      return false;
    }

    /// @brief Check if the handle is lock free on the current platform
    /// @return True if lock free
    bool is_lock_free() const noexcept { return _handle.is_lock_free(); }

    /// @brief Check if the handle is always lock free on the current platform
    static constexpr bool is_always_lock_free =
        std::atomic<void*>::is_always_lock_free;
  };
} // namespace stdcolt::coroutines

#endif // !__HG_STDCOLT_COROUTINE_ATOMIC_COROUTINE_HANDLE
