/*****************************************************************/ /**
 * @file   mallocator.cpp
 * @brief  Contains the implementation of `mallocator.h`.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#include <stdcolt_allocators/allocators/mallocator.h>
#include <cstddef>
#include <cstdlib>

#if defined(_WIN32)
  #include <malloc.h> // _aligned_malloc, _aligned_free
#else
  #include <errno.h>
  #include <stdlib.h> // posix_memalign, free
#endif

namespace stdcolt::alloc
{
  Block Mallocator::allocate(Layout request) const noexcept
  {
    if (request.align() > PREFERRED_ALIGNMENT)
      return nullblock;
    return {std::malloc(request.size()), request.size()};
  }

  void Mallocator::deallocate(Block blk) const noexcept
  {
    std::free(blk.ptr());
  }

  static void* aligned_malloc(size_t size, size_t alignment)
  {
    if (alignment < PREFERRED_ALIGNMENT)
      alignment = PREFERRED_ALIGNMENT;

#if defined(_WIN32)
    return _aligned_malloc(size, alignment);
#else
    void* p = nullptr;
    int rc  = posix_memalign(&p, alignment, size);

    if (rc != 0)
      return nullptr;
    return p;
#endif
  }

  static void aligned_free(void* ptr) noexcept
  {
    if (ptr == nullptr)
      return;

#if defined(_WIN32)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
  }

  Block MallocatorAligned::allocate(Layout request) const noexcept
  {
    return {aligned_malloc(request.size(), request.align()), request.size()};
  }

  void MallocatorAligned::deallocate(Block blk) const noexcept
  {
    aligned_free(blk.ptr());
  }
} // namespace stdcolt::alloc
