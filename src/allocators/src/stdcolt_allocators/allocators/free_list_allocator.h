/*****************************************************************/ /**
 * @file   free_list_allocator.h
 * @brief  Contains `FreeListAllocator`.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_ALLOCATORS_FREE_LIST
#define __HG_STDCOLT_ALLOCATORS_FREE_LIST

#include <stdcolt_allocators/allocator.h>
#include <stdcolt_allocators/allocators/null_allocator.h>

namespace stdcolt::alloc
{
  namespace details
  {
    /// @brief Empty struct used for empty base optimization (EBO)
    struct FreeListEmpty
    {
    };

    /// @brief Struct to inherit from for free list size bookkeeping
    struct FreeListSize
    {
      /// @brief The number of elements in the free list
      size_t size = 0;

      constexpr FreeListSize() noexcept = default;

      // the free list shouldn't be copyable vvv
      constexpr FreeListSize(const FreeListSize&) noexcept            = delete;
      constexpr FreeListSize& operator=(const FreeListSize&) noexcept = delete;

      // the free list should be movable vvv
      constexpr FreeListSize(FreeListSize&& other) noexcept
          : size(std::exchange(other.size, 0))
      {
      }
      constexpr FreeListSize& operator=(FreeListSize&& other) noexcept
      {
        size = std::exchange(other.size, 0);
        return *this;
      }
    };
  } // namespace details

  /// @brief Free list allocator.
  /// A free list holds previously allocated blocks that have been returned
  /// by this allocator and are eligible for reuse. Instead of immediately
  /// returning such blocks to the underlying allocator, we store them in a
  /// singly linked list so that future allocations of compatible sizes can
  /// be satisfied without invoking the underlying allocator. This reduces
  /// allocation/deallocation overhead.
  /// This free list implementation is quite customizable: it allows
  /// selectively returning blocks to the underlying allocator if the block
  /// size is not in the range [MIN_BLOCK, MAX_BLOCK], or if the free list holds
  /// more elements than `MAX_FREE_LIST`.
  /// @tparam ALLOCATOR The underlying allocator
  /// @tparam MIN_BLOCK The minimum size of blocks to try to add to the free list
  /// @tparam MAX_BLOCK The maximum size of blocks to try to add to the free list
  /// @tparam MAX_FREE_LIST The maximum number of elements in the free list.
  /// If the list has a size of `MAX_FREE_LIST`, then blocks are returned directly
  /// to the underlying allocator. If `MAX_FREE_LIST` is set to the maximum value
  /// of `size_t`, then no counting will happen, and the size of the list is unbounded.
  /// @tparam TOLERATED_SIZE_DIFFERENCE_PERCENT Allowed oversize percentage for reusing a free-list block (0 = exact match).
  /// Maximum percentage by which a reused block may exceed the requested size.
  /// A value of 0 requires an exact-size match. Larger values allow slightly
  /// oversized blocks from the free list to be reused, reducing calls to the
  /// underlying allocator at the cost of potential internal fragmentation.
  /// @tparam FIRST_FIT Returns the first block that fits the requested allocation.
  /// This is only used when `TOLERATED_SIZE_DIFFERENCE_PERCENT` is not 0. Rather
  /// than searching in the complete free list for the best match, the first
  /// match is returned. This is faster at the cost of potential internal
  /// fragmentation.
  template<
      IsAllocator ALLOCATOR, size_t MIN_BLOCK, size_t MAX_BLOCK,
      size_t MAX_FREE_LIST                     = (size_t)-1,
      size_t TOLERATED_SIZE_DIFFERENCE_PERCENT = 0, bool FIRST_FIT = true>
  class FreeListAllocator
      : private ALLOCATOR
      , private std::conditional_t<
            MAX_FREE_LIST == (size_t)-1, details::FreeListEmpty,
            details::FreeListSize>
  {
    /// @brief If true, then we store the number of active element in the free-list.
    /// The size is accessible using `FREE_LIST_SIZE::size`.
    static constexpr bool HAS_MAX_SIZE = MAX_FREE_LIST != (size_t)-1;

    /// @brief Node for the linked list
    struct Node
    {
      /// @brief The size of the current block
      size_t size;
      /// @brief Pointer to the next block
      Node* next;
    };

    /// @brief Short-hand to access `size` when HAS_MAX_SIZE is true.
    using FREE_LIST_SIZE = std::conditional_t<
        !HAS_MAX_SIZE, details::FreeListEmpty, details::FreeListSize>;

    /// @brief Short-hand to access allocator information of the parent
    static constexpr AllocatorInfo inherited_alloc_info = ALLOCATOR::allocator_info;
    /// @brief Should the allocator only return block of the exact size requested?
    static constexpr bool FORCE_RETURN_EXACT =
        TOLERATED_SIZE_DIFFERENCE_PERCENT == 0;

    static_assert(
        MIN_BLOCK >= sizeof(Node), "MIN_BLOCK must be greater than sizeof(Block)!");
    static_assert(MAX_BLOCK >= MIN_BLOCK, "MAX_BLOCK must be >= MIN_BLOCK!");
    static_assert(
        inherited_alloc_info.alignment >= alignof(Node),
        "Alignment of ALLOCATOR must be >= alignof(Block)");
    static_assert(
        !FORCE_RETURN_EXACT || inherited_alloc_info.returns_exact_size,
        "(TOLERATED_SIZE_DIFFERENCE_PERCENT == 0) implies that "
        "ALLOCATOR::allocator_info.returns_exact_size");

    /// @brief The free-list
    Node* _free_list = nullptr;

    /// @brief Check if a block may be added to the free list.
    /// If `HAS_MAX_SIZE`, then the number of elements in the free list is
    /// also considered.
    /// @param block The block to check for
    /// @return True if the block may be added to the free list
    inline bool is_block_valid_for_free_list(Block block) const noexcept
    {
      if constexpr (HAS_MAX_SIZE)
      {
        return MIN_BLOCK <= block.size() && block.size() <= MAX_BLOCK
               && FREE_LIST_SIZE::size != MAX_FREE_LIST;
      }
      else
      {
        return MIN_BLOCK <= block.size() && block.size() <= MAX_BLOCK;
      }
    }

    inline void clear_free_list() noexcept
    {
      while (_free_list != nullptr)
      {
        Node* next = _free_list->next;
        ALLOCATOR::deallocate({static_cast<void*>(_free_list), _free_list->size});
        _free_list = next;
      }
      if constexpr (HAS_MAX_SIZE)
        FREE_LIST_SIZE::size = 0;
    }

  public:
    static constexpr AllocatorInfo allocator_info = {
        .is_thread_safe      = false,
        .is_fallible         = inherited_alloc_info.is_fallible,
        .is_nothrow_fallible = inherited_alloc_info.is_nothrow_fallible,
        .returns_exact_size  = FORCE_RETURN_EXACT,
        .alignment           = inherited_alloc_info.alignment,
    };

    FreeListAllocator(const FreeListAllocator&)            = delete;
    FreeListAllocator& operator=(const FreeListAllocator&) = delete;

    template<typename... Ts>
    constexpr FreeListAllocator(Ts&&... vals)
      requires std::is_constructible_v<ALLOCATOR, Ts...>
        : ALLOCATOR(std::forward<Ts>(vals)...)
    {
    }

    constexpr FreeListAllocator(FreeListAllocator&& other) noexcept(
        std::is_nothrow_move_constructible_v<ALLOCATOR>)
      requires std::is_move_constructible_v<ALLOCATOR>
        : ALLOCATOR(std::move(static_cast<ALLOCATOR&>(other)))
        , FREE_LIST_SIZE(std::move(static_cast<FREE_LIST_SIZE&>(other)))
        , _free_list(std::exchange(other._free_list, nullptr))
    {
    }

    constexpr FreeListAllocator& operator=(FreeListAllocator&& other) noexcept(
        std::is_nothrow_move_assignable_v<ALLOCATOR>)
      requires std::is_move_assignable_v<ALLOCATOR>
    {
      if (this == &other)
        return *this;
      // we need to return the free list to the underlying allocator
      // to be able to steal the free list of the other.
      // a swap wouldn't really be safe except when the move assignment
      // operator of the allocator is also swapping. as we can't
      // know, play it safe:
      clear_free_list();

      *static_cast<ALLOCATOR*>(this) = std::move(static_cast<ALLOCATOR&>(other));
      *static_cast<FREE_LIST_SIZE*>(this) =
          std::move(static_cast<FREE_LIST_SIZE&>(other));
      _free_list = std::exchange(other._free_list, nullptr);

      return *this;
    }

    Block allocate(Layout request) noexcept(is_allocate_nothrow_v<ALLOCATOR>)
    {
      const size_t requested_size = request.size();

      // If we can't possibly have a block for this size, or the list is empty,
      // just forward to the underlying allocator.
      if (requested_size < MIN_BLOCK || requested_size > MAX_BLOCK
          || _free_list == nullptr)
        return ALLOCATOR::allocate(request);

      Node* prev      = nullptr;
      Node* current   = _free_list;
      Node* best_prev = nullptr;
      Node* best      = nullptr;

      if constexpr (FORCE_RETURN_EXACT)
      {
        // Need an exact match:
        while (current != nullptr)
        {
          if (current->size == requested_size)
          {
            best      = current;
            best_prev = prev;
            break;
          }
          prev    = current;
          current = current->next;
        }
      }
      else
      {
        const size_t max_acceptable_size =
            requested_size
            + (requested_size * TOLERATED_SIZE_DIFFERENCE_PERCENT) / 100;

        while (current != nullptr)
        {
          if (current->size >= requested_size
              && current->size <= max_acceptable_size)
          {
            if (best == nullptr || current->size < best->size)
            {
              best      = current;
              best_prev = prev;
              if constexpr (FIRST_FIT)
                break;
            }
          }

          prev    = current;
          current = current->next;
        }
      }

      if (best == nullptr)
        return ALLOCATOR::allocate(request);

      if (best_prev != nullptr)
        best_prev->next = best->next;
      else
        _free_list = best->next;

      if constexpr (HAS_MAX_SIZE)
        --FREE_LIST_SIZE::size;

      return {static_cast<void*>(best), best->size};
    }

    void deallocate(Block block) noexcept
    {
      if (is_block_valid_for_free_list(block))
      {
        _free_list = new (block.ptr()) Node{block.size(), _free_list};
        if constexpr (HAS_MAX_SIZE)
          ++FREE_LIST_SIZE::size;
      }
      else
        ALLOCATOR::deallocate(block);
    }

    ~FreeListAllocator() noexcept { clear_free_list(); }

    bool owns(Block block) const noexcept
      requires IsOwningAllocator<ALLOCATOR>
    {
      // the blocks owned by the free list appear still active for that allocator
      return ALLOCATOR::owns(block);
    }
  };
  static_assert(IsAllocator<FreeListAllocator<NullAllocator, 64, 64, 1>>);
  static_assert(IsAllocator<FreeListAllocator<NullAllocatorThrow, 64, 64, 1>>);
  static_assert(IsAllocator<FreeListAllocator<NullAllocatorAbort, 64, 64, 1>>);
} // namespace stdcolt::alloc

#endif // !__HG_STDCOLT_ALLOCATORS_FREE_LIST
