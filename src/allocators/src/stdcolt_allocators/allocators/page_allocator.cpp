/*****************************************************************/ /**
 * @file   page_allocator.cpp
 * @brief  Contains the implementation of `page_allocator.h`.
 * 
 * @author Raphael Dib Nehme
 * @date   March 2026
 *********************************************************************/
#include <stdcolt_allocators/allocators/page_allocator.h>

#ifdef _WIN32
  #define NOMINMAX
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <sys/mman.h>
  #include <unistd.h>
#endif

namespace stdcolt::alloc
{
  static size_t align_up_to(size_t n, size_t align) noexcept
  {
    return (n + align - 1) / align * align;
  }

  size_t PageAllocator::RUNTIME_PAGE_SIZE = []()
  {
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return static_cast<size_t>(si.dwPageSize);
#else
    return static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif
  }();

  Block PageAllocator::allocate(Layout request) noexcept
  {
    if (request.size() == 0)
      return nullblock;
    const size_t page_size = RUNTIME_PAGE_SIZE;
    if (request.align() > page_size)
      return nullblock;

    const size_t size = align_up_to(request.size(), page_size);

#if defined(_WIN32)
    void* ptr =
        VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (ptr == nullptr)
      return nullblock;
#else
    void* ptr = mmap(
        nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
      return nullblock;
#endif
    return {ptr, size};
  }

  void PageAllocator::deallocate(Block blk) noexcept
  {
#if defined(_WIN32)
    VirtualFree(blk.ptr(), 0, MEM_RELEASE);
#else
    munmap(blk.ptr(), blk.size());
#endif
  }
} // namespace stdcolt::alloc
