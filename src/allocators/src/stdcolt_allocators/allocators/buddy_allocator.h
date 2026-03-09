/*****************************************************************/ /**
 * @file   buddy_allocator.h
 * @brief  Contains `BuddyAllocatorMT`.
 * 
 * @author Raphael Dib Nehme
 * @date   March 2026
 *********************************************************************/
#ifndef __HG_STDCOLT_ALLOCATORS_BUDDY_ALLOCATOR
#define __HG_STDCOLT_ALLOCATORS_BUDDY_ALLOCATOR

#include <limits>
#include <mutex>
#include <stdcolt_allocators/block.h>
#include <stdcolt_allocators/allocator.h>

namespace stdcolt::alloc
{
  template<size_t SIZE, size_t MIN_BLOCK, IsAllocator ALLOCATOR>
  class BuddyAllocatorMT : private ALLOCATOR
  {
    static_assert(is_power_of_2(SIZE), "SIZE must be a power of 2");
    static_assert(is_power_of_2(MIN_BLOCK), "MIN_BLOCK must be a power of 2");
    static_assert(SIZE > MIN_BLOCK, "SIZE must be > MIN_BLOCK");
    static_assert(
        ALLOCATOR::allocator_info.is_thread_safe, "ALLOCATOR must be thread safe");
    static_assert(
        MIN_BLOCK >= ALLOCATOR::allocator_info.alignment,
        "MIN_BLOCK must be >= ALIGN_AS to guarantee alignment of every block");

    static constexpr size_t ALIGN_AS   = ALLOCATOR::allocator_info.alignment;
    static constexpr size_t NUM_LEVELS = std::bit_width(SIZE / MIN_BLOCK);

    struct FreeNode
    {
      FreeNode* prev;
      FreeNode* next;
    };
    static_assert(
        MIN_BLOCK >= sizeof(FreeNode), "MIN_BLOCK must be >= sizeof(FreeNode)");
    static_assert(
        ALIGN_AS >= alignof(FreeNode),
        "ALLOCATOR alignment must be >= alignof(FreeNode)");

    static constexpr size_t BITMAP_BITS  = SIZE / MIN_BLOCK;
    static constexpr size_t BITMAP_WORDS = (BITMAP_BITS + 63) / 64;

    // --- Compile-time helpers ---

    static constexpr size_t block_size_at(size_t l) noexcept
    {
      return MIN_BLOCK << l;
    }

    static constexpr size_t level_for_size(size_t size) noexcept
    {
      const size_t clamped = std::max(size, MIN_BLOCK);
      const size_t rounded = std::bit_ceil(clamped);
      return std::bit_width(rounded / MIN_BLOCK) - 1;
    }

    static constexpr size_t level_of_block(size_t size) noexcept
    {
      return std::bit_width(size / MIN_BLOCK) - 1;
    }

    // Offset of the first bitmap bit for level l.
    // Level l has SIZE/(MIN_BLOCK<<(l+1)) buddy pairs.
    static constexpr size_t bitmap_offset(size_t l) noexcept
    {
      return SIZE / MIN_BLOCK - SIZE / (MIN_BLOCK << l);
    }

    // --- Runtime helpers ---

    size_t block_index(const void* ptr, size_t l) const noexcept
    {
      return (static_cast<const char*>(ptr) - static_cast<const char*>(_slab.ptr()))
             / block_size_at(l);
    }

    void* buddy_of(void* ptr, size_t l) const noexcept
    {
      return static_cast<char*>(_slab.ptr())
             + (block_index(ptr, l) ^ size_t(1)) * block_size_at(l);
    }

    // Toggle the buddy-pair bit for ptr at level l.
    // Bit invariant: 1 if exactly one block of the pair is in-use, 0 otherwise.
    // Returns true if the new bit value is 1.
    bool flip_buddy_bit(const void* ptr, size_t l) noexcept
    {
      const size_t bit    = bitmap_offset(l) + block_index(ptr, l) / 2;
      const size_t word   = bit / 64;
      const size_t shift  = bit % 64;
      const uint64_t mask = uint64_t(1) << shift;
      _bitmap[word] ^= mask;
      return (_bitmap[word] & mask) != 0;
    }

    // --- Free list operations ---

    void push_free(void* ptr, size_t l) noexcept
    {
      auto* node = static_cast<FreeNode*>(ptr);
      node->prev = nullptr;
      node->next = _free_lists[l];
      if (_free_lists[l])
        _free_lists[l]->prev = node;
      _free_lists[l] = node;
    }

    void* pop_free(size_t l) noexcept
    {
      FreeNode* node = _free_lists[l];
      if (!node)
        return nullptr;
      _free_lists[l] = node->next;
      if (_free_lists[l])
        _free_lists[l]->prev = nullptr;
      return node;
    }

