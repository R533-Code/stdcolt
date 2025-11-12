/*****************************************************************/ /**
 * @file   block.h
 * @brief  Contains `Block`, the allocation unit of all allocators.
 * 
 * @author RPC
 * @date   November 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_ALLOCATORS_BLOCK
#define __HG_STDCOLT_ALLOCATORS_BLOCK

#include <cstdint>
#include <tuple>
#include <utility>

namespace stdcolt::alloc
{
  /// @brief Allocation unit, a pointer and size.
  class Block
  {
    /// @brief The pointer
    void* _ptr = nullptr;
    /// @brief The size of the allocation
    size_t _size = 0;

  public:
    /// @brief Constructs an empty block
    constexpr Block() noexcept = default;
    /// @brief Constructs a block
    /// @param ptr The pointer
    /// @param size The size of the allocation
    constexpr Block(void* ptr, size_t size) noexcept
        : _ptr(ptr)
        , _size((size_t)(ptr != nullptr) * size)
    {
    }

    /// @brief Returns the pointer to the block
    /// @return The pointer
    constexpr void* ptr() noexcept { return _ptr; }
    /// @brief Returns the pointer to the block
    /// @return The pointer
    constexpr const void* ptr() const noexcept { return _ptr; }
    /// @brief Returns the size of the block
    /// @return The size
    constexpr size_t size() const noexcept { return _size; }

    constexpr bool operator==(const Block&) const = default;
  };

  /// @brief Null block
  static inline constexpr Block nullblock = {};
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_BLOCK
