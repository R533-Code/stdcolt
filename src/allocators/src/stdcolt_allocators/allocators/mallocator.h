/*****************************************************************/ /**
 * @file   mallocator.h
 * @brief  Contains `Mallocator`.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_ALLOCATORS_MALLOCATOR
#define __HG_STDCOLT_ALLOCATORS_MALLOCATOR

#include <stdcolt_allocators_export.h>
#include <stdcolt_allocators/allocator.h>

namespace stdcolt::alloc
{
  /// @brief Allocator wrapper over `malloc` and `free`
  struct Mallocator
  {
    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe      = true,
        .is_fallible         = true,
        .is_nothrow_fallible = true,
        .returns_exact_size  = true,
    };

    STDCOLT_ALLOCATORS_EXPORT
    /// @brief Allocates a block using `malloc`
    /// @param size The size of the block
    /// @return The block or nullblock on failure
    Block allocate(size_t size) noexcept;

    STDCOLT_ALLOCATORS_EXPORT
    /// @brief Deallocates a block
    /// @param blk The block to deallocate
    void deallocate(Block blk) noexcept;
  };
  static_assert(IsAllocator<Mallocator>);
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_MALLOCATOR