    void remove_free(void* ptr, size_t l) noexcept
    {
      auto* node = static_cast<FreeNode*>(ptr);
      if (node->prev)
        node->prev->next = node->next;
      else
        _free_lists[l] = node->next;
      if (node->next)
        node->next->prev = node->prev;
    }

    // --- Internal helpers (always called under _mutex) ---

    Block allocate_impl(size_t min_level) noexcept
    {
      for (size_t found_level = min_level; found_level < NUM_LEVELS; ++found_level)
      {
        void* ptr = pop_free(found_level);
        if (!ptr)
          continue;

        // flip at found_level: ptr transitions from free to in-use.
        // top level (NUM_LEVELS-1) has no buddy, skip.
        if (found_level < NUM_LEVELS - 1)
          flip_buddy_bit(ptr, found_level);

        // split downward from found_level to min_level.
        // at each level: push right buddy as free, flip ptr as it becomes
        // in-use at the next level down. sl-1 is always < NUM_LEVELS-1 here.
        for (size_t sl = found_level; sl > min_level; --sl)
        {
          void* buddy = static_cast<char*>(ptr) + block_size_at(sl - 1);
          push_free(buddy, sl - 1);
          flip_buddy_bit(ptr, sl - 1);
        }

        return {ptr, block_size_at(min_level)};
      }
      return nullblock;
    }

    void deallocate_impl(void* ptr, size_t l) noexcept
    {
      while (l < NUM_LEVELS - 1)
      {
        // flip: ptr transitions from in-use to free.
        // new=1: buddy is in-use, cannot coalesce.
        // new=0: buddy is free, coalesce upward.
        if (flip_buddy_bit(ptr, l))
        {
          push_free(ptr, l);
          return;
        }
        void* buddy = buddy_of(ptr, l);
        remove_free(buddy, l);
        ptr = (ptr < buddy) ? ptr : buddy;
        ++l;
      }
      // top level: no buddy, just push
      push_free(ptr, l);
    }

    // --- State ---

    Block _slab                       = nullblock;
    FreeNode* _free_lists[NUM_LEVELS] = {};
    uint64_t _bitmap[BITMAP_WORDS]    = {};
    std::mutex _mutex;

  public:
    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe      = true,
        .is_fallible         = true,
        .is_nothrow_fallible = true,
        .returns_exact_size  = false,
        .alignment           = ALIGN_AS,
    };

    template<typename... Ts>
    explicit BuddyAllocatorMT(Ts&&... args)
      requires std::is_constructible_v<ALLOCATOR, Ts...>
        : ALLOCATOR(std::forward<Ts>(args)...)
    {
      _slab = ALLOCATOR::allocate(Layout{SIZE, ALIGN_AS});
      if (_slab != nullblock)
        push_free(_slab.ptr(), NUM_LEVELS - 1);
    }

    BuddyAllocatorMT(const BuddyAllocatorMT&)            = delete;
    BuddyAllocatorMT& operator=(const BuddyAllocatorMT&) = delete;
    BuddyAllocatorMT(BuddyAllocatorMT&&)                 = delete;
    BuddyAllocatorMT& operator=(BuddyAllocatorMT&&)      = delete;

    ~BuddyAllocatorMT() noexcept
    {
      if (_slab != nullblock)
        ALLOCATOR::deallocate(_slab);
    }

    Block allocate(Layout request) noexcept
    {
      if (_slab == nullblock || request.size() == 0 || request.size() > SIZE)
        return nullblock;
      if (request.align() > ALIGN_AS)
        return nullblock;

      const size_t l = level_for_size(request.size());

      std::lock_guard lock{_mutex};
      return allocate_impl(l);
    }

    void deallocate(Block blk) noexcept
    {
      STDCOLT_pre(owns(blk), "received non-owned block");

      std::lock_guard lock{_mutex};
      deallocate_impl(blk.ptr(), level_of_block(blk.size()));
    }

    bool owns(Block blk) const noexcept
    {
      if (_slab == nullblock)
        return false;

      const size_t sz = blk.size();
      if (!is_power_of_2(sz) || sz < MIN_BLOCK || sz > SIZE)
        return false;

      const auto addr  = reinterpret_cast<std::uintptr_t>(blk.ptr());
      const auto start = reinterpret_cast<std::uintptr_t>(_slab.ptr());
      const auto end   = start + SIZE;

      if (addr < start || addr > end)
        return false;
      if (sz > end - addr)
        return false;

      const size_t offset = static_cast<size_t>(addr - start);
      return offset % sz == 0;
    }
  };
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_BUDDY_ALLOCATOR
