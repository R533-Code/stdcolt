#include <doctest/doctest.h>
#include <stdcolt_allocators/allocators/null_allocator.h>
#include <stdcolt_allocators/allocators/stack_allocator.h>
#include <stdcolt_allocators/allocators/mallocator.h>
#include <stdcolt_allocators/allocators/free_list_allocator.h>

#define PTR_OF(block) (char*)block.ptr()

TEST_CASE("stdcolt/alloc")
{
  using namespace stdcolt::alloc;

  constexpr std::size_t TEST_ALIGN = PREFERRED_ALIGNMENT;

  SUBCASE("NullAllocator::allocate returns nullblock")
  {
    const NullAllocator alloc{};
    Layout layout{16, TEST_ALIGN};

    Block b1 = alloc.allocate(layout);
    Block b2 = alloc.allocate(layout);

    CHECK(b1 == nullblock);
    CHECK(b2 == nullblock);
  }

  SUBCASE("NullAllocator::deallocate accepts only nullblock")
  {
    const NullAllocator alloc{};
    alloc.deallocate(nullblock);
  }

  SUBCASE("NullAllocatorThrow::allocate throws std::bad_alloc")
  {
    const NullAllocatorThrow alloc{};
    Layout layout{16, TEST_ALIGN};

    CHECK_THROWS_AS(alloc.allocate(layout), std::bad_alloc);
  }

  SUBCASE("NullAllocatorAbort::allocate is not executed in tests")
  {
    const NullAllocatorAbort alloc{};
    (void)alloc;
    CHECK(true); // death-path behavior not tested in-process
  }

  SUBCASE("StackAllocator basic allocation and ownership")
  {
    using SA = StackAllocator<128, TEST_ALIGN>;
    SA alloc;

    Layout layout{16, TEST_ALIGN};

    Block b1 = alloc.allocate(layout);
    CHECK(b1 != nullblock);
    CHECK(alloc.owns(b1));

    Block b2 = alloc.allocate(layout);
    CHECK(b2 != nullblock);
    CHECK(alloc.owns(b2));

    if (b1 != nullblock && b2 != nullblock)
      CHECK(PTR_OF(b1) + b1.size() == PTR_OF(b2));
  }

  SUBCASE("StackAllocator rejects too-large alignment")
  {
    using SA = StackAllocator<64, TEST_ALIGN>;
    SA alloc;

    Layout layout_too_aligned{8, TEST_ALIGN * 2};

    Block blk = alloc.allocate(layout_too_aligned);
    CHECK(blk == nullblock);
  }

  SUBCASE("StackAllocator eventually exhausts buffer")
  {
    using SA = StackAllocator<64, TEST_ALIGN>;
    SA alloc;

    Layout layout{8, TEST_ALIGN};

    bool seen_null = false;
    for (int i = 0; i < 32; ++i)
    {
      Block b = alloc.allocate(layout);
      if (b == nullblock)
      {
        seen_null = true;
        break;
      }
      CHECK(alloc.owns(b));
    }

    CHECK(seen_null);
  }

  SUBCASE("StackAllocator LIFO deallocation semantics")
  {
    using SA = StackAllocator<128, TEST_ALIGN>;
    SA alloc;

    Layout layout{16, TEST_ALIGN};

    Block a = alloc.allocate(layout);
    Block b = alloc.allocate(layout);
    CHECK(a != nullblock);
    CHECK(b != nullblock);
    CHECK(alloc.owns(a));
    CHECK(alloc.owns(b));

    // Non-top deallocation: no change in _size
    alloc.deallocate(a);
    CHECK(alloc.owns(a));
    CHECK(alloc.owns(b));

    Block c = alloc.allocate(layout);
    CHECK(c != nullblock);
    CHECK(alloc.owns(c));
    if (b != nullblock && c != nullblock)
      CHECK(PTR_OF(b) + b.size() == PTR_OF(c));

    // Proper LIFO frees shrink the stack
    alloc.deallocate(c);
    alloc.deallocate(b);

    CHECK(alloc.owns(a));
    CHECK_FALSE(alloc.owns(b));
    CHECK_FALSE(alloc.owns(c));

    Block d = alloc.allocate(layout);
    CHECK(d != nullblock);
    CHECK(alloc.owns(d));

    if (d != nullblock && b != nullblock)
      CHECK(d.ptr() == b.ptr());
  }

  SUBCASE("StackAllocator deallocate_all resets allocator")
  {
    using SA = StackAllocator<128, TEST_ALIGN>;
    SA alloc;

    Layout layout{32, TEST_ALIGN};

    Block b1 = alloc.allocate(layout);
    Block b2 = alloc.allocate(layout);
    CHECK(b1 != nullblock);
    CHECK(b2 != nullblock);
    CHECK(alloc.owns(b1));
    CHECK(alloc.owns(b2));

    alloc.deallocate_all();

    CHECK_FALSE(alloc.owns(b1));
    CHECK_FALSE(alloc.owns(b2));

    Block b3 = alloc.allocate(layout);
    CHECK(b3 != nullblock);
    CHECK(alloc.owns(b3));
  }

  SUBCASE("StackAllocatorMT basic allocation and ownership")
  {
    using SA = StackAllocatorMT<128, TEST_ALIGN>;
    SA alloc;

    Layout layout{16, TEST_ALIGN};

    Block b1 = alloc.allocate(layout);
    CHECK(b1 != nullblock);
    CHECK(alloc.owns(b1));

    Block b2 = alloc.allocate(layout);
    CHECK(b2 != nullblock);
    CHECK(alloc.owns(b2));

    if (b1 != nullblock && b2 != nullblock)
      CHECK(PTR_OF(b1) + b1.size() == PTR_OF(b2));
  }

  SUBCASE("StackAllocatorMT rejects too-large alignment")
  {
    using SA = StackAllocatorMT<64, TEST_ALIGN>;
    SA alloc;

    Layout layout_too_aligned{8, TEST_ALIGN * 2}; // > ALIGN_AS

    Block blk = alloc.allocate(layout_too_aligned);
    CHECK(blk == nullblock);
  }

  SUBCASE("StackAllocatorMT LIFO deallocation semantics (single-thread)")
  {
    using SA = StackAllocatorMT<128, TEST_ALIGN>;
    SA alloc;

    Layout layout{16, TEST_ALIGN};

    Block a = alloc.allocate(layout);
    Block b = alloc.allocate(layout);
    CHECK(a != nullblock);
    CHECK(b != nullblock);
    CHECK(alloc.owns(a));
    CHECK(alloc.owns(b));

    alloc.deallocate(a); // non-top free: no change
    CHECK(alloc.owns(a));
    CHECK(alloc.owns(b));

    Block c = alloc.allocate(layout);
    CHECK(c != nullblock);
    CHECK(alloc.owns(c));
    if (b != nullblock && c != nullblock)
      CHECK(PTR_OF(b) + b.size() == PTR_OF(c));

    alloc.deallocate(c);
    alloc.deallocate(b);

    CHECK(alloc.owns(a));
    CHECK_FALSE(alloc.owns(b));
    CHECK_FALSE(alloc.owns(c));

    Block d = alloc.allocate(layout);
    CHECK(d != nullblock);
    CHECK(alloc.owns(d));

    if (d != nullblock && b != nullblock)
      CHECK(d.ptr() == b.ptr());
  }

  SUBCASE("StackAllocatorMT deallocate_all resets allocator")
  {
    using SA = StackAllocatorMT<128, TEST_ALIGN>;
    SA alloc;

    Layout layout{32, TEST_ALIGN};

    Block b1 = alloc.allocate(layout);
    Block b2 = alloc.allocate(layout);
    CHECK(b1 != nullblock);
    CHECK(b2 != nullblock);
    CHECK(alloc.owns(b1));
    CHECK(alloc.owns(b2));

    alloc.deallocate_all();

    CHECK_FALSE(alloc.owns(b1));
    CHECK_FALSE(alloc.owns(b2));

    Block b3 = alloc.allocate(layout);
    CHECK(b3 != nullblock);
    CHECK(alloc.owns(b3));
  }
}
