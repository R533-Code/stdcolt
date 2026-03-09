/*****************************************************************/ /**
 * @file   pool_allocator.h
 * @brief  Contains `PoolAllocator` and `PoolAllocatorMT`.
 * 
 * @author Raphael Dib Nehme
 * @date   March 2026
 *********************************************************************/
#ifndef __HG_STDCOLT_ALLOCATORS_POOL_ALLOCATOR
#define __HG_STDCOLT_ALLOCATORS_POOL_ALLOCATOR

#include <atomic>
#include <limits>
#include <stdcolt_allocators/block.h>
#include <stdcolt_allocators/allocator.h>

namespace stdcolt::alloc
{
  template<size_t BLOCK_SIZE, size_t BLOCK_COUNT, IsAllocator ALLOCATOR>
  class PoolAllocator : private ALLOCATOR
  {
    static constexpr size_t ACTUAL_BLOCK_SIZE =
        align_up<ALLOCATOR::allocator_info.alignment>(BLOCK_SIZE);

    static_assert(BLOCK_COUNT > 0, "BLOCK_COUNT must be > 0");
    static_assert(
        BLOCK_COUNT <= std::numeric_limits<uint32_t>::max(),
        "BLOCK_COUNT must fit in uint32_t");

    struct Node
    {
      Node* next;
    };
    static_assert(
        ACTUAL_BLOCK_SIZE >= sizeof(Node),
        "BLOCK_SIZE (after alignment rounding) must be >= sizeof(void*)");

    Block _slab      = nullblock;
    Node* _free_list = nullptr;

    void init() noexcept
    {
      char* ptr = static_cast<char*>(_slab.ptr());
      for (size_t i = 0; i < BLOCK_COUNT - 1; ++i)
      {
        auto* node = reinterpret_cast<Node*>(ptr + i * ACTUAL_BLOCK_SIZE);
        node->next = reinterpret_cast<Node*>(ptr + (i + 1) * ACTUAL_BLOCK_SIZE);
      }
      reinterpret_cast<Node*>(ptr + (BLOCK_COUNT - 1) * ACTUAL_BLOCK_SIZE)->next =
          nullptr;
      _free_list = reinterpret_cast<Node*>(ptr);
    }

  public:
    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe      = false,
        .is_fallible         = true,
        .is_nothrow_fallible = true,
        .returns_exact_size  = true,
        .alignment           = ALLOCATOR::allocator_info.alignment,
    };

    template<typename... Ts>
    explicit PoolAllocator(Ts&&... args)
      requires std::is_constructible_v<ALLOCATOR, Ts...>
        : ALLOCATOR(std::forward<Ts>(args)...)
    {
      _slab = ALLOCATOR::allocate(Layout{
          ACTUAL_BLOCK_SIZE * BLOCK_COUNT, ALLOCATOR::allocator_info.alignment});
      if (_slab != nullblock)
        init();
    }

    PoolAllocator(const PoolAllocator&)            = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    PoolAllocator(PoolAllocator&& other) noexcept(
        std::is_nothrow_move_constructible_v<ALLOCATOR>)
      requires std::is_move_constructible_v<ALLOCATOR>
        : ALLOCATOR(std::move(static_cast<ALLOCATOR&>(other)))
        , _slab(std::exchange(other._slab, nullblock))
        , _free_list(std::exchange(other._free_list, nullptr))
    {
    }

    PoolAllocator& operator=(PoolAllocator&& other) noexcept(
        std::is_nothrow_move_assignable_v<ALLOCATOR>)
      requires std::is_move_assignable_v<ALLOCATOR>
    {
      if (this == &other)
        return *this;
      if (_slab != nullblock)
        ALLOCATOR::deallocate(_slab);
      static_cast<ALLOCATOR&>(*this) = std::move(static_cast<ALLOCATOR&>(other));
      _slab                          = std::exchange(other._slab, nullblock);
      _free_list                     = std::exchange(other._free_list, nullptr);
      return *this;
    }

    ~PoolAllocator() noexcept
    {
      if (_slab != nullblock)
        ALLOCATOR::deallocate(_slab);
    }

    Block allocate(Layout request) noexcept
    {
      if (request.size() > ACTUAL_BLOCK_SIZE
          || request.align() > ALLOCATOR::allocator_info.alignment
          || _free_list == nullptr)
        return nullblock;

      Node* node = _free_list;
      _free_list = node->next;
      return {static_cast<void*>(node), ACTUAL_BLOCK_SIZE};
    }

    void deallocate(Block blk) noexcept
    {
      STDCOLT_pre(owns(blk), "received non-owned block");
      auto* node = static_cast<Node*>(blk.ptr());
      node->next = _free_list;
      _free_list = node;
    }

