/*****************************************************************/ /**
 * @file   allocator.h
 * @brief  Contains allocator recipes (to type erase allocators).
 * @author Raphael Dib Nehme
 * @date   December 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_EXT_RUNTIME_TYPE_ALLOCATOR
#define __HG_STDCOLT_EXT_RUNTIME_TYPE_ALLOCATOR

#include <cstdint>
#include <concepts>
#include <type_traits>
#include <stdcolt_allocators/allocator.h>
#include <stdcolt_runtime_type_export.h>

namespace stdcolt::ext::rt
{
  /// @brief Allocated block.
  /// The exact same block obtained on allocation must be passed on deallocation.
  struct Block
  {
    /// @brief The pointer to the block or nullptr
    void* ptr = nullptr;
    /// @brief The size of the allocation
    uint64_t size = 0;
  };

  /// @brief Type erased allocator.
  struct RecipeAllocator
  {
    /// @brief Size of the resulting allocator state (may be zero)
    uint32_t allocator_sizeof;
    /// @brief The alignment of the allocator state (>= 1)
    uint32_t allocator_alignof;
    /// @brief Constructs the allocator.
    /// First argument, the storage of size `allocator_sizeof`.
    /// Returns 0 on success.
    int32_t (*allocator_construct)(void*) noexcept;
    /// @brief Destructs the allocator.
    /// First argument, the storage passed to `allocator_construct`.
    /// @pre May only be called if `allocator_construct` returned 0.
    void (*allocator_destruct)(void*) noexcept;
    /// @brief Allocates using the allocator.
    /// First argument, the storage passed to `allocator_construct`.
    /// Second argument, the size of the allocation in bytes.
    /// Third argument, the alignment of the allocation in bytes (>= 0).
    /// @pre May only be called if `allocator_construct` returned 0.
    Block (*allocator_alloc)(void*, uint64_t, uint64_t) noexcept;
    /// @brief Deallocates a block obtained using `allocator_alloc`.
    /// First argument, the storage passed to `allocator_construct`.
    /// Second argument, the block obtained from `allocator_alloc`.
    /// @pre May only be called if `allocator_construct` returned 0.
    void (*allocator_dealloc)(void*, Block) noexcept;

    /// @brief Converts a `stdcolt` allocator to an allocator recipe
    /// @tparam ALLOC The allocator to use
    /// @return RecipeAllocator representing the allocator
    template<alloc::IsAllocator ALLOC>
      requires std::is_default_constructible_v<ALLOC>
    static RecipeAllocator to_recipe() noexcept;
  };

  /// @brief Per-VTable allocator data
  struct Allocator
  {
    /// @brief Pointer to the allocator storage
    void* state;
    /// @brief Allocates using the allocator.
    /// First argument, the storage passed to `allocator_construct`.
    /// Second argument, the size of the allocation in bytes.
    /// Third argument, the alignment of the allocation in bytes (>= 0).
    /// @pre May only be called if `allocator_construct` returned 0.
    Block (*allocator_alloc)(void*, uint64_t, uint64_t) noexcept;
    /// @brief Deallocates a block obtained using `allocator_alloc`.
    /// First argument, the storage passed to `allocator_construct`.
    /// Second argument, the block obtained from `allocator_alloc`.
    /// @pre May only be called if `allocator_construct` returned 0.
    void (*allocator_dealloc)(void*, Block) noexcept;
    /// @brief Destructs the allocator.
    /// First argument, the storage passed to `allocator_construct`.
    /// @pre May only be called if `allocator_construct` returned 0.
    void (*allocator_destruct)(void*) noexcept;
  };

  /// @brief Returns the default allocator used by the library.
  /// The allocator returned supports extended alignment.
  /// @return The default allocator
  STDCOLT_RUNTIME_TYPE_EXPORT
  RecipeAllocator default_allocator() noexcept;

  template<alloc::IsAllocator ALLOC>
    requires std::is_default_constructible_v<ALLOC>
  RecipeAllocator RecipeAllocator::to_recipe() noexcept
  {
    static_assert(sizeof(ALLOC) < (1uLL << 32uLL));
    static_assert(alignof(ALLOC) < (1uLL << 32uLL));
    // TODO: add support for zero sized allocators (stateless)
    return {
        sizeof(ALLOC),
        alignof(ALLOC),
        +[](void* storage) noexcept
        {
          STDCOLT_assert(storage != nullptr, "pointer may not be null");
          try
          {
            new (storage) ALLOC();
            return 0;
          }
          catch (...)
          {
            return -1;
          }
        },
        +[](void* storage) noexcept
        {
          STDCOLT_assert(storage != nullptr, "pointer may not be null");
          try
          {
            ((ALLOC*)storage)->~ALLOC();
          }
          catch (...)
          {
            // swallow...
          }
        },
        +[](void* storage, uint64_t size, uint64_t align) noexcept
        {
          STDCOLT_assert(storage != nullptr, "pointer may not be null");
          try
          {
            auto blk = ((ALLOC*)storage)->allocate(alloc::Layout(size, align));
            return Block{blk.ptr(), blk.size()};
          }
          catch (...)
          {
            return Block{};
          }
        },
        +[](void* storage, Block block) noexcept
        {
          STDCOLT_assert(storage != nullptr, "pointer may not be null");
          if (block.ptr == nullptr)
            return;
          // already noexcept
          ((ALLOC*)storage)->deallocate(alloc::Block(block.ptr, block.size));
        }};
  }
} // namespace stdcolt::ext::rt

#endif // !__HG_STDCOLT_EXT_RUNTIME_TYPE_ALLOCATOR
