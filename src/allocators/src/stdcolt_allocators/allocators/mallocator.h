/*****************************************************************/ /**
 * @file   mallocator.h
 * @brief  Contains `Mallocator` and `AlignedMallocator`.
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
  /// @brief Allocator wrapper over `malloc` and `free`.
  /// This allocator does not support extended alignment, use `AlignedMallocator`
  /// for that.
  /// This allocator is guaranteed stateless: constructor/destructor
  /// are not required to be called.
  struct Mallocator
  {
    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe      = true,
        .is_fallible         = true,
        .is_nothrow_fallible = true,
        .returns_exact_size  = true,
        .alignment           = PREFERRED_ALIGNMENT,
    };

    STDCOLT_ALLOCATORS_EXPORT
    /// @brief Allocates a block using `malloc`
    /// @param request The allocation request
    /// @return The block or nullblock on failure
    Block allocate(Layout request) const noexcept;

    STDCOLT_ALLOCATORS_EXPORT
    /// @brief Deallocates a block
    /// @param blk The block to deallocate
    void deallocate(Block blk) const noexcept;
  };
  static_assert(IsAllocator<Mallocator>);

  /// @brief Allocator wrapper over aligned `malloc` and aligned `free`.
  /// This allocator supports extended alignment. If the underlying OS
  /// cannot allocate a block with that specific alignment, `nullblock`
  /// is returned.
  /// This allocator is guaranteed stateless: constructor/destructor
  /// are not required to be called.
  struct MallocatorAligned
  {
    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe      = true,
        .is_fallible         = true,
        .is_nothrow_fallible = true,
        .returns_exact_size  = true,
        .alignment           = PREFERRED_ALIGNMENT,
    };

    STDCOLT_ALLOCATORS_EXPORT
    /// @brief Allocates a block using `malloc`
    /// @param request The allocation request
    /// @return The block or nullblock on failure
    Block allocate(Layout request) const noexcept;

    STDCOLT_ALLOCATORS_EXPORT
    /// @brief Deallocates a block
    /// @param blk The block to deallocate
    void deallocate(Block blk) const noexcept;
  };
  static_assert(IsAllocator<Mallocator>);
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_MALLOCATOR
