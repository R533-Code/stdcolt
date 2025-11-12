/*****************************************************************/ /**
 * @file   null_allocator.h
 * @brief  Contains `NullAllocator`, `NullAllocatorThrow`, `NullAllocatorAbort`.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_ALLOCATORS_NULL_ALLOCATOR
#define __HG_STDCOLT_ALLOCATORS_NULL_ALLOCATOR

#include <stdcolt_contracts/contracts.h>
#include <stdcolt_allocators/block.h>
#include <stdcolt_allocators/allocator.h>
#include <exception>

namespace stdcolt::alloc
{
  /// @brief Allocator that always fails by returning `nullblock`.
  /// This allocator always returns `nullblock`.
  struct NullAllocator
  {
    static constexpr bool is_thread_safe      = true;
    static constexpr bool is_fallible         = true;
    static constexpr bool is_nothrow_fallible = true;

    constexpr Block allocate(size_t) const noexcept { return nullblock; }
    constexpr void deallocate(Block blk) const noexcept
    {
      STDCOLT_debug_pre(blk == nullblock, "expected `nullblock`");
    }
  };
  static_assert(IsAllocator<NullAllocator>);

  /// @brief Allocator that always fails by throwing `std::bad_alloc`.
  /// This allocator always throws `std::bad_alloc`.
  struct NullAllocatorThrow
  {
    static constexpr bool is_thread_safe      = true;
    static constexpr bool is_fallible         = true;
    static constexpr bool is_nothrow_fallible = false;

    [[noreturn]]
    Block allocate(size_t) const
    {
      throw std::bad_alloc{};
    }

    [[noreturn]]
    void deallocate(Block) const
    {
      // as allocate never returns Block, deallocate must NEVER be called.
      stdcolt::contracts::unreachable();
    }
  };
  static_assert(IsAllocator<NullAllocatorThrow>);

  /// @brief Allocator that always fails by calling `handle_alloc_fail`.
  struct NullAllocatorAbort
  {
    static constexpr bool is_thread_safe      = true;
    static constexpr bool is_fallible         = false;
    static constexpr bool is_nothrow_fallible = false;

    [[noreturn]]
    Block allocate(size_t size) const
    {
      handle_alloc_fail(size);
    }

    [[noreturn]]
    void deallocate(Block) const
    {
      // as allocate never returns Block, deallocate must NEVER be called.
      stdcolt::contracts::unreachable();
    }
  };
  static_assert(IsAllocator<NullAllocatorAbort>);
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_NULL_ALLOCATOR
