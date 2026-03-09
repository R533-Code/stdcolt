/*****************************************************************/ /**
 * @file   infallible_allocator.h
 * @brief  Contains `InfallibleAllocator`.
 * 
 * @author Raphael Dib Nehme
 * @date   March 2026
 *********************************************************************/
#ifndef __HG_STDCOLT_ALLOCATORS_INFALLIBLE_ALLOCATOR
#define __HG_STDCOLT_ALLOCATORS_INFALLIBLE_ALLOCATOR

#include <stdcolt_allocators/block.h>
#include <stdcolt_allocators/allocator.h>

namespace stdcolt::alloc
{
  /// @brief Wraps an allocator to make it infallible.
  /// Calls `handle_alloc_fail` on allocation failure
  /// @tparam ALLOCATOR The allocator to wrap
  template<IsAllocator ALLOCATOR>
  class InfallibleAllocator : private ALLOCATOR
  {
    static constexpr AllocatorInfo inherited_info = ALLOCATOR::allocator_info;

  public:
    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe      = inherited_info.is_thread_safe,
        .is_fallible         = false,
        .is_nothrow_fallible = false,
        .returns_exact_size  = inherited_info.returns_exact_size,
        .alignment           = inherited_info.alignment,
    };

    template<typename... Ts>
    constexpr InfallibleAllocator(Ts&&... vals)
      requires std::is_constructible_v<ALLOCATOR, Ts...>
        : ALLOCATOR(std::forward<Ts>(vals)...)
    {
    }

    Block allocate(Layout request) noexcept
    {
      if constexpr (!inherited_info.is_fallible)
      {
        // already infallible, just forward
        return ALLOCATOR::allocate(request);
      }
      else if constexpr (inherited_info.is_nothrow_fallible)
      {
        // returns nullblock on failure
        Block blk = ALLOCATOR::allocate(request);
        if (blk == nullblock)
          handle_alloc_fail(request, loc);
        return blk;
      }
      else
      {
        // throws on failure
        try
        {
          return ALLOCATOR::allocate(request);
        }
        catch (...)
        {
          handle_alloc_fail(request, loc);
        }
      }
    }

    void deallocate(Block blk) noexcept { ALLOCATOR::deallocate(blk); }

    bool owns(Block blk) const noexcept
      requires IsOwningAllocator<ALLOCATOR>
    {
      return ALLOCATOR::owns(blk);
    }
  };
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_INFALLIBLE_ALLOCATOR
