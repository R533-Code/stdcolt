/*****************************************************************/ /**
 * @file   allocator.cpp
 * @brief  Contains the implementation of `allocator.h`.
 * @author Raphael Dib Nehme
 * @date   December 2025
 *********************************************************************/
#include "./allocator.h"
#include <stdcolt_allocators/allocator.h>
#include <stdcolt_allocators/allocators/mallocator.h>

using namespace stdcolt;

extern "C"
{
  stdcolt_ext_rt_RecipeAllocator stdcolt_ext_rt_default_allocator()
  {
    return {
        .allocator_sizeof    = 0,
        .allocator_alignof   = 1,
        .allocator_construct = +[](void*) noexcept { return 0; },
        .allocator_destruct  = +[](void*) noexcept { /* does nothing */ },
        .allocator_alloc =
            +[](void*, uint64_t size, uint64_t align) noexcept
            {
              auto blk =
                  alloc::MallocatorAligned{}.allocate(alloc::Layout{size, align});
              return stdcolt_ext_rt_Block{blk.ptr(), blk.size()};
            },
        .allocator_dealloc =
            +[](void*, const stdcolt_ext_rt_Block* blk) noexcept
            {
              alloc::MallocatorAligned{}.deallocate(
                  alloc::Block{blk->ptr, blk->size});
            },
    };
  }
} // namespace stdcolt::ext::rt
