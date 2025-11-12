/*****************************************************************/ /**
 * @file   mallocator.cpp
 * @brief  Contains the implementation of `mallocator.h`.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#include <stdcolt_allocators/allocators/mallocator.h>
#include <cstdlib>

namespace stdcolt::alloc
{
  Block Mallocator::allocate(size_t size) noexcept
  {
    return {std::malloc(size), size};
  }

  void Mallocator::deallocate(Block blk) noexcept
  {
    std::free(blk.ptr());
  }
} // namespace stdcolt::alloc
