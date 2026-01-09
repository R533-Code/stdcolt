#include <doctest/doctest.h>
#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <memory>
#include <string_view>
#include <utility>

#include <stdcolt_allocators/allocators/mallocator.h>
#include <stdcolt_runtime_type/runtime_type.h>
#include <stdcolt_runtime_type/allocator.h>
#include <stdcolt_runtime_type/perfect_hash_function.h>

using stdcolt::alloc::MallocatorAligned;

// -----------------------------
// Small C-API helpers
// -----------------------------
static stdcolt_ext_rt_StringView sv(std::string_view s)
{
  return stdcolt_ext_rt_StringView{s.data(), static_cast<uint64_t>(s.size())};
}

static uintptr_t found_addr(const stdcolt_ext_rt_ResultLookup& r)
{
  return r.data.found.address_or_offset;
}

static stdcolt_ext_rt_Type mismatch_actual(const stdcolt_ext_rt_ResultLookup& r)
{
  return r.data.mismatch_type.actual_type;
}

static bool is_aligned(uintptr_t offset, uint32_t align)
{
  return align == 0 ? false : (offset % align) == 0;
}

// -----------------------------
// Default recipes
// -----------------------------
// These are expected to be provided by the library headers you already include.
// If your project uses different names, adjust these two calls accordingly.
static stdcolt_ext_rt_RecipeAllocator default_alloc_recipe()
{
  return stdcolt_ext_rt_default_allocator();
}
static stdcolt_ext_rt_RecipePerfectHashFunction default_phf_recipe()
{
  return stdcolt_ext_rt_default_perfect_hash_function();
}

static stdcolt_ext_rt_RuntimeContext* require_ctx()
{
  auto alloc = default_alloc_recipe();
  auto phf   = default_phf_recipe();

  auto r = stdcolt_ext_rt_create(&alloc, &phf);
  REQUIRE(r.result == STDCOLT_EXT_RT_CTX_SUCCESS);
  REQUIRE(r.data.success.context != nullptr);
  return r.data.success.context;
}

static stdcolt_ext_rt_ResultLookup lookup_found(
    stdcolt_ext_rt_Type ty, std::string_view name, stdcolt_ext_rt_Type expected)
{
  auto n = sv(name);
  auto r = stdcolt_ext_rt_type_lookup(ty, &n, expected);
  REQUIRE(r.result == STDCOLT_EXT_RT_LOOKUP_FOUND);
  return r;
}

template<class Fn>
static Fn lookup_thunk(
    stdcolt_ext_rt_Type ty, std::string_view name,
    stdcolt_ext_rt_Type expected_fn_type)
{
  auto r  = lookup_found(ty, name, expected_fn_type);
  auto fn = reinterpret_cast<Fn>(found_addr(r));
  REQUIRE(fn != nullptr);
  return fn;
}

// -----------------------------
// Test types
// -----------------------------
struct Foo
{
  int x;

  int add_ref(int& a, const int& b)
  {
    a += b;
    return a;
  }
  int getx() const { return x; }
};

// ABI-lowered thunks stored in the runtime type as function pointers.
static int Foo_add_ref_thunk(void* self, int* a, const int* b)
{
  return static_cast<Foo*>(self)->add_ref(*a, *b);
}
static int Foo_getx_thunk(const void* self)
{
  return static_cast<const Foo*>(self)->getx();
}

struct PaddingHeavy
{
  uint8_t a;  // 1
  uint64_t b; // 8
  uint16_t c; // 2
  uint32_t d; // 4
};

// A non-trivial type to test destroy recursion
struct DtorCounter
{
  static inline std::atomic<int> dtors{0};
  int v{0};
  ~DtorCounter() { dtors.fetch_add(1); }
};

// -----------------------------
// Named lifetime helpers (C API)
// -----------------------------
static void dtorcounter_move(const stdcolt_ext_rt_TypeDesc*, void* out, void* src)
{
  auto* s = static_cast<DtorCounter*>(src);
  std::construct_at(static_cast<DtorCounter*>(out), std::move(*s));
  std::destroy_at(s);
}

static bool dtorcounter_copy(
    const stdcolt_ext_rt_TypeDesc*, void* out, const void* src)
{
  auto* s = static_cast<const DtorCounter*>(src);
  std::construct_at(static_cast<DtorCounter*>(out), *s);
  return true;
}

static void dtorcounter_destroy(const stdcolt_ext_rt_TypeDesc*, void* p)
{
  std::destroy_at(static_cast<DtorCounter*>(p));
}

static stdcolt_ext_rt_NamedLifetime trivially_copyable_lifetime()
{
  stdcolt_ext_rt_NamedLifetime lt{};
  lt.move_fn               = nullptr;
  lt.copy_fn               = nullptr;
  lt.destroy_fn            = nullptr;
  lt.is_trivially_movable  = 1;
  lt.is_trivially_copyable = 1;
  return lt;
}

static stdcolt_ext_rt_NamedLifetime dtorcounter_lifetime()
{
  stdcolt_ext_rt_NamedLifetime lt{};
  lt.move_fn               = &dtorcounter_move;
  lt.copy_fn               = &dtorcounter_copy;
  lt.destroy_fn            = &dtorcounter_destroy;
  lt.is_trivially_movable  = 0;
  lt.is_trivially_copyable = 0;
  return lt;
}

