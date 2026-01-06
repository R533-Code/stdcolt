/*****************************************************************/ /**
 * @file   allocator.h
 * @brief  Contains allocator recipes (to type erase allocators).
 * This header is suitable to be consumed by C compilers.
 * @author Raphael Dib Nehme
 * @date   December 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_EXT_RUNTIME_TYPE_ALLOCATOR
#define __HG_STDCOLT_EXT_RUNTIME_TYPE_ALLOCATOR

#include <stdint.h>
#include <stdcolt_runtime_type_export.h>

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

  /// @brief Allocated block.
  /// The exact same block obtained on allocation must be passed on deallocation.
  typedef struct
  {
    /// @brief The pointer to the block or nullptr
    void* ptr;
    /// @brief The size of the allocation
    uint64_t size;
  } stdcolt_ext_rt_Block;

  /// @brief Type erased allocator.
  typedef struct
  {
    /// @brief Size of the resulting allocator state (may be zero)
    uint32_t allocator_sizeof;
    /// @brief The alignment of the allocator state (>= 1)
    uint32_t allocator_alignof;
    /// @brief Constructs the allocator.
    /// First argument, the storage of size `allocator_sizeof`.
    /// Returns 0 on success.
    int32_t (*allocator_construct)(void*);
    /// @brief Destructs the allocator.
    /// First argument, the storage passed to `allocator_construct`.
    /// @pre May only be called if `allocator_construct` returned 0.
    void (*allocator_destruct)(void*);
    /// @brief Allocates using the allocator.
    /// First argument, the storage passed to `allocator_construct`.
    /// Second argument, the size of the allocation in bytes.
    /// Third argument, the alignment of the allocation in bytes (>= 0).
    /// @pre May only be called if `allocator_construct` returned 0.
    stdcolt_ext_rt_Block (*allocator_alloc)(void*, uint64_t, uint64_t);
    /// @brief Deallocates a block obtained using `allocator_alloc`.
    /// First argument, the storage passed to `allocator_construct`.
    /// Second argument, the block obtained from `allocator_alloc`.
    /// @pre May only be called if `allocator_construct` returned 0.
    void (*allocator_dealloc)(void*, const stdcolt_ext_rt_Block*);
  } stdcolt_ext_rt_RecipeAllocator;

  /// @brief Per-VTable allocator data
  typedef struct
  {
    /// @brief Pointer to the allocator storage
    void* state;
    /// @brief Allocates using the allocator.
    /// First argument, the storage passed to `allocator_construct`.
    /// Second argument, the size of the allocation in bytes.
    /// Third argument, the alignment of the allocation in bytes (>= 0).
    /// @pre May only be called if `allocator_construct` returned 0.
    stdcolt_ext_rt_Block (*allocator_alloc)(void*, uint64_t, uint64_t);
    /// @brief Deallocates a block obtained using `allocator_alloc`.
    /// First argument, the storage passed to `allocator_construct`.
    /// Second argument, the block obtained from `allocator_alloc`.
    /// @pre May only be called if `allocator_construct` returned 0.
    void (*allocator_dealloc)(void*, const stdcolt_ext_rt_Block*);
    /// @brief Destructs the allocator.
    /// First argument, the storage passed to `allocator_construct`.
    /// @pre May only be called if `allocator_construct` returned 0.
    void (*allocator_destruct)(void*);
  } stdcolt_ext_rt_Allocator;

  /// @brief Returns the default allocator used by the library.
  /// The allocator returned supports extended alignment.
  /// @return The default allocator
  STDCOLT_RUNTIME_TYPE_EXPORT
  stdcolt_ext_rt_RecipeAllocator stdcolt_ext_rt_default_allocator();

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !__HG_STDCOLT_EXT_RUNTIME_TYPE_ALLOCATOR
