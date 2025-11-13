/*****************************************************************/ /**
 * @file   stack_allocator.h
 * @brief  Contains `StackAllocator`.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_ALLOCATORS_STACK_ALLOCATOR
#define __HG_STDCOLT_ALLOCATORS_STACK_ALLOCATOR

#include <stdcolt_allocators/allocator.h>
#include <atomic>

namespace stdcolt::alloc
{
  /// @brief Allocator that returns memory from the stack.
  /// @tparam SIZE The size of the buffer (automatically aligned to the alignment)
  /// @tparam ALIGN_AS The alignment of the allocations
  template<size_t SIZE, size_t ALIGN_AS = PREFERRED_ALIGNMENT>
  class StackAllocator
  {
    static_assert(ALIGN_AS != 0, "Alignment may not be zero!");
    static_assert(SIZE != 0, "Size may not be zero!");
    static_assert(is_power_of_2(ALIGN_AS), "Alignment must be power of 2!");
    static constexpr size_t BUFFER_SIZE = align_up<ALIGN_AS>(SIZE);

    alignas(ALIGN_AS) char _buffer[BUFFER_SIZE];
    size_t _size = 0;

  public:
    StackAllocator()                      = default;
    StackAllocator(StackAllocator&&)      = delete;
    StackAllocator(const StackAllocator&) = delete;

    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe      = false,
        .is_fallible         = true,
        .is_nothrow_fallible = true,
        .returns_exact_size  = false,
        .alignment           = ALIGN_AS,
    };

    /// @brief Allocates a block of memory
    /// @param request The allocation request
    /// @return Block or nullblock on failure
    constexpr Block allocate(Layout request) noexcept
    {
      if (request.align() > ALIGN_AS)
        return nullblock;

      auto size = align_up<ALIGN_AS>(request.size());
      if (_size + size > BUFFER_SIZE)
        return nullblock;
      Block ret = {_buffer + _size, size};
      _size += size;
      return ret;
    }
    /// @brief Deallocates a block
    /// @param blk The block
    /// @pre `blk` must be owned by the current allocator
    constexpr void deallocate(Block blk) noexcept
    {
      STDCOLT_pre(owns(blk), "received non-owned block");

      // we can only free a block if it was the last allocated one
      if (blk.ptr() + blk.size() == _buffer + _size)
        _size -= blk.size();
    }

    /// @brief Check if the stack allocators owns a block
    /// @param blk The memory block
    /// @return True if owned by the stack allocator
    constexpr bool owns(Block blk) const noexcept
    {
      auto* p = blk.ptr();
      auto s  = blk.size();
      return p >= _buffer && p + s <= _buffer + _size;
    }

    /// @brief Deallocates all the allocated memory.
    /// @warning This is unsafe!
    constexpr void deallocate_all() noexcept { _size = 0; }
  };
  static_assert(IsOwningAllocator<StackAllocator<8>>);

  /// @brief Thread safe allocator that returns memory from the stack.
  /// This allocator should rarely be used without a free-list: deallocation
  /// will most likely not free anything if other allocations have occurred
  /// in between, as only the most recently allocated block can be released.
  /// @tparam SIZE The size of the buffer (automatically aligned to the alignment)
  /// @tparam ALIGN_AS The alignment of the allocations
  template<size_t SIZE, size_t ALIGN_AS = PREFERRED_ALIGNMENT>
  class StackAllocatorMT
  {
    static_assert(ALIGN_AS != 0, "Alignment may not be zero!");
    static_assert(is_power_of_2(ALIGN_AS), "Alignment must be power of 2!");
    static constexpr size_t BUFFER_SIZE = align_up<ALIGN_AS>(SIZE);

    alignas(ALIGN_AS) char _buffer[BUFFER_SIZE];
    std::atomic<size_t> _size{0};

  public:
    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe      = true,
        .is_fallible         = true,
        .is_nothrow_fallible = true,
        .returns_exact_size  = false,
        .alignment           = PREFERRED_ALIGNMENT,
    };

    /// @brief Allocates a block of memory
    /// @param request The allocation request
    /// @return Block or nullblock on failure
    constexpr Block allocate(Layout request) noexcept
    {
      if (request.align() > ALIGN_AS)
        return nullblock;

      auto size = align_up<ALIGN_AS>(request.size());

      size_t old_size = _size.load(std::memory_order_relaxed);
      for (;;)
      {
        if (old_size + size > BUFFER_SIZE)
          return nullblock;

        size_t new_size = old_size + size;
        if (_size.compare_exchange_weak(
                old_size, new_size, std::memory_order_relaxed,
                std::memory_order_relaxed))
          return {_buffer + old_size, size};
      }
    }
    /// @brief Deallocates a block
    /// @param blk The block
    /// @pre `blk` must be owned by the current allocator
    constexpr void deallocate(Block blk) noexcept
    {
      STDCOLT_pre(owns(blk), "received non-owned block");

      auto* p = blk.ptr();
      auto s  = blk.size();

      const auto offset = static_cast<size_t>(p - _buffer);
      const size_t end  = offset + s;

      size_t curr = _size.load(std::memory_order_relaxed);
      for (;;)
      {
        // we can only free a block if it is currently the last allocated one
        if (curr != end)
          return;

        const size_t new_size = curr - s;
        if (_size.compare_exchange_weak(
                curr, new_size, std::memory_order_relaxed,
                std::memory_order_relaxed))
          return;
      }
    }

    /// @brief Check if the stack allocators owns a block
    /// @param blk The memory block
    /// @return True if owned by the stack allocator
    constexpr bool owns(Block blk) const noexcept
    {
      auto* p     = blk.ptr();
      auto s      = blk.size();
      size_t curr = _size.load(std::memory_order_relaxed);

      return p >= _buffer && p + s <= _buffer + curr;
    }

    /// @brief Deallocates all the allocated memory.
    /// @warning This is unsafe!
    constexpr void deallocate_all() noexcept
    {
      _size.store(0, std::memory_order_relaxed);
    }
  };
  static_assert(IsOwningAllocator<StackAllocatorMT<8>>);
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_STACK_ALLOCATOR