// -----------------------------
// Member lists
// -----------------------------
static auto paddingheavy_members(stdcolt_ext_rt_RuntimeContext* ctx)
{
  auto u8 = stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_U8);
  auto u64 =
      stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_U64);
  auto u16 =
      stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_U16);
  auto u32 =
      stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_U32);

  REQUIRE(u8.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  REQUIRE(u64.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  REQUIRE(u16.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  REQUIRE(u32.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  return std::array<stdcolt_ext_rt_MemberInfo, 4>{{
      {sv("a"), sv(""), u8.data.success.type},
      {sv("b"), sv(""), u64.data.success.type},
      {sv("c"), sv(""), u16.data.success.type},
      {sv("d"), sv(""), u32.data.success.type},
  }};
}

// -----------------------------
// Allocator override test support (C allocator recipe)
// -----------------------------
struct GlobalAllocCounters
{
  std::atomic<int> constructs{0};
  std::atomic<int> destructs{0};
  std::atomic<int> allocs{0};
  std::atomic<int> deallocs{0};
};
static GlobalAllocCounters g_counting;

static size_t align_up_size_t(size_t v, size_t a)
{
  return (v + (a - 1)) & ~(a - 1);
}

static int32_t counting_alloc_construct(void*) noexcept
{
  g_counting.constructs.fetch_add(1);
  return 0;
}
static void counting_alloc_destruct(void*) noexcept
{
  g_counting.destructs.fetch_add(1);
}

static stdcolt_ext_rt_Block counting_alloc_alloc(
    void*, size_t bytes, size_t align) noexcept
{
  g_counting.allocs.fetch_add(1);
  size_t n = align_up_size_t(bytes, align);
  void* p  = MallocatorAligned{}.allocate({n, align}).ptr();
  return stdcolt_ext_rt_Block{p, n};
}
static void counting_alloc_dealloc(void*, const stdcolt_ext_rt_Block* b) noexcept
{
  g_counting.deallocs.fetch_add(1);
  MallocatorAligned{}.deallocate({b->ptr, b->size});
}

static stdcolt_ext_rt_RecipeAllocator make_global_counting_allocator_recipe() noexcept
{
  stdcolt_ext_rt_RecipeAllocator r{};
  r.allocator_sizeof    = 0;
  r.allocator_alignof   = 1;
  r.allocator_construct = &counting_alloc_construct;
  r.allocator_destruct  = &counting_alloc_destruct;
  r.allocator_alloc     = &counting_alloc_alloc;
  r.allocator_dealloc   = &counting_alloc_dealloc;
  return r;
}

static stdcolt_ext_rt_RecipePerfectHashFunction make_invalid_phf_recipe() noexcept
{
  auto r       = default_phf_recipe();
  r.phf_lookup = nullptr; // invalid by contract
  return r;
}

static stdcolt_ext_rt_RecipeAllocator make_invalid_alloc_recipe() noexcept
{
  auto r            = default_alloc_recipe();
  r.allocator_alloc = nullptr; // invalid by contract
  return r;
}

// -----------------------------
// Tests
// -----------------------------
TEST_CASE("stdcolt/extensions/runtime_type: create invalid recipes")
{
  {
    auto bad_alloc = make_invalid_alloc_recipe();
    auto ok_phf    = default_phf_recipe();
    auto r         = stdcolt_ext_rt_create(&bad_alloc, &ok_phf);
    CHECK(r.result == STDCOLT_EXT_RT_CTX_INVALID_ALLOCATOR);
  }
  {
    auto ok_alloc = default_alloc_recipe();
    auto bad_phf  = make_invalid_phf_recipe();
    auto r        = stdcolt_ext_rt_create(&ok_alloc, &bad_phf);
    CHECK(r.result == STDCOLT_EXT_RT_CTX_INVALID_PHF);
  }
}

TEST_CASE(
    "stdcolt/extensions/runtime_type: builtin/pointer/array/fn dedup and identity")
{
  auto* ctx = require_ctx();

  SUBCASE("builtin kind")
  {
    auto ti =
        stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
    REQUIRE(ti.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    REQUIRE(ti.data.success.type != nullptr);
    CHECK(ti.data.success.type->kind == STDCOLT_EXT_RT_TYPE_KIND_BUILTIN);
  }

  SUBCASE("pointer dedup + pointee constness")
  {
    auto ti =
        stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
    REQUIRE(ti.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    auto* i32 = ti.data.success.type;

    auto p1 = stdcolt_ext_rt_type_create_ptr(ctx, i32, false);
    auto p2 = stdcolt_ext_rt_type_create_ptr(ctx, i32, false);
    REQUIRE(p1.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    REQUIRE(p2.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    CHECK(p1.data.success.type == p2.data.success.type);

    auto* pt = p1.data.success.type;
    REQUIRE(pt->kind == STDCOLT_EXT_RT_TYPE_KIND_POINTER);
    CHECK(pt->info.kind_pointer.pointee_type == i32);
    CHECK(pt->info.kind_pointer.pointee_is_const == 0);

    auto pc = stdcolt_ext_rt_type_create_ptr(ctx, i32, true);
    REQUIRE(pc.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    REQUIRE(pc.data.success.type->kind == STDCOLT_EXT_RT_TYPE_KIND_POINTER);
    CHECK(pc.data.success.type->info.kind_pointer.pointee_type == i32);
    CHECK(pc.data.success.type->info.kind_pointer.pointee_is_const == 1);
    CHECK(pc.data.success.type != pt);

    // Opaque address vs const opaque address must be distinct
    auto v1 = stdcolt_ext_rt_type_create_builtin(
        ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_OPAQUE_ADDRESS);
    auto v2 = stdcolt_ext_rt_type_create_builtin(
        ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_CONST_OPAQUE_ADDRESS);
    REQUIRE(v1.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    REQUIRE(v2.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    CHECK(v1.data.success.type != v2.data.success.type);
  }

  SUBCASE("array dedup + extent key")
  {
    auto ti =
        stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
    REQUIRE(ti.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    auto* i32 = ti.data.success.type;

    auto a1 = stdcolt_ext_rt_type_create_array(ctx, i32, 3);
    auto a2 = stdcolt_ext_rt_type_create_array(ctx, i32, 3);
    auto a3 = stdcolt_ext_rt_type_create_array(ctx, i32, 4);
    REQUIRE(a1.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    REQUIRE(a2.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    REQUIRE(a3.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    CHECK(a1.data.success.type == a2.data.success.type);
    CHECK(a1.data.success.type != a3.data.success.type);
  }

  SUBCASE("function type dedup")
  {
    auto ti32 =
        stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
    REQUIRE(ti32.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    auto* i32 = ti32.data.success.type;

    auto tptr = stdcolt_ext_rt_type_create_ptr(ctx, i32, true);
    REQUIRE(tptr.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    auto* ci32p = tptr.data.success.type;

    // F = int(i32, const i32*)
    stdcolt_ext_rt_Type args1[] = {i32, ci32p};
    stdcolt_ext_rt_TypeView av1{args1, 2};

    auto f1 = stdcolt_ext_rt_type_create_fn(ctx, i32, av1);
    auto f2 = stdcolt_ext_rt_type_create_fn(ctx, i32, av1);
    REQUIRE(f1.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    REQUIRE(f2.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    CHECK(f1.data.success.type == f2.data.success.type);

    // G differs in pointer constness
    auto tptr2 = stdcolt_ext_rt_type_create_ptr(ctx, i32, false);
    REQUIRE(tptr2.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    auto* i32p = tptr2.data.success.type;

    stdcolt_ext_rt_Type args2[] = {i32, i32p};
    stdcolt_ext_rt_TypeView av2{args2, 2};

    auto g = stdcolt_ext_rt_type_create_fn(ctx, i32, av2);
    REQUIRE(g.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    CHECK(f1.data.success.type != g.data.success.type);
  }

  stdcolt_ext_rt_destroy(ctx);
}

TEST_CASE("stdcolt/extensions/runtime_type: named type exposes members and methods")
{
  auto* ctx = require_ctx();

  // Build needed types
  auto ti32 =
      stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
  REQUIRE(ti32.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  auto* t_int = ti32.data.success.type;

  // Expected thunk types
  // add_ref thunk: int(void*, int*, const int*)
  auto t_voidp = stdcolt_ext_rt_type_create_builtin(
      ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_OPAQUE_ADDRESS);
  REQUIRE(t_voidp.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  auto t_intp = stdcolt_ext_rt_type_create_ptr(ctx, t_int, false);
  REQUIRE(t_intp.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  auto t_cintp = stdcolt_ext_rt_type_create_ptr(ctx, t_int, true);
  REQUIRE(t_cintp.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  stdcolt_ext_rt_Type add_args[] = {
      t_voidp.data.success.type,
      t_intp.data.success.type,
      t_cintp.data.success.type,
  };
  auto add_fn = stdcolt_ext_rt_type_create_fn(
      ctx, t_int, stdcolt_ext_rt_TypeView{add_args, 3});
  REQUIRE(add_fn.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  // getx thunk: int(const void*)
  auto t_cvoidp = stdcolt_ext_rt_type_create_builtin(
      ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_CONST_OPAQUE_ADDRESS);
  REQUIRE(t_cvoidp.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  stdcolt_ext_rt_Type getx_args[] = {t_cvoidp.data.success.type};
  auto getx_fn                    = stdcolt_ext_rt_type_create_fn(
      ctx, t_int, stdcolt_ext_rt_TypeView{getx_args, 1});
  REQUIRE(getx_fn.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  // Create Foo named type with realized members
  stdcolt_ext_rt_Member mems[] = {
      {sv("x"), sv("field"), t_int, (uintptr_t)offsetof(Foo, x)},
      {sv("add_ref"), sv("method"), add_fn.data.success.type,
       (uintptr_t)reinterpret_cast<void*>(&Foo_add_ref_thunk)},
      {sv("getx"), sv("const method"), getx_fn.data.success.type,
       (uintptr_t)reinterpret_cast<void*>(&Foo_getx_thunk)},
  };
  stdcolt_ext_rt_MemberView mv{mems, 3};

  auto lt = trivially_copyable_lifetime();

  auto name   = sv("Foo");
  auto ty_res = stdcolt_ext_rt_type_create(
      ctx, &name, &mv, alignof(Foo), sizeof(Foo), &lt, nullptr, nullptr);

  REQUIRE(ty_res.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  auto* ty = ty_res.data.success.type;
  REQUIRE(ty != nullptr);
  REQUIRE(ty->kind == STDCOLT_EXT_RT_TYPE_KIND_NAMED);

  SUBCASE("field lookup returns offsetof")
  {
    auto r = lookup_found(ty, "x", t_int);
    CHECK(found_addr(r) == (uintptr_t)offsetof(Foo, x));
  }

  SUBCASE("method lookup: ABI-lowered thunk is callable")
  {
    using ThunkSig = int (*)(void*, int*, const int*);
    auto fn        = lookup_thunk<ThunkSig>(ty, "add_ref", add_fn.data.success.type);

    Foo f{};
    int a   = 2;
    int b   = 3;
    int out = fn(&f, &a, &b);
    CHECK(out == 5);
    CHECK(a == 5);
  }

  SUBCASE("const method lookup: thunk uses const Foo*")
  {
    using ThunkSig = int (*)(const void*);
    auto fn        = lookup_thunk<ThunkSig>(ty, "getx", getx_fn.data.success.type);

    Foo f{};
    f.x = 42;
    CHECK(fn(&f) == 42);
  }

  SUBCASE("lookup mismatch type returns LOOKUP_MISMATCH_TYPE")
  {
    auto wrong =
        stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I64);
    REQUIRE(wrong.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

    auto n = sv("x");
    auto r = stdcolt_ext_rt_type_lookup(ty, &n, wrong.data.success.type);
    REQUIRE(r.result == STDCOLT_EXT_RT_LOOKUP_MISMATCH_TYPE);
    CHECK(mismatch_actual(r) == t_int);
  }

  stdcolt_ext_rt_destroy(ctx);
}

TEST_CASE("stdcolt/extensions/runtime_type: type_create_runtime optimize-size-fast "
          "invariants")
{
  auto* ctx = require_ctx();

  SUBCASE("optimize size fast: size <= declared and offsets aligned")
  {
    auto mems = paddingheavy_members(ctx);

    auto n1 = sv("PaddingHeavy_decl");
    stdcolt_ext_rt_MemberInfoView mv1{mems.data(), (uint64_t)mems.size()};
    auto t_decl = stdcolt_ext_rt_type_create_runtime(
        ctx, &n1, &mv1, STDCOLT_EXT_RT_LAYOUT_AS_DECLARED, nullptr, nullptr);

    REQUIRE(t_decl.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    auto* decl = t_decl.data.success.type;
    REQUIRE(decl != nullptr);

    auto n2    = sv("PaddingHeavy_opt");
    auto t_opt = stdcolt_ext_rt_type_create_runtime(
        ctx, &n2, &mv1, STDCOLT_EXT_RT_LAYOUT_OPTIMIZE_SIZE_FAST, nullptr, nullptr);

    REQUIRE(t_opt.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    auto* opt = t_opt.data.success.type;
    REQUIRE(opt != nullptr);

    CHECK(opt->type_size <= decl->type_size);

    for (const auto& m : mems)
    {
      auto r = stdcolt_ext_rt_type_lookup(opt, &m.name, m.type);
      REQUIRE(r.result == STDCOLT_EXT_RT_LOOKUP_FOUND);

      const uint32_t a = (uint32_t)m.type->type_align;
      CHECK(is_aligned(found_addr(r), a));
    }
  }

  SUBCASE("optimize size fast: empty and singleton")
  {
    // Empty
    {
      auto name = sv("Empty");
      stdcolt_ext_rt_MemberInfoView none{nullptr, 0};
      auto t = stdcolt_ext_rt_type_create_runtime(
          ctx, &name, &none, STDCOLT_EXT_RT_LAYOUT_OPTIMIZE_SIZE_FAST, nullptr,
          nullptr);

      REQUIRE(t.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
      REQUIRE(t.data.success.type != nullptr);
      CHECK(t.data.success.type->type_align == 1);
      CHECK(t.data.success.type->type_size == 0);
    }

    // Singleton
    {
      auto u64 =
          stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_U64);
      REQUIRE(u64.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

      stdcolt_ext_rt_MemberInfo mems1[] = {
          {sv("e"), sv(""), u64.data.success.type},
      };
      stdcolt_ext_rt_MemberInfoView mv{mems1, 1};

      auto name = sv("One");
      auto t    = stdcolt_ext_rt_type_create_runtime(
          ctx, &name, &mv, STDCOLT_EXT_RT_LAYOUT_OPTIMIZE_SIZE_FAST, nullptr,
          nullptr);

      REQUIRE(t.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
      REQUIRE(t.data.success.type != nullptr);

      CHECK(t.data.success.type->type_align == u64.data.success.type->type_align);
      CHECK(t.data.success.type->type_size == u64.data.success.type->type_size);

      auto n = sv("e");
      auto r = stdcolt_ext_rt_type_lookup(t.data.success.type, &n, mems1[0].type);
      REQUIRE(r.result == STDCOLT_EXT_RT_LOOKUP_FOUND);
      CHECK(found_addr(r) == 0);
    }
  }

  SUBCASE("invalid layout enum returns TYPE_INVALID_PARAM")
  {
    auto i32 =
        stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
    REQUIRE(i32.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

    stdcolt_ext_rt_MemberInfo mems1[] = {
        {sv("x"), sv(""), i32.data.success.type},
    };
    stdcolt_ext_rt_MemberInfoView mv{mems1, 1};

    auto bad = (stdcolt_ext_rt_Layout)(STDCOLT_EXT_RT_LAYOUT_end + 1);

    auto name = sv("BadLayout");
    auto t =
        stdcolt_ext_rt_type_create_runtime(ctx, &name, &mv, bad, nullptr, nullptr);
    CHECK(t.result == STDCOLT_EXT_RT_TYPE_INVALID_PARAM);
  }

  stdcolt_ext_rt_destroy(ctx);
}

TEST_CASE("stdcolt/extensions/runtime_type: builtin creation invalid param")
{
  auto* ctx = require_ctx();

  auto t = stdcolt_ext_rt_type_create_builtin(ctx, (stdcolt_ext_rt_BuiltInType)255);
  CHECK(t.result == STDCOLT_EXT_RT_TYPE_INVALID_PARAM);

  stdcolt_ext_rt_destroy(ctx);
}

TEST_CASE("stdcolt/extensions/runtime_type: type_create_fn validates return owner "
          "(void allowed)")
{
  auto* ctx  = require_ctx();
  auto* ctx2 = require_ctx();

  auto ret_other =
      stdcolt_ext_rt_type_create_builtin(ctx2, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
  auto arg =
      stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
  REQUIRE(ret_other.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  REQUIRE(arg.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  stdcolt_ext_rt_Type args[] = {arg.data.success.type};
  stdcolt_ext_rt_TypeView av{args, 1};

  auto bad = stdcolt_ext_rt_type_create_fn(ctx, ret_other.data.success.type, av);
  CHECK(bad.result == STDCOLT_EXT_RT_TYPE_INVALID_OWNER);

  auto ok = stdcolt_ext_rt_type_create_fn(ctx, nullptr, av);
  CHECK(ok.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  stdcolt_ext_rt_destroy(ctx2);
  stdcolt_ext_rt_destroy(ctx);
}

TEST_CASE("stdcolt/extensions/runtime_type: type_create duplicate name -> "
          "TYPE_FAIL_EXISTS")
{
  auto* ctx = require_ctx();

  auto i32 =
      stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
  REQUIRE(i32.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  stdcolt_ext_rt_Member mems[] = {
      {sv("x"), sv(""), i32.data.success.type, (uintptr_t)offsetof(Foo, x)},
  };
  stdcolt_ext_rt_MemberView mv{mems, 1};
  auto lt = trivially_copyable_lifetime();

  auto n  = sv("FooDup");
  auto t1 = stdcolt_ext_rt_type_create(
      ctx, &n, &mv, alignof(Foo), sizeof(Foo), &lt, nullptr, nullptr);
  REQUIRE(t1.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  auto t2 = stdcolt_ext_rt_type_create(
      ctx, &n, &mv, alignof(Foo), sizeof(Foo), &lt, nullptr, nullptr);
  REQUIRE(t2.result == STDCOLT_EXT_RT_TYPE_FAIL_EXISTS);
  CHECK(t2.data.fail_exists.existing_type == t1.data.success.type);

  stdcolt_ext_rt_destroy(ctx);
}

TEST_CASE("stdcolt/extensions/runtime_type: lookup_fast vs lookup + prepared member")
{
  auto* ctx = require_ctx();

  auto i32 =
      stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
  REQUIRE(i32.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  stdcolt_ext_rt_Member mems[] = {
      {sv("x"), sv(""), i32.data.success.type, (uintptr_t)offsetof(Foo, x)},
  };
  stdcolt_ext_rt_MemberView mv{mems, 1};
  auto lt = trivially_copyable_lifetime();

  auto n      = sv("FooLook");
  auto ty_res = stdcolt_ext_rt_type_create(
      ctx, &n, &mv, alignof(Foo), sizeof(Foo), &lt, nullptr, nullptr);
  REQUIRE(ty_res.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  auto* ty = ty_res.data.success.type;

  auto name_x = sv("x");

  auto r1 = stdcolt_ext_rt_type_lookup(ty, &name_x, i32.data.success.type);
  REQUIRE(r1.result == STDCOLT_EXT_RT_LOOKUP_FOUND);

  auto r2 = stdcolt_ext_rt_type_lookup_fast(ty, &name_x, i32.data.success.type);
  REQUIRE(r2.result == STDCOLT_EXT_RT_LOOKUP_FOUND);
  CHECK(found_addr(r2) == found_addr(r1));

  auto pm = stdcolt_ext_rt_prepare_member(ty, &name_x, i32.data.success.type);
  auto r3 = stdcolt_ext_rt_resolve_prepared_member(&pm);
  REQUIRE(r3.result == STDCOLT_EXT_RT_LOOKUP_FOUND);
  CHECK(found_addr(r3) == found_addr(r1));

  auto i64 =
      stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I64);
  REQUIRE(i64.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  auto bad = stdcolt_ext_rt_type_lookup_fast(ty, &name_x, i64.data.success.type);
  CHECK(bad.result == STDCOLT_EXT_RT_LOOKUP_MISMATCH_TYPE);

  auto name_nope = sv("nope");
  auto nf        = stdcolt_ext_rt_type_lookup(ty, &name_nope, i32.data.success.type);
  CHECK(nf.result == STDCOLT_EXT_RT_LOOKUP_NOT_FOUND);

  stdcolt_ext_rt_destroy(ctx);
}

TEST_CASE("stdcolt/extensions/runtime_type: opaque registration set/get/overwrite")
{
  auto* ctx = require_ctx();

  auto ti = stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
  auto tu = stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_U32);
  REQUIRE(ti.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  REQUIRE(tu.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  void* id = (void*)0x1234;
  CHECK(stdcolt_ext_rt_register_set_type(ctx, id, ti.data.success.type) == true);
  CHECK(stdcolt_ext_rt_register_get_type(ctx, id) == ti.data.success.type);

  CHECK(stdcolt_ext_rt_register_set_type(ctx, id, tu.data.success.type) == true);
  CHECK(stdcolt_ext_rt_register_get_type(ctx, id) == tu.data.success.type);

  CHECK(
      stdcolt_ext_rt_register_set_type(nullptr, id, tu.data.success.type) == false);
  CHECK(
      stdcolt_ext_rt_register_set_type(ctx, nullptr, tu.data.success.type) == false);
  CHECK(stdcolt_ext_rt_register_get_type(nullptr, id) == nullptr);

  stdcolt_ext_rt_destroy(ctx);
}

TEST_CASE("stdcolt/extensions/runtime_type: Value construction modes and destroy "
          "clears header")
{
  auto* ctx = require_ctx();

  SUBCASE("empty is noop")
  {
    stdcolt_ext_rt_Value v{};
    stdcolt_ext_rt_val_construct_empty(&v);
    CHECK(v.header.type == nullptr);
    CHECK(v.header.address == nullptr);
    stdcolt_ext_rt_val_destroy(&v);
    CHECK(v.header.type == nullptr);
    CHECK(v.header.address == nullptr);
  }

  SUBCASE("SBO object clears header on destroy")
  {
    stdcolt_ext_rt_Value v{};
    auto t_i32 =
        stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
    REQUIRE(t_i32.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

    REQUIRE(
        stdcolt_ext_rt_val_construct(&v, t_i32.data.success.type)
        == STDCOLT_EXT_RT_VALUE_SUCCESS);
    CHECK(v.header.type == t_i32.data.success.type);
    CHECK(v.header.address != nullptr);

    *reinterpret_cast<int32_t*>(v.header.address) = 123;

    stdcolt_ext_rt_val_destroy(&v);
    CHECK(v.header.type == nullptr);
    CHECK(v.header.address == nullptr);
  }

  SUBCASE("heap object clears header on destroy")
  {
    stdcolt_ext_rt_Value v{};
    auto u8 =
        stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_U8);
    REQUIRE(u8.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

    auto big = stdcolt_ext_rt_type_create_array(ctx, u8.data.success.type, 512);
    REQUIRE(big.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

    REQUIRE(
        stdcolt_ext_rt_val_construct(&v, big.data.success.type)
        == STDCOLT_EXT_RT_VALUE_SUCCESS);
    CHECK(v.header.type == big.data.success.type);
    CHECK(v.header.address != nullptr);

    stdcolt_ext_rt_val_destroy(&v);
    CHECK(v.header.type == nullptr);
    CHECK(v.header.address == nullptr);
  }

  stdcolt_ext_rt_destroy(ctx);
}

TEST_CASE("stdcolt/extensions/runtime_type: Runtime type lifetime recursion: "
          "destroy of member runs")
{
  auto* ctx = require_ctx();
  DtorCounter::dtors.store(0);

  // DtorCounter named type with non-trivial lifetime (copy/destroy provided)
  stdcolt_ext_rt_MemberView empty_members{nullptr, 0};
  auto lt   = dtorcounter_lifetime();
  auto n_dt = sv("DtorCounter");
  auto dt   = stdcolt_ext_rt_type_create(
      ctx, &n_dt, &empty_members, alignof(DtorCounter), sizeof(DtorCounter), &lt,
      nullptr, nullptr);
  REQUIRE(dt.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  auto* t_dt = dt.data.success.type;

  // runtime type with one member of that type
  stdcolt_ext_rt_MemberInfo mems[] = {
      {sv("m"), sv(""), t_dt},
  };
  stdcolt_ext_rt_MemberInfoView mv{mems, 1};
  auto n_outer = sv("Outer");
  auto outer   = stdcolt_ext_rt_type_create_runtime(
      ctx, &n_outer, &mv, STDCOLT_EXT_RT_LAYOUT_AS_DECLARED, nullptr, nullptr);
  REQUIRE(outer.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  auto* t_outer = outer.data.success.type;

  stdcolt_ext_rt_Value v{};
  REQUIRE(stdcolt_ext_rt_val_construct(&v, t_outer) == STDCOLT_EXT_RT_VALUE_SUCCESS);

  auto nm  = sv("m");
  auto off = stdcolt_ext_rt_type_lookup(t_outer, &nm, t_dt);
  REQUIRE(off.result == STDCOLT_EXT_RT_LOOKUP_FOUND);

  void* member_addr = (uint8_t*)v.header.address + found_addr(off);
  std::construct_at(reinterpret_cast<DtorCounter*>(member_addr));

  stdcolt_ext_rt_val_destroy(&v);
  CHECK(DtorCounter::dtors.load() == 1);

  stdcolt_ext_rt_destroy(ctx);
}

TEST_CASE("stdcolt/extensions/runtime_type: Copy rollback destroys already-copied "
          "member on failure")
{
  auto* ctx = require_ctx();
  DtorCounter::dtors.store(0);

  // FailCopy named type: copy_fn always fails
  stdcolt_ext_rt_NamedLifetime fail_lt{};
  fail_lt.move_fn               = nullptr;
  fail_lt.copy_fn               = +[](const stdcolt_ext_rt_TypeDesc*, void*,
                        const void*) noexcept -> bool { return false; };
  fail_lt.destroy_fn            = nullptr;
  fail_lt.is_trivially_movable  = 1;
  fail_lt.is_trivially_copyable = 0;

  stdcolt_ext_rt_MemberView empty_members{nullptr, 0};
  auto n_fail = sv("FailCopy");
  auto fail   = stdcolt_ext_rt_type_create(
      ctx, &n_fail, &empty_members, 1, 1, &fail_lt, nullptr, nullptr);
  REQUIRE(fail.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  auto* t_fail = fail.data.success.type;

  // DtorCounter named type with copy/destroy
  auto lt_dt = dtorcounter_lifetime();
  auto n_dt  = sv("DtorCounter2");
  auto dt    = stdcolt_ext_rt_type_create(
      ctx, &n_dt, &empty_members, alignof(DtorCounter), sizeof(DtorCounter), &lt_dt,
      nullptr, nullptr);
  REQUIRE(dt.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  auto* t_dt = dt.data.success.type;

  // Outer = { DtorCounter a; FailCopy b; }
  stdcolt_ext_rt_MemberInfo mems[] = {
      {sv("a"), sv(""), t_dt},
      {sv("b"), sv(""), t_fail},
  };
  stdcolt_ext_rt_MemberInfoView mv{mems, 2};
  auto n_outer = sv("OuterRollback");
  auto outer   = stdcolt_ext_rt_type_create_runtime(
      ctx, &n_outer, &mv, STDCOLT_EXT_RT_LAYOUT_AS_DECLARED, nullptr, nullptr);
  REQUIRE(outer.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  auto* t_outer = outer.data.success.type;

  stdcolt_ext_rt_Value src{};
  REQUIRE(
      stdcolt_ext_rt_val_construct(&src, t_outer) == STDCOLT_EXT_RT_VALUE_SUCCESS);

  auto na    = sv("a");
  auto nb    = sv("b");
  auto off_a = stdcolt_ext_rt_type_lookup(t_outer, &na, t_dt);
  auto off_b = stdcolt_ext_rt_type_lookup(t_outer, &nb, t_fail);
  REQUIRE(off_a.result == STDCOLT_EXT_RT_LOOKUP_FOUND);
  REQUIRE(off_b.result == STDCOLT_EXT_RT_LOOKUP_FOUND);

  void* a_addr = (uint8_t*)src.header.address + found_addr(off_a);
  void* b_addr = (uint8_t*)src.header.address + found_addr(off_b);

  std::construct_at(reinterpret_cast<DtorCounter*>(a_addr));
  *reinterpret_cast<uint8_t*>(b_addr) = 7;

  stdcolt_ext_rt_Value dst{};
  auto ok = stdcolt_ext_rt_val_construct_from_copy(&dst, &src);
  CHECK(ok != STDCOLT_EXT_RT_VALUE_SUCCESS);
  CHECK(dst.header.type == nullptr);
  CHECK(dst.header.address == nullptr);
  CHECK(DtorCounter::dtors.load() == 1);

  stdcolt_ext_rt_val_destroy(&src);
  stdcolt_ext_rt_destroy(ctx);
}

TEST_CASE("stdcolt/extensions/runtime_type: Custom allocator override used by Value "
          "(and arrays recurse)")
{
  auto* ctx = require_ctx();

  g_counting.constructs.store(0);
  g_counting.destructs.store(0);
  g_counting.allocs.store(0);
  g_counting.deallocs.store(0);

  auto counting = make_global_counting_allocator_recipe();
  auto phf      = default_phf_recipe();

  auto triv_lt = trivially_copyable_lifetime();
  stdcolt_ext_rt_MemberView empty_members{nullptr, 0};

  auto n1 = sv("AllocNamed");
  auto tn = stdcolt_ext_rt_type_create(
      ctx, &n1, &empty_members, 8, 64, &triv_lt, &counting, &phf);
  REQUIRE(tn.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  auto* t_named = tn.data.success.type;
  REQUIRE(t_named != nullptr);
  CHECK(g_counting.constructs.load() == 1);

  auto n2     = sv("AllocNamedBig");
  auto tn_big = stdcolt_ext_rt_type_create(
      ctx, &n2, &empty_members, 8, 256, &triv_lt, &counting, &phf);
  REQUIRE(tn_big.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  auto* t_big = tn_big.data.success.type;
  REQUIRE(t_big != nullptr);
  CHECK(g_counting.constructs.load() == 2);

  // 1) Value of the named type should allocate via override allocator (heap expected)
  {
    int a0 = g_counting.allocs.load();
    int d0 = g_counting.deallocs.load();

    stdcolt_ext_rt_Value v1{};
    REQUIRE(
        stdcolt_ext_rt_val_construct(&v1, t_big) == STDCOLT_EXT_RT_VALUE_SUCCESS);
    CHECK(g_counting.allocs.load() == a0 + 1);

    stdcolt_ext_rt_val_destroy(&v1);
    CHECK(g_counting.deallocs.load() == d0 + 1);
  }

  // 2) Array of named should recurse to element allocator and also use override allocator
  {
    auto arr_res = stdcolt_ext_rt_type_create_array(ctx, t_big, 4);
    REQUIRE(arr_res.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    auto* t_arr = arr_res.data.success.type;
    REQUIRE(t_arr != nullptr);

    int a1 = g_counting.allocs.load();
    int d1 = g_counting.deallocs.load();

    stdcolt_ext_rt_Value v2{};
    REQUIRE(
        stdcolt_ext_rt_val_construct(&v2, t_arr) == STDCOLT_EXT_RT_VALUE_SUCCESS);
    CHECK(g_counting.allocs.load() == a1 + 1);

    stdcolt_ext_rt_val_destroy(&v2);
    CHECK(g_counting.deallocs.load() == d1 + 1);
  }

  // 3) Builtin Value should not use counting allocator (SBO anyway; verify no counting alloc)
  {
    auto t_u64 =
        stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_U64);
    REQUIRE(t_u64.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

    int a2 = g_counting.allocs.load();

    stdcolt_ext_rt_Value v3{};
    REQUIRE(
        stdcolt_ext_rt_val_construct(&v3, t_u64.data.success.type)
        == STDCOLT_EXT_RT_VALUE_SUCCESS);
    CHECK(g_counting.allocs.load() == a2);

    stdcolt_ext_rt_val_destroy(&v3);
  }

  stdcolt_ext_rt_destroy(ctx);
  CHECK(g_counting.destructs.load() == 2);
}

TEST_CASE("stdcolt/extensions/runtime_type: lookup on non-named types returns "
          "LOOKUP_EXPECTED_NAMED")
{
  auto* ctx = require_ctx();

  auto t_i32 =
      stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
  REQUIRE(t_i32.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  auto t_ptr = stdcolt_ext_rt_type_create_ptr(ctx, t_i32.data.success.type, false);
  REQUIRE(t_ptr.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  auto t_arr = stdcolt_ext_rt_type_create_array(ctx, t_i32.data.success.type, 3);
  REQUIRE(t_arr.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  stdcolt_ext_rt_Type fn_args[] = {t_i32.data.success.type};
  auto t_fn                     = stdcolt_ext_rt_type_create_fn(
      ctx, t_i32.data.success.type, stdcolt_ext_rt_TypeView{fn_args, 1});
  REQUIRE(t_fn.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  auto expected = t_i32.data.success.type;
  auto name_x   = sv("x");

  stdcolt_ext_rt_Type ts[] = {
      t_i32.data.success.type,
      t_ptr.data.success.type,
      t_arr.data.success.type,
      t_fn.data.success.type,
  };

  for (auto* t : ts)
  {
    auto r = stdcolt_ext_rt_type_lookup(t, &name_x, expected);
    CHECK(r.result == STDCOLT_EXT_RT_LOOKUP_EXPECTED_NAMED);

    auto rf = stdcolt_ext_rt_type_lookup_fast(t, &name_x, expected);
    CHECK(rf.result == STDCOLT_EXT_RT_LOOKUP_EXPECTED_NAMED);
  }

  stdcolt_ext_rt_destroy(ctx);
}

TEST_CASE("stdcolt/extensions/runtime_type: type_create_ptr/array/fn invalid param "
          "and invalid owner matrices")
{
  auto* ctx  = require_ctx();
  auto* ctx2 = require_ctx();

  auto t_i32 =
      stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
  auto t_i32b =
      stdcolt_ext_rt_type_create_builtin(ctx2, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
  REQUIRE(t_i32.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
  REQUIRE(t_i32b.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  SUBCASE("ptr: null pointee -> TYPE_INVALID_PARAM")
  {
    auto r = stdcolt_ext_rt_type_create_ptr(ctx, nullptr, false);
    CHECK(r.result == STDCOLT_EXT_RT_TYPE_INVALID_PARAM);
  }

  SUBCASE("ptr: pointee owned by other ctx -> TYPE_INVALID_OWNER")
  {
    auto r = stdcolt_ext_rt_type_create_ptr(ctx, t_i32b.data.success.type, false);
    CHECK(r.result == STDCOLT_EXT_RT_TYPE_INVALID_OWNER);
  }

  SUBCASE("array: null element -> TYPE_INVALID_PARAM")
  {
    auto r = stdcolt_ext_rt_type_create_array(ctx, nullptr, 3);
    CHECK(r.result == STDCOLT_EXT_RT_TYPE_INVALID_PARAM);
  }

  SUBCASE("array: element owned by other ctx -> TYPE_INVALID_OWNER")
  {
    auto r = stdcolt_ext_rt_type_create_array(ctx, t_i32b.data.success.type, 3);
    CHECK(r.result == STDCOLT_EXT_RT_TYPE_INVALID_OWNER);
  }

  SUBCASE("fn: args contains nullptr -> TYPE_INVALID_PARAM")
  {
    stdcolt_ext_rt_Type args[] = {t_i32.data.success.type, nullptr};
    auto r                     = stdcolt_ext_rt_type_create_fn(
        ctx, t_i32.data.success.type, stdcolt_ext_rt_TypeView{args, 2});
    CHECK(r.result == STDCOLT_EXT_RT_TYPE_INVALID_PARAM);
  }

  SUBCASE("fn: args contains type owned by other ctx -> TYPE_INVALID_OWNER")
  {
    stdcolt_ext_rt_Type args[] = {t_i32.data.success.type, t_i32b.data.success.type};
    auto r                     = stdcolt_ext_rt_type_create_fn(
        ctx, t_i32.data.success.type, stdcolt_ext_rt_TypeView{args, 2});
    CHECK(r.result == STDCOLT_EXT_RT_TYPE_INVALID_OWNER);
  }

  SUBCASE("fn: return type owned by other ctx -> TYPE_INVALID_OWNER")
  {
    stdcolt_ext_rt_Type args[] = {t_i32.data.success.type};
    auto r                     = stdcolt_ext_rt_type_create_fn(
        ctx, t_i32b.data.success.type, stdcolt_ext_rt_TypeView{args, 1});
    CHECK(r.result == STDCOLT_EXT_RT_TYPE_INVALID_OWNER);
  }

  stdcolt_ext_rt_destroy(ctx2);
  stdcolt_ext_rt_destroy(ctx);
}

TEST_CASE("stdcolt/extensions/runtime_type: Value move steals storage and preserves "
          "bits (SBO and heap)")
{
  auto* ctx = require_ctx();

  SUBCASE("SBO: int32_t moved value preserved; source becomes empty")
  {
    auto t_i32 =
        stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_I32);
    REQUIRE(t_i32.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

    stdcolt_ext_rt_Value src{};
    REQUIRE(
        stdcolt_ext_rt_val_construct(&src, t_i32.data.success.type)
        == STDCOLT_EXT_RT_VALUE_SUCCESS);
    *reinterpret_cast<int32_t*>(src.header.address) = 123;

    stdcolt_ext_rt_Value dst{};
    stdcolt_ext_rt_val_construct_from_move(&dst, &src);

    CHECK(src.header.type == nullptr);
    CHECK(src.header.address == nullptr);

    CHECK(dst.header.type == t_i32.data.success.type);
    REQUIRE(dst.header.address != nullptr);
    CHECK(*reinterpret_cast<int32_t*>(dst.header.address) == 123);

    stdcolt_ext_rt_val_destroy(&dst);
    stdcolt_ext_rt_val_destroy(&src);
  }

  SUBCASE("heap: big array storage pointer is stolen; bytes preserved; source "
          "becomes empty")
  {
    auto u8 =
        stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_U8);
    REQUIRE(u8.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

    auto big = stdcolt_ext_rt_type_create_array(ctx, u8.data.success.type, 512);
    REQUIRE(big.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

    stdcolt_ext_rt_Value src{};
    REQUIRE(
        stdcolt_ext_rt_val_construct(&src, big.data.success.type)
        == STDCOLT_EXT_RT_VALUE_SUCCESS);

    auto* p = reinterpret_cast<uint8_t*>(src.header.address);
    p[0]    = 0xAB;
    p[511]  = 0xCD;

    void* stolen_ptr = src.header.address;

    stdcolt_ext_rt_Value dst{};
    stdcolt_ext_rt_val_construct_from_move(&dst, &src);

    CHECK(src.header.type == nullptr);
    CHECK(src.header.address == nullptr);

    CHECK(dst.header.type == big.data.success.type);
    REQUIRE(dst.header.address != nullptr);
    CHECK(dst.header.address == stolen_ptr);

    auto* q = reinterpret_cast<uint8_t*>(dst.header.address);
    CHECK(q[0] == 0xAB);
    CHECK(q[511] == 0xCD);

    stdcolt_ext_rt_val_destroy(&dst);
    stdcolt_ext_rt_val_destroy(&src);
  }

  stdcolt_ext_rt_destroy(ctx);
}

TEST_CASE("stdcolt/extensions/runtime_type: val_construct_from_copy success + deep "
          "rollback recursion")
{
  auto* ctx = require_ctx();

  SUBCASE("copy success: trivially copyable named type")
  {
    auto lt = trivially_copyable_lifetime();
    stdcolt_ext_rt_MemberView empty_members{nullptr, 0};

    auto n     = sv("TrivNamed8");
    auto t_res = stdcolt_ext_rt_type_create(
        ctx, &n, &empty_members, 8, 8, &lt, nullptr, nullptr);
    REQUIRE(t_res.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    auto* t = t_res.data.success.type;

    stdcolt_ext_rt_Value src{};
    REQUIRE(stdcolt_ext_rt_val_construct(&src, t) == STDCOLT_EXT_RT_VALUE_SUCCESS);
    auto* ps = reinterpret_cast<uint8_t*>(src.header.address);
    for (int i = 0; i < 8; ++i)
      ps[i] = static_cast<uint8_t>(0xA0 + i);

    stdcolt_ext_rt_Value dst{};
    auto ok = stdcolt_ext_rt_val_construct_from_copy(&dst, &src);
    CHECK(ok == STDCOLT_EXT_RT_VALUE_SUCCESS);
    CHECK(dst.header.type == t);
    REQUIRE(dst.header.address != nullptr);

    auto* pd = reinterpret_cast<uint8_t*>(dst.header.address);
    for (int i = 0; i < 8; ++i)
      CHECK(pd[i] == static_cast<uint8_t>(0xA0 + i));

    stdcolt_ext_rt_val_destroy(&dst);
    stdcolt_ext_rt_val_destroy(&src);
  }

  SUBCASE("copy success: trivially copyable builtin array")
  {
    auto u32 =
        stdcolt_ext_rt_type_create_builtin(ctx, STDCOLT_EXT_RT_BUILTIN_TYPE_U32);
    REQUIRE(u32.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

    auto t_arr = stdcolt_ext_rt_type_create_array(ctx, u32.data.success.type, 4);
    REQUIRE(t_arr.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

    stdcolt_ext_rt_Value src{};
    REQUIRE(
        stdcolt_ext_rt_val_construct(&src, t_arr.data.success.type)
        == STDCOLT_EXT_RT_VALUE_SUCCESS);

    auto* a = reinterpret_cast<uint32_t*>(src.header.address);
    a[0]    = 10;
    a[1]    = 20;
    a[2]    = 30;
    a[3]    = 40;

    stdcolt_ext_rt_Value dst{};
    auto ok = stdcolt_ext_rt_val_construct_from_copy(&dst, &src);
    CHECK(ok == STDCOLT_EXT_RT_VALUE_SUCCESS);
    CHECK(dst.header.type == t_arr.data.success.type);

    auto* b = reinterpret_cast<uint32_t*>(dst.header.address);
    CHECK(b[0] == 10);
    CHECK(b[1] == 20);
    CHECK(b[2] == 30);
    CHECK(b[3] == 40);

    stdcolt_ext_rt_val_destroy(&dst);
    stdcolt_ext_rt_val_destroy(&src);
  }

  SUBCASE("deep rollback: destroys already-copied members across nesting when inner "
          "copy fails")
  {
    DtorCounter::dtors.store(0);

    // FailCopy (copy always fails)
    stdcolt_ext_rt_NamedLifetime fail_lt{};
    fail_lt.move_fn               = nullptr;
    fail_lt.copy_fn               = +[](const stdcolt_ext_rt_TypeDesc*, void*,
                          const void*) noexcept -> bool { return false; };
    fail_lt.destroy_fn            = nullptr;
    fail_lt.is_trivially_movable  = 1;
    fail_lt.is_trivially_copyable = 0;

    stdcolt_ext_rt_MemberView empty_members{nullptr, 0};
    auto n_fail = sv("FailCopyDeep");
    auto fail   = stdcolt_ext_rt_type_create(
        ctx, &n_fail, &empty_members, 1, 1, &fail_lt, nullptr, nullptr);
    REQUIRE(fail.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    auto* t_fail = fail.data.success.type;

    // DtorCounter named type with copy/destroy
    auto lt_dt = dtorcounter_lifetime();
    auto n_dt  = sv("DtorCounterDeep");
    auto dt    = stdcolt_ext_rt_type_create(
        ctx, &n_dt, &empty_members, alignof(DtorCounter), sizeof(DtorCounter),
        &lt_dt, nullptr, nullptr);
    REQUIRE(dt.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    auto* t_dt = dt.data.success.type;

    // Inner = { DtorCounter b; FailCopy c; }
    stdcolt_ext_rt_MemberInfo inner_mems[] = {
        {sv("b"), sv(""), t_dt},
        {sv("c"), sv(""), t_fail},
    };
    stdcolt_ext_rt_MemberInfoView inner_mv{inner_mems, 2};
    auto n_inner   = sv("InnerDeep");
    auto inner_res = stdcolt_ext_rt_type_create_runtime(
        ctx, &n_inner, &inner_mv, STDCOLT_EXT_RT_LAYOUT_AS_DECLARED, nullptr,
        nullptr);
    REQUIRE(inner_res.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    auto* t_inner = inner_res.data.success.type;

    // Outer = { DtorCounter a; Inner i; }
    stdcolt_ext_rt_MemberInfo outer_mems[] = {
        {sv("a"), sv(""), t_dt},
        {sv("i"), sv(""), t_inner},
    };
    stdcolt_ext_rt_MemberInfoView outer_mv{outer_mems, 2};
    auto n_outer   = sv("OuterDeep");
    auto outer_res = stdcolt_ext_rt_type_create_runtime(
        ctx, &n_outer, &outer_mv, STDCOLT_EXT_RT_LAYOUT_AS_DECLARED, nullptr,
        nullptr);
    REQUIRE(outer_res.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    auto* t_outer = outer_res.data.success.type;

    // Build src
    stdcolt_ext_rt_Value src{};
    REQUIRE(
        stdcolt_ext_rt_val_construct(&src, t_outer) == STDCOLT_EXT_RT_VALUE_SUCCESS);

    auto na    = sv("a");
    auto ni    = sv("i");
    auto off_a = stdcolt_ext_rt_type_lookup(t_outer, &na, t_dt);
    auto off_i = stdcolt_ext_rt_type_lookup(t_outer, &ni, t_inner);
    REQUIRE(off_a.result == STDCOLT_EXT_RT_LOOKUP_FOUND);
    REQUIRE(off_i.result == STDCOLT_EXT_RT_LOOKUP_FOUND);

    void* a_addr = (uint8_t*)src.header.address + found_addr(off_a);
    void* i_addr = (uint8_t*)src.header.address + found_addr(off_i);

    auto nb    = sv("b");
    auto nc    = sv("c");
    auto off_b = stdcolt_ext_rt_type_lookup(t_inner, &nb, t_dt);
    auto off_c = stdcolt_ext_rt_type_lookup(t_inner, &nc, t_fail);
    REQUIRE(off_b.result == STDCOLT_EXT_RT_LOOKUP_FOUND);
    REQUIRE(off_c.result == STDCOLT_EXT_RT_LOOKUP_FOUND);

    void* b_addr = (uint8_t*)i_addr + found_addr(off_b);
    void* c_addr = (uint8_t*)i_addr + found_addr(off_c);

    std::construct_at(reinterpret_cast<DtorCounter*>(a_addr));
    std::construct_at(reinterpret_cast<DtorCounter*>(b_addr));
    *reinterpret_cast<uint8_t*>(c_addr) = 7;

    stdcolt_ext_rt_Value dst{};
    auto ok = stdcolt_ext_rt_val_construct_from_copy(&dst, &src);
    CHECK(ok != STDCOLT_EXT_RT_VALUE_SUCCESS);
    CHECK(dst.header.type == nullptr);
    CHECK(dst.header.address == nullptr);

    CHECK(DtorCounter::dtors.load() == 2); // rollback copies

    stdcolt_ext_rt_val_destroy(&src);
    CHECK(DtorCounter::dtors.load() == 4);
  }

  stdcolt_ext_rt_destroy(ctx);
}