    bool owns(Block blk) const noexcept
    {
      if (_slab == nullblock)
        return false;
      auto* p     = static_cast<char*>(blk.ptr());
      auto* start = static_cast<char*>(_slab.ptr());
      return p >= start && p < start + ACTUAL_BLOCK_SIZE * BLOCK_COUNT;
    }
  };

  template<size_t BLOCK_SIZE, size_t BLOCK_COUNT, IsAllocator ALLOCATOR>
    requires(ALLOCATOR::allocator_info.is_thread_safe)
  class PoolAllocatorMT : private ALLOCATOR
  {
    static constexpr size_t ACTUAL_BLOCK_SIZE =
        align_up<ALLOCATOR::allocator_info.alignment>(BLOCK_SIZE);

    static_assert(BLOCK_COUNT > 0, "BLOCK_COUNT must be > 0");
    static_assert(
        BLOCK_COUNT < std::numeric_limits<uint32_t>::max(),
        "BLOCK_COUNT must fit in uint32_t");
    static_assert(
        ACTUAL_BLOCK_SIZE >= sizeof(uint32_t),
        "BLOCK_SIZE must be >= sizeof(uint32_t) after alignment rounding");
    static_assert(
        ALLOCATOR::allocator_info.alignment
            >= std::atomic_ref<uint32_t>::required_alignment,
        "ALLOCATOR alignment must satisfy atomic_ref<uint32_t>::required_alignment");

    static constexpr uint32_t NULL_INDEX = std::numeric_limits<uint32_t>::max();

    struct alignas(8) Head
    {
      uint32_t index;
      uint32_t version;
    };

    static Head unpack(uint64_t v) noexcept
    {
      Head h;
      std::memcpy(&h, &v, 8);
      return h;
    }
    static uint64_t pack(Head h) noexcept
    {
      uint64_t v;
      std::memcpy(&v, &h, 8);
      return v;
    }

    std::atomic_ref<uint32_t> next_of(uint32_t index) noexcept
    {
      return std::atomic_ref<uint32_t>(*reinterpret_cast<uint32_t*>(
          static_cast<char*>(_slab.ptr())
          + static_cast<size_t>(index) * ACTUAL_BLOCK_SIZE));
    }

    Block _slab = nullblock;
    std::atomic<uint64_t> _head{pack({NULL_INDEX, 0})};

    void init() noexcept
    {
      for (uint32_t i = 0; i < static_cast<uint32_t>(BLOCK_COUNT) - 1; ++i)
        next_of(i).store(i + 1, std::memory_order_relaxed);
      next_of(static_cast<uint32_t>(BLOCK_COUNT - 1))
          .store(NULL_INDEX, std::memory_order_relaxed);
      _head.store(pack({0, 0}), std::memory_order_relaxed);
    }

  public:
    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe      = true,
        .is_fallible         = true,
        .is_nothrow_fallible = true,
        .returns_exact_size  = true,
        .alignment           = ALLOCATOR::allocator_info.alignment,
    };

    template<typename... Ts>
    explicit PoolAllocatorMT(Ts&&... args)
      requires std::is_constructible_v<ALLOCATOR, Ts...>
        : ALLOCATOR(std::forward<Ts>(args)...)
    {
      _slab = ALLOCATOR::allocate(Layout{
          ACTUAL_BLOCK_SIZE * BLOCK_COUNT, ALLOCATOR::allocator_info.alignment});
      if (_slab != nullblock)
        init();
    }

    PoolAllocatorMT(const PoolAllocatorMT&)            = delete;
    PoolAllocatorMT& operator=(const PoolAllocatorMT&) = delete;
    PoolAllocatorMT(PoolAllocatorMT&&)                 = delete;
    PoolAllocatorMT& operator=(PoolAllocatorMT&&)      = delete;

    ~PoolAllocatorMT() noexcept
    {
      if (_slab != nullblock)
        ALLOCATOR::deallocate(_slab);
    }

    Block allocate(Layout request) noexcept
    {
      if (request.size() > ACTUAL_BLOCK_SIZE
          || request.align() > ALLOCATOR::allocator_info.alignment
          || _slab == nullblock)
        return nullblock;

      for (;;)
      {
        Head old = unpack(_head.load(std::memory_order_acquire));

        if (old.index == NULL_INDEX)
          return nullblock;

        const uint32_t next = next_of(old.index).load(std::memory_order_relaxed);
        const Head new_head = {next, old.version + 1};

        uint64_t expected = pack(old);
        if (_head.compare_exchange_weak(
                expected, pack(new_head), std::memory_order_relaxed,
                std::memory_order_relaxed))
          return {
              static_cast<char*>(_slab.ptr())
                  + static_cast<size_t>(old.index) * ACTUAL_BLOCK_SIZE,
              ACTUAL_BLOCK_SIZE};
      }
    }

    void deallocate(Block blk) noexcept
    {
      STDCOLT_pre(owns(blk), "received non-owned block");

      const uint32_t index = static_cast<uint32_t>(
          (static_cast<char*>(blk.ptr()) - static_cast<char*>(_slab.ptr()))
          / ACTUAL_BLOCK_SIZE);

      uint64_t old_val = _head.load(std::memory_order_relaxed);
      for (;;)
      {
        Head old = unpack(old_val);
        next_of(index).store(old.index, std::memory_order_relaxed);
        const Head new_head = {index, old.version + 1};

        if (_head.compare_exchange_weak(
                old_val, pack(new_head), std::memory_order_release,
                std::memory_order_relaxed))
          return;
      }
    }

    bool owns(Block blk) const noexcept
    {
      if (_slab == nullblock)
        return false;
      auto* p     = static_cast<char*>(blk.ptr());
      auto* start = static_cast<char*>(_slab.ptr());
      return p >= start && p < start + ACTUAL_BLOCK_SIZE * BLOCK_COUNT;
    }
  };
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_POOL_ALLOCATOR
