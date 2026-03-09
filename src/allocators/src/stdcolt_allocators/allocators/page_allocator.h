/*****************************************************************/ /**
 * @file   page_allocator.h
 * @brief  Contains `PageAllocator`.
 * 
 * @author Raphael Dib Nehme
 * @date   March 2026
 *********************************************************************/
#ifndef __HG_STDCOLT_ALLOCATORS_PAGE_ALLOCATOR
#define __HG_STDCOLT_ALLOCATORS_PAGE_ALLOCATOR

#include <stdcolt_allocators/block.h>
#include <stdcolt_allocators/allocator.h>
#include <stdcolt_allocators_export.h>

namespace stdcolt::alloc
{
  /// @brief Page allocator, allocates whole pages of memory at a time.
  /// This allocator is guaranteed stateless: constructor/destructor
  /// are not required to be called.
  struct PageAllocator
  {
    /// @brief The minimum page size guaranteed by all supported OSes.
    static constexpr size_t MIN_PAGE_SIZE = 4096;

    STDCOLT_ALLOCATORS_EXPORT
    /// @brief The runtime minimum page size on the current OS.
    static size_t RUNTIME_PAGE_SIZE;

    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe      = true,
        .is_fallible         = true,
        .is_nothrow_fallible = true,
        .returns_exact_size  = false,
        .alignment           = MIN_PAGE_SIZE,
    };

    STDCOLT_ALLOCATORS_EXPORT
    Block allocate(Layout request) noexcept;

    STDCOLT_ALLOCATORS_EXPORT
    void deallocate(Block blk) noexcept;
  };
  static_assert(IsAllocator<PageAllocator>);
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_PAGE_ALLOCATOR
