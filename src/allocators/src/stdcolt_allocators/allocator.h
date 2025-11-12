/*****************************************************************/ /**
 * @file   allocator.h
 * @brief  Contains `IsAllocator` concept and allocator failure utilities.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_ALLOCATORS_ALLOCATOR
#define __HG_STDCOLT_ALLOCATORS_ALLOCATOR

#include <stdcolt_allocators_export.h>
#include <source_location>
#include <stdcolt_allocators/block.h>
#include <stdcolt_contracts/contracts.h>

/// @brief Everything related to memory allocation
namespace stdcolt::alloc
{
  /// @brief Check if an integer is a power of two
  /// @tparam T The integer type
  /// @param n The integer
  /// @return True if power of two
  template<std::integral T>
  constexpr bool is_power_of_2(T n) noexcept
  {
    return (n & (n - 1)) == 0;
  }

  /// @brief Aligns a value up to a multiple of another
  /// @tparam ALIGN_AS The value to align to
  /// @param n The value to align
  /// @return Aligned value
  template<size_t ALIGN_AS>
  constexpr size_t align_up(size_t n) noexcept
  {
    static_assert(ALIGN_AS != 0, "ALIGN_AS must not be zero");
    return (n + ALIGN_AS - 1) / ALIGN_AS * ALIGN_AS;
  }


  template<typename T>
  concept IsAllocator = requires(T alloc, size_t bytes, Block block) {
    // Is the allocator always thread safe?
    { T::is_thread_safe } -> std::convertible_to<bool>;
    // Is the allocator fallible?
    // If it is, the is_nothow_fallible explains failure handling.
    // If it isn't, then the allocator must terminate on failure.
    { T::is_fallible } -> std::convertible_to<bool>;
    // Is the allocator nothrow fallible?
    // If it is then it must return a nullblock on failure.
    // If not, it must throw an exception on failure.
    { T::is_nothrow_fallible } -> std::convertible_to<bool>;

    // allocates a memory block
    { alloc.allocate(bytes) } -> std::same_as<Block>;
    // deallocates a memory block
    { alloc.deallocate(block) } -> std::same_as<void>;

    // vvv small checks to verify correctness
    requires(noexcept(alloc.allocate(bytes)) == T::is_nothrow_fallible);
    requires(noexcept(alloc.deallocate(block)) == T::is_nothrow_fallible);
    // is_nothrow_fallible implies is_fallible.
    requires(!T::is_nothrow_fallible || T::is_fallible);
  };

  /// @brief The function to call on allocation failure.
  /// The function receives the size of the attempted allocation, and
  /// the source location of the allocation.
  using alloc_fail_fn_t = void (*)(size_t, const std::source_location&) noexcept;

  /// @brief Register a function to call on infallible allocation failure.
  /// @param fn The function to call on infallible allocation failure.
  /// @return The old registered function
  /// @note This function is thread safe.
  /// @pre `fn` must not be nullptr
  STDCOLT_ALLOCATORS_EXPORT
  alloc_fail_fn_t register_on_alloc_fail(alloc_fail_fn_t fn) noexcept;

  STDCOLT_ALLOCATORS_EXPORT
  [[noreturn]]
  /// @brief The default function that is called on allocation failure
  /// @param size The size of the allocation
  /// @param loc The source location of the allocation
  void default_on_alloc_fail(size_t size, const std::source_location& loc) noexcept;

  STDCOLT_ALLOCATORS_EXPORT
  [[noreturn]]
  /// @brief Function that MUST be called when an infallible allocation fails.
  /// @note Allocators calling this function should avoid holding any locks.
  /// @param size The size of the allocation
  /// @param loc The source location
  void handle_alloc_fail(
      size_t size,
      const std::source_location& loc = STDCOLT_CURRENT_SOURCE_LOCATION) noexcept;
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_ALLOCATOR
