/*****************************************************************/ /**
 * @file   arena_allocator.h
 * @brief  Contains `ArenaAllocator`.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_ALLOCATORS_ARENA_ALLOCATOR
#define __HG_STDCOLT_ALLOCATORS_ARENA_ALLOCATOR

#include <stdcolt_allocators/allocator.h>
#include <atomic>

namespace stdcolt::alloc
{
  template<size_t SLAB_SIZE, IsAllocator ALLOCATOR>
  class ArenaAllocator : private ALLOCATOR
  {
    static constexpr size_t ALIGN_AS = ALLOCATOR::allocator_info.alignment;

    struct SlabHeader
    {
      SlabHeader* prev;
      size_t capacity; // usable bytes after header
    };

    static constexpr size_t HEADER_SIZE = align_up<ALIGN_AS>(sizeof(SlabHeader));

    static_assert(
        SLAB_SIZE > HEADER_SIZE,
        "SLAB_SIZE must be larger than the aligned slab header");
    static_assert(
        ALIGN_AS >= alignof(SlabHeader),
        "ALLOCATOR alignment must be >= alignof(SlabHeader)");

    char* usable_start(SlabHeader* hdr) const noexcept
    {
      return reinterpret_cast<char*>(hdr) + HEADER_SIZE;
    }
    const char* usable_start(const SlabHeader* hdr) const noexcept
    {
      return reinterpret_cast<const char*>(hdr) + HEADER_SIZE;
    }

    SlabHeader* alloc_slab(size_t min_total) noexcept(
        is_allocate_nothrow_v<ALLOCATOR>)
    {
      Block blk = ALLOCATOR::allocate(Layout{min_total, ALIGN_AS});
      if (blk == nullblock)
        return nullptr;
      auto* hdr     = static_cast<SlabHeader*>(blk.ptr());
      hdr->prev     = _current;
      hdr->capacity = blk.size() - HEADER_SIZE;
      return hdr;
    }

    SlabHeader* _current = nullptr;
    size_t _cursor       = 0; // byte offset into usable area of current slab

  public:
    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe      = false,
        .is_fallible         = ALLOCATOR::allocator_info.is_fallible,
        .is_nothrow_fallible = ALLOCATOR::allocator_info.is_nothrow_fallible,
        .returns_exact_size  = false,
        .alignment           = ALIGN_AS,
    };

    template<typename... Ts>
    explicit ArenaAllocator(Ts&&... args)
      requires std::is_constructible_v<ALLOCATOR, Ts...>
        : ALLOCATOR(std::forward<Ts>(args)...)
    {
    }

    ArenaAllocator(const ArenaAllocator&)            = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;
    ArenaAllocator(ArenaAllocator&&)                 = delete;
    ArenaAllocator& operator=(ArenaAllocator&&)      = delete;

    ~ArenaAllocator() noexcept { deallocate_all(); }

    Block allocate(Layout request) noexcept(is_allocate_nothrow_v<ALLOCATOR>)
    {
      if (request.size() == 0)
        return nullblock;
      if (request.align() > ALIGN_AS)
        return nullblock;

      const size_t size = align_up<ALIGN_AS>(request.size());

      if (_current && _cursor + size <= _current->capacity)
      {
        char* ptr = usable_start(_current) + _cursor;
        _cursor += size;
        return {ptr, size};
      }

      const size_t min_total = max_of(SLAB_SIZE, HEADER_SIZE + size);
      SlabHeader* slab       = alloc_slab(min_total);
      if (!slab)
        return nullblock;

      _current = slab;
      _cursor  = size;
      return {usable_start(slab), size};
    }

    /// @brief No-op. Arena only frees memory in deallocate_all.
    void deallocate(Block blk) noexcept
    {
      STDCOLT_pre(owns(blk), "received non-owned block");
    }

    bool owns(Block blk) const noexcept
    {
      const auto* p = static_cast<const char*>(blk.ptr());
      const auto s  = blk.size();

      for (const SlabHeader* slab = _current; slab; slab = slab->prev)
      {
        const char* start = usable_start(slab);
        const char* end   = start + slab->capacity;
        if (p >= start && p + s <= end)
          return true;
      }
      return false;
    }

    /// @brief Deallocates all slabs back to the upstream allocator.
    void deallocate_all() noexcept
    {
      SlabHeader* slab = _current;
      while (slab)
      {
        SlabHeader* prev = slab->prev;
        ALLOCATOR::deallocate({slab, HEADER_SIZE + slab->capacity});
        slab = prev;
      }
      _current = nullptr;
      _cursor  = 0;
    }
  };
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_ARENA_ALLOCATOR
