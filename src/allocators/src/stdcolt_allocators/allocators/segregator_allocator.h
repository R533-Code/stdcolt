/*****************************************************************/ /**
 * @file   segregator_allocator.h
 * @brief  Contains `SegregatorAllocator`.
 * 
 * @author Raphael Dib Nehme
 * @date   March 2026
 *********************************************************************/
#ifndef __HG_STDCOLT_ALLOCATORS_SEGREGATOR_ALLOCATOR
#define __HG_STDCOLT_ALLOCATORS_SEGREGATOR_ALLOCATOR

#include <stdcolt_allocators/block.h>
#include <stdcolt_allocators/allocator.h>

namespace stdcolt::alloc
{
  template<size_t MAX_SIZE, IsAllocator PRIMARY, IsAllocator SECONDARY>
  class SegregatorAllocator
      : private PRIMARY
      , private SECONDARY
  {
    static_assert(
        PRIMARY::allocator_info.returns_exact_size,
        "SegregatorAllocator: PRIMARY must return exact allocation sizes");

  public:
    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe = PRIMARY::allocator_info.is_thread_safe
                          && SECONDARY::allocator_info.is_thread_safe,
        .is_fallible = PRIMARY::allocator_info.is_fallible
                       || SECONDARY::allocator_info.is_fallible,
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
      if (request.size() <= MAX_SIZE)
        return PRIMARY::allocate(request);
      return SECONDARY::allocate(request);
    }

    void deallocate(Block blk) noexcept
    {
      if (blk.size() <= MAX_SIZE)
        PRIMARY::deallocate(blk);
      else
        SECONDARY::deallocate(blk);
    }

    bool owns(Block blk) const noexcept
      requires IsOwningAllocator<PRIMARY> && IsOwningAllocator<SECONDARY>
    {
      return PRIMARY::owns(blk) || SECONDARY::owns(blk);
    }
  };
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_SEGREGATOR_ALLOCATOR
