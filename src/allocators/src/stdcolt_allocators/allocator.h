/*****************************************************************/ /**
 * @file   allocator.h
 * @brief  Contains `IsAllocator` concept and allocator failure utilities.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_ALLOCATORS_ALLOCATOR
#define __HG_STDCOLT_ALLOCATORS_ALLOCATOR

#include <source_location>
#include <cstddef>

#include <stdcolt_allocators_export.h>
#include <stdcolt_allocators/block.h>
#include <stdcolt_contracts/contracts.h>

/// @brief Everything related to memory allocation
namespace stdcolt::alloc
{
  /// @brief The recommended alignment that allocators should have.
  /// This value is guaranteed to be >= to the alignment of `std::max_align_t`,
  /// and is usually aligned enough for SIMD primitives.
  inline constexpr std::size_t PREFERRED_ALIGNMENT =
#ifdef __STDCPP_DEFAULT_NEW_ALIGNMENT__
      __STDCPP_DEFAULT_NEW_ALIGNMENT__;
#else
      alignof(std::max_align_t);
#endif
  ;

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

  /// @brief Allocator information
  struct AllocatorInfo
  {
    /// @brief Is the allocator thread safe?
    /// If it is, any of the allocation methods can be called from any
    /// threads without risking data races.
    bool is_thread_safe = false;
    /// @brief Is the allocator fallible?
    /// If it is, the `is_nothow_fallible` explains failure handling.
    /// If it isn't, then the allocator must terminate on failure.
    bool is_fallible = true;
    /// @brief Is the allocator nothrow fallible?
    /// If it is, then nullblock is returned on failure.
    /// If not, an exception is thrown on failure (`std::bad_alloc`).
    bool is_nothrow_fallible = false;
    /// @brief Does the allocator always return an allocation of the exact size requested?
    /// If not, the Block passed to the allocator in `deallocate` MUST be of the exact same
    /// size as obtained by `allocate`.
    bool returns_exact_size = false;
    /// @brief The minimum alignment guaranteed by the allocator
    size_t alignment = 1;
  };

  class Layout
  {
    size_t _size;
    size_t _align;

  public:
    constexpr Layout(size_t size, size_t align) noexcept
        : _size(size)
        , _align(align)
    {
    }

    constexpr Layout(Layout&&) noexcept                 = default;
    constexpr Layout(const Layout&) noexcept            = default;
    constexpr Layout& operator=(Layout&&) noexcept      = default;
    constexpr Layout& operator=(const Layout&) noexcept = default;

    constexpr size_t size() const noexcept { return _size; }
    constexpr size_t align() const noexcept { return _align; }
  };

  template<typename T>
  concept IsAllocator = requires(T alloc, Layout alloc_req, Block block) {
    { T::allocator_info } -> std::convertible_to<AllocatorInfo>;

    // allocates a memory block
    { alloc.allocate(alloc_req) } -> std::same_as<Block>;
    // deallocates a memory block
    { alloc.deallocate(block) } noexcept -> std::same_as<void>;

    // noexcept rules for allocate:
    //  - not fallible          -> must be noexcept (terminates on failure)
    //  - fallible & nothrow    -> must be noexcept (returns nullblock)
    //  - fallible & may throw  -> must NOT be noexcept
    requires(
        T::allocator_info.is_fallible ? (noexcept(alloc.allocate(alloc_req))
                                         == T::allocator_info.is_nothrow_fallible)
                                      : noexcept(alloc.allocate(alloc_req)));

    // is_nothrow_fallible implies is_fallible.
    requires(
        !T::allocator_info.is_nothrow_fallible || T::allocator_info.is_fallible);
    requires(T::allocator_info.alignment != 0);
  };

  /// @brief The function to call on allocation failure.
  /// The function receives the attempted allocation, and
  /// the source location of the allocation.
  using alloc_fail_fn_t = void (*)(Layout, const std::source_location&) noexcept;

  /// @brief Register a function to call on infallible allocation failure.
  /// @param fn The function to call on infallible allocation failure.
  /// @return The old registered function
  /// @note This function is thread safe.
  /// @pre `fn` must not be nullptr
  STDCOLT_ALLOCATORS_EXPORT
  alloc_fail_fn_t register_on_alloc_fail(alloc_fail_fn_t fn) noexcept;

  [[noreturn]]
  STDCOLT_ALLOCATORS_EXPORT
      /// @brief The default function that is called on allocation failure
      /// @param size The size of the allocation
      /// @param loc The source location of the allocation
      void default_on_alloc_fail(
          Layout size, const std::source_location& loc) noexcept;

  [[noreturn]]
  STDCOLT_ALLOCATORS_EXPORT
      /// @brief Function that MUST be called when an infallible allocation fails.
      /// @note Allocators calling this function should avoid holding any locks.
      /// @param size The size of the allocation
      /// @param loc The source location
      void handle_alloc_fail(
          Layout size, const std::source_location& loc =
                           STDCOLT_CURRENT_SOURCE_LOCATION) noexcept;
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_ALLOCATOR
