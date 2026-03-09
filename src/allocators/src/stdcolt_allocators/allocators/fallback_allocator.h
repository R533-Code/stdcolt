/*****************************************************************/ /**
 * @file   fallback_allocator.h
 * @brief  Contains `FallbackAllocator`.
 * 
 * @author Raphael Dib Nehme
 * @date   March 2026
 *********************************************************************/
#ifndef __HG_STDCOLT_ALLOCATORS_FALLBACK_ALLOCATOR
#define __HG_STDCOLT_ALLOCATORS_FALLBACK_ALLOCATOR

#include <stdcolt_allocators/block.h>
#include <stdcolt_allocators/allocator.h>

namespace stdcolt::alloc
{
  /// @brief Fallback allocator: uses an allocator, and on failure fallbacks to another
  /// @tparam PRIMARY The primary allocator
  /// @tparam SECONDARY The fallback allocator
  template<IsOwningAllocator PRIMARY, IsAllocator SECONDARY>
  class FallbackAllocator
      : private PRIMARY
      , private SECONDARY
  {
    static_assert(
        PRIMARY::allocator_info.is_fallible,
        "FallbackAllocator: PRIMARY is infallible, SECONDARY is unreachable");

  public:
    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe = PRIMARY::allocator_info.is_thread_safe
                          && SECONDARY::allocator_info.is_thread_safe,
        .is_fallible         = SECONDARY::allocator_info.is_fallible,
        .is_nothrow_fallible = PRIMARY::allocator_info.is_nothrow_fallible
                               && SECONDARY::allocator_info.is_nothrow_fallible,
        .returns_exact_size = PRIMARY::allocator_info.returns_exact_size
                              && SECONDARY::allocator_info.returns_exact_size,
        .alignment = min_of(
            PRIMARY::allocator_info.alignment, SECONDARY::allocator_info.alignment),
    };

    Block allocate(Layout request) noexcept(
        !allocator_info.is_fallible || allocator_info.is_nothrow_fallible)
    {
      if constexpr (PRIMARY::allocator_info.is_nothrow_fallible)
      {
        Block blk = PRIMARY::allocate(request);
        if (blk != nullblock)
          return blk;
      }
      else
      {
        try
        {
          return PRIMARY::allocate(request);
        }
        catch (...)
        {
          // fallback...
        }
      }
      return SECONDARY::allocate(request);
    }

    void deallocate(Block blk) noexcept
    {
      if (PRIMARY::owns(blk))
      {
        PRIMARY::deallocate(blk);
        return;
      }
      SECONDARY::deallocate(blk);
    }

    bool owns(Block blk) const noexcept
      requires IsOwningAllocator<SECONDARY>
    {
      return PRIMARY::owns(blk) || SECONDARY::owns(blk);
    }
  };
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_FALLBACK_ALLOCATOR
