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
    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe      = true,
        .is_fallible         = true,
        .is_nothrow_fallible = true,
        .returns_exact_size  = true,
        .alignment           = PREFERRED_ALIGNMENT,
    };

    constexpr Block allocate(Layout) const noexcept { return nullblock; }
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
    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe      = true,
        .is_fallible         = true,
        .is_nothrow_fallible = false,
        .returns_exact_size  = true,
        .alignment           = PREFERRED_ALIGNMENT,
    };

    [[noreturn]]
    Block allocate(Layout) const
    {
      throw std::bad_alloc{};
    }

    [[noreturn]]
    void deallocate(Block) const noexcept
    {
      // as allocate never returns Block, deallocate must NEVER be called.
      stdcolt::contracts::unreachable();
    }
  };
  static_assert(IsAllocator<NullAllocatorThrow>);

  /// @brief Allocator that always fails by calling `handle_alloc_fail`.
  struct NullAllocatorAbort
  {
    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe     = true,
        .is_fallible        = false,
        .returns_exact_size = true,
        .alignment          = PREFERRED_ALIGNMENT,
    };

    [[noreturn]]
    Block allocate(Layout request) const noexcept
    {
      handle_alloc_fail(request);
    }

    [[noreturn]]
    void deallocate(Block) const noexcept
    {
      // as allocate never returns Block, deallocate must NEVER be called.
      stdcolt::contracts::unreachable();
    }
  };
  static_assert(IsAllocator<NullAllocatorAbort>);
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_NULL_ALLOCATOR
