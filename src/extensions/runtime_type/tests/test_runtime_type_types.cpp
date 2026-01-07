#include <doctest/doctest.h>
#include <stdcolt_runtime_type/cpp/runtime_type.h>
#include <stdcolt_runtime_type/cpp/bindings.h>

using namespace stdcolt::ext::rt;

struct CxxBindingBasicPOD
{
  uint8_t a;
  double b;
  int16_t c;
  uint64_t d;
};

TEST_CASE("stdcolt/extensions/runtime_type: C++ bindings")
{
  auto ctx_res = stdcolt_ext_rt_create(nullptr, nullptr);
  REQUIRE(ctx_res.result == STDCOLT_EXT_RT_CTX_SUCCESS);
  auto ctx = ctx_res.data.success.context;

  SUBCASE("empty checks")
  {
    auto val = Value{};
    REQUIRE(val.is_empty());
    REQUIRE(val.type() == nullptr);
    REQUIRE(val.is_type<int>() == false);
    REQUIRE(val.as_type<int>() == nullptr);

    auto cpy_res = val.copy();
    REQUIRE(cpy_res.has_value());
    auto& cpy = *cpy_res;
    REQUIRE(cpy.is_empty());
    REQUIRE(cpy.type() == nullptr);
    REQUIRE(cpy.is_type<int>() == false);
    REQUIRE(cpy.as_type<int>() == nullptr);
  }
  SUBCASE("basic built-in")
  {
    auto val_res1 = Value::make_in_place<uint32_t>(ctx, 10);
    REQUIRE(val_res1.has_value());
    auto& val = *val_res1;
    REQUIRE(val.is_empty() == false);
    REQUIRE(val.context() == ctx);
    REQUIRE(val.type() == type_of<uint32_t>(ctx));

    auto int_val1 = val.as_type<int32_t>();
    REQUIRE(int_val1 == nullptr);

    auto int_val2 = val.as_type<uint32_t>();
    REQUIRE(int_val2 != nullptr);
    REQUIRE(*int_val2 == 10);

    *int_val2 *= 10;

    int_val2 = val.as_type<uint32_t>();
    REQUIRE(int_val2 != nullptr);
    REQUIRE(*int_val2 == 100);

    auto cpy_res = val.copy();
    REQUIRE(cpy_res.has_value());
    auto& cpy = *cpy_res;

    auto int_val3 = cpy.as_type<uint32_t>();
    REQUIRE(int_val3 != nullptr);
    REQUIRE(int_val3 != int_val2);
    REQUIRE(*int_val2 == *int_val3);
  }
  SUBCASE("basic bind trivial")
  {
    auto _CxxBindingBasicPOD = bind_type<CxxBindingBasicPOD>(
        ctx, u8"CxxBindingBasicPOD",
        STDCOLT_RT_FIELD(CxxBindingBasicPOD, a, u8"a", u8"Hello member a!"),
        STDCOLT_RT_FIELD(CxxBindingBasicPOD, b, u8"b", u8"Hello member b!"),
        STDCOLT_RT_FIELD(CxxBindingBasicPOD, c, u8"c", u8"Hello member c!"),
        STDCOLT_RT_FIELD(CxxBindingBasicPOD, d, u8"d", u8"Hello member d!"));

    REQUIRE(_CxxBindingBasicPOD.result == STDCOLT_EXT_RT_TYPE_SUCCESS);
    auto type = type_of<CxxBindingBasicPOD>(ctx);
    REQUIRE(_CxxBindingBasicPOD.data.success.type == type);
    REQUIRE(type->trivial_copyable);
    REQUIRE(type->trivial_movable);
    REQUIRE(type->trivial_destroy);

    auto val_res = Value::make_in_place<CxxBindingBasicPOD>(ctx, 1, 0.2, 3, 4);
    REQUIRE(val_res.has_value());
    auto& val = *val_res;

    auto ptr = val.as_type<CxxBindingBasicPOD>();
    REQUIRE(ptr != nullptr);
    REQUIRE(ptr->a == 1);
    REQUIRE(ptr->b == 0.2);
    REQUIRE(ptr->c == 3);
    REQUIRE(ptr->d == 4);

    auto a_ptr = val.lookup<uint8_t>(u8"a");
    REQUIRE(a_ptr != nullptr);
    REQUIRE(*a_ptr == 1);
    *a_ptr = 10;

    REQUIRE(ptr->a == 10);

    for (auto [name, desc, type, _] : val.reflect())
    {
      REQUIRE(name.size() == 1);
      REQUIRE(type->kind == STDCOLT_EXT_RT_TYPE_KIND_BUILTIN);
    }
  }

  stdcolt_ext_rt_destroy(ctx);
}

//#include <atomic>
//#include <array>
//#include <cstddef>
//#include <cstdint>
//#include <new>
//
//#include <stdcolt_allocators/allocators/mallocator.h>
//#include <stdcolt_runtime_type/cpp/bindings.h>
//#include <stdcolt_runtime_type/runtime_type.h>
//
//using namespace stdcolt::ext::rt;
//using stdcolt::alloc::MallocatorAligned;
//
//// -----------------------------
//// Test types
//// -----------------------------
//struct Foo
//{
//  int x;
//
//  int add_ref(int& a, const int& b)
//  {
//    a += b;
//    return a;
//  }
//  int getx() const { return x; }
//};
//
//struct PaddingHeavy
//{
//  uint8_t a;  // 1
//  uint64_t b; // 8
//  uint16_t c; // 2
//  uint32_t d; // 4
//};
//
//struct ManySmall
//{
//  uint8_t a;
//  uint8_t b;
//  uint16_t c;
//  uint32_t d;
//  uint64_t e;
//};
//
//// A non-trivial type to test destroy recursion
//struct DtorCounter
//{
//  static inline std::atomic<int> dtors{0};
//  int v{0};
//  ~DtorCounter() { dtors.fetch_add(1); }
//};
//
//// A type that is copyable but NOT nothrow-movable (move may throw => lifetime should omit move_fn)
//struct ThrowMove
//{
//  int x{0};
//  ThrowMove()                 = default;
//  ThrowMove(const ThrowMove&) = default;
//  ThrowMove(ThrowMove&&) noexcept(false) {}
//};
//
//// -----------------------------
//// Small helpers
//// -----------------------------
//static bool is_aligned(uintptr_t offset, uint32_t align)
//{
//  return align == 0 ? false : (offset % align) == 0;
//}
//
//static RuntimeContext* require_ctx()
//{
//  auto r = rt_create();
//  REQUIRE(r.result == RuntimeContextResult::RT_SUCCESS);
//  return r.success.context;
//}
//
//static ResultLookup lookup_found(Type ty, std::u8string_view name, Type expected)
//{
//  auto r = rt_type_lookup(ty, name, expected);
//  REQUIRE(r.result == ResultLookup::ResultKind::LOOKUP_FOUND);
//  return r;
//}
//
//template<class Fn>
//static Fn lookup_thunk(Type ty, std::u8string_view name, RuntimeContext* ctx)
//{
//  Type expected = type_of<Fn>(ctx);
//  auto r        = lookup_found(ty, name, expected);
//  auto fn       = reinterpret_cast<Fn>(r.found.address_or_offset);
//  REQUIRE(fn != nullptr);
//  return fn;
//}
//
//static auto paddingheavy_members(RuntimeContext* ctx)
//{
//  return std::array<MemberInfo, 4>{{
//      {"a", "", type_of<uint8_t>(ctx)},
//      {"b", "", type_of<uint64_t>(ctx)},
//      {"c", "", type_of<uint16_t>(ctx)},
//      {"d", "", type_of<uint32_t>(ctx)},
//  }};
//}
//
//// -----------------------------
//// Allocator override test support
//// -----------------------------
//struct GlobalAllocCounters
//{
//  std::atomic<int> constructs{0};
//  std::atomic<int> destructs{0};
//  std::atomic<int> allocs{0};
//  std::atomic<int> deallocs{0};
//};
//
//static GlobalAllocCounters g_counting;
//
//static size_t align_up_size_t(size_t v, size_t a)
//{
//  return (v + (a - 1)) & ~(a - 1);
//}
//
//static int32_t counting_alloc_construct(void*) noexcept
//{
//  g_counting.constructs.fetch_add(1);
//  return 0;
//}
//static void counting_alloc_destruct(void*) noexcept
//{
//  g_counting.destructs.fetch_add(1);
//}
//static Block counting_alloc_alloc(void*, size_t bytes, size_t align) noexcept
//{
//  g_counting.allocs.fetch_add(1);
//  size_t n = align_up_size_t(bytes, align);
//  void* p  = MallocatorAligned{}.allocate({n, align}).ptr();
//  return Block{p, n};
//}
//static void counting_alloc_dealloc(void*, Block b) noexcept
//{
//  g_counting.deallocs.fetch_add(1);
//  MallocatorAligned{}.deallocate({b.ptr, b.size});
//}
//
//static RecipeAllocator make_global_counting_allocator_recipe() noexcept
//{
//  RecipeAllocator r{};
//  r.allocator_sizeof    = 0;
//  r.allocator_alignof   = 1;
//  r.allocator_construct = &counting_alloc_construct;
//  r.allocator_destruct  = &counting_alloc_destruct;
//  r.allocator_alloc     = &counting_alloc_alloc;
//  r.allocator_dealloc   = &counting_alloc_dealloc;
//  return r;
//}
//
//static RecipePerfectHashFunction make_invalid_phf_recipe() noexcept
//{
//  RecipePerfectHashFunction r = default_perfect_hash_function();
//  r.phf_lookup                = nullptr; // invalid by contract
//  return r;
//}
//
//static RecipeAllocator make_invalid_alloc_recipe() noexcept
//{
//  RecipeAllocator r = default_allocator();
//  r.allocator_alloc = nullptr; // invalid by contract
//  return r;
//}
//
//// -----------------------------
//// Tests
//// -----------------------------
//
//TEST_CASE("stdcolt/extensions/runtime_type: rt_create invalid recipes")
//{
//  {
//    auto bad_alloc = make_invalid_alloc_recipe();
//    auto ok_phf    = default_perfect_hash_function();
//    auto r         = rt_create(&bad_alloc, &ok_phf);
//    CHECK(r.result == RuntimeContextResult::RT_INVALID_ALLOCATOR);
//  }
//  {
//    auto ok_alloc = default_allocator();
//    auto bad_phf  = make_invalid_phf_recipe();
//    auto r        = rt_create(&ok_alloc, &bad_phf);
//    CHECK(r.result == RuntimeContextResult::RT_INVALID_PHF);
//  }
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: type_of caching and identity")
//{
//  RuntimeContext* ctx = require_ctx();
//
//  SUBCASE("builtin kind")
//  {
//    Type ti = type_of<int32_t>(ctx);
//    REQUIRE(ti != nullptr);
//    CHECK(ti->kind == (uint64_t)TypeKind::KIND_BUILTIN);
//  }
//
//  SUBCASE("pointer dedup + pointee constness")
//  {
//    Type ti = type_of<int32_t>(ctx);
//    REQUIRE(ti != nullptr);
//
//    Type p1 = type_of<int32_t*>(ctx);
//    Type p2 = type_of<int32_t*>(ctx);
//    REQUIRE(p1 != nullptr);
//    REQUIRE(p2 != nullptr);
//    CHECK(p1 == p2);
//
//    REQUIRE(p1->kind == (uint64_t)TypeKind::KIND_POINTER);
//    CHECK(p1->kind_pointer.pointee_type == ti);
//    CHECK(p1->kind_pointer.pointee_is_const == 0);
//
//    Type pc = type_of<const int32_t*>(ctx);
//    REQUIRE(pc != nullptr);
//    REQUIRE(pc->kind == (uint64_t)TypeKind::KIND_POINTER);
//    CHECK(pc->kind_pointer.pointee_type == ti);
//    CHECK(pc->kind_pointer.pointee_is_const == 1);
//    CHECK(pc != p1);
//
//    // if void has special-casing, ensure constness still participates
//    Type v1 = type_of<void*>(ctx);
//    Type v2 = type_of<const void*>(ctx);
//    REQUIRE(v1 != nullptr);
//    REQUIRE(v2 != nullptr);
//    CHECK(v1 != v2);
//  }
//
//  SUBCASE("array dedup + extent key")
//  {
//    Type a1 = type_of<int32_t[3]>(ctx);
//    Type a2 = type_of<int32_t[3]>(ctx);
//    Type a3 = type_of<int32_t[4]>(ctx);
//    REQUIRE(a1 != nullptr);
//    REQUIRE(a2 != nullptr);
//    REQUIRE(a3 != nullptr);
//    CHECK(a1 == a2);
//    CHECK(a1 != a3);
//  }
//
//  SUBCASE("function type dedup")
//  {
//    using F = int (*)(int32_t, const int32_t*);
//    Type t1 = type_of<F>(ctx);
//    Type t2 = type_of<F>(ctx);
//    REQUIRE(t1 != nullptr);
//    REQUIRE(t2 != nullptr);
//    CHECK(t1 == t2);
//
//    using G = int (*)(int32_t, int32_t*);
//    Type t3 = type_of<G>(ctx);
//    REQUIRE(t3 != nullptr);
//    CHECK(t1 != t3);
//  }
//
//  rt_destroy(ctx);
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: bind_type exposes members and methods")
//{
//  RuntimeContext* ctx = require_ctx();
//
//  TypeResult ty_res = bind_type<Foo>(
//      ctx, u8"Foo", STDCOLT_RT_FIELD(Foo, x, u8"x", u8"field"),
//      STDCOLT_RT_METHOD(Foo, add_ref, u8"add_ref", u8"method"),
//      STDCOLT_RT_METHOD(Foo, getx, u8"getx", u8"const method"));
//
//  REQUIRE(ty_res.result == TypeResult::TYPE_SUCCESS);
//  Type ty = ty_res.success.type;
//  REQUIRE(ty != nullptr);
//  REQUIRE(ty->kind == (uint64_t)TypeKind::KIND_NAMED);
//
//  SUBCASE("field lookup returns offsetof")
//  {
//    Type expected = type_of<int>(ctx);
//    auto r        = lookup_found(ty, {u8"x", 1}, expected);
//    CHECK(r.found.address_or_offset == (uintptr_t)offsetof(Foo, x));
//  }
//
//  SUBCASE("method lookup: ABI-lowered thunk is callable")
//  {
//    using ThunkSig = int (*)(void*, int*, const int*);
//    auto fn        = lookup_thunk<ThunkSig>(ty, {u8"add_ref", 7}, ctx);
//
//    Foo f{};
//    int a   = 2;
//    int b   = 3;
//    int out = fn(&f, &a, &b);
//    CHECK(out == 5);
//    CHECK(a == 5);
//  }
//
//  SUBCASE("const method lookup: thunk uses const Foo*")
//  {
//    using ThunkSig = int (*)(const void*);
//    auto fn        = lookup_thunk<ThunkSig>(ty, {u8"getx", 4}, ctx);
//
//    Foo f{};
//    f.x = 42;
//    CHECK(fn(&f) == 42);
//  }
//
//  SUBCASE("lookup mismatch type returns LOOKUP_MISMATCH_TYPE")
//  {
//    Type expected = type_of<int>(ctx);
//    Type wrong    = type_of<int64_t>(ctx);
//    REQUIRE(expected != nullptr);
//    REQUIRE(wrong != nullptr);
//
//    auto r = rt_type_lookup(ty, {u8"x", 1}, wrong);
//    REQUIRE(r.result == ResultLookup::ResultKind::LOOKUP_MISMATCH_TYPE);
//    CHECK(r.mismatch_type.actual_type == expected);
//  }
//
//  rt_destroy(ctx);
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: rt_type_create_runtime "
//          "optimize-size-fast invariants")
//{
//  RuntimeContext* ctx = require_ctx();
//
//  SUBCASE(
//      "optimize size fast: size <= declared + aligned member offsets + lookup works")
//  {
//    auto mems = paddingheavy_members(ctx);
//
//    auto t_decl = rt_type_create_runtime(
//        ctx, {u8"PaddingHeavy_decl", 17}, {mems.data(), mems.size()},
//        RuntimeTypeLayout::LAYOUT_AS_DECLARED);
//
//    REQUIRE(t_decl.result == TypeResult::TYPE_SUCCESS);
//    Type decl = t_decl.success.type;
//    REQUIRE(decl != nullptr);
//
//    auto t_opt = rt_type_create_runtime(
//        ctx, {u8"PaddingHeavy_opt", 16}, {mems.data(), mems.size()},
//        RuntimeTypeLayout::LAYOUT_OPTIMIZE_SIZE_FAST);
//
//    REQUIRE(t_opt.result == TypeResult::TYPE_SUCCESS);
//    Type opt = t_opt.success.type;
//    REQUIRE(opt != nullptr);
//
//    CHECK(opt->type_size <= decl->type_size);
//
//    // For each member: lookup must succeed and offset must be aligned to member alignment.
//    for (const auto& m : mems)
//    {
//      auto r = rt_type_lookup(opt, m.name, m.type);
//      REQUIRE(r.result == ResultLookup::ResultKind::LOOKUP_FOUND);
//
//      const uint32_t a = m.type->type_align;
//      CHECK(is_aligned(r.found.address_or_offset, a));
//    }
//  }
//
//  SUBCASE("optimize size fast: empty and singleton")
//  {
//    // Empty members
//    {
//      auto t = rt_type_create_runtime(
//          ctx, {u8"Empty", 5}, {}, RuntimeTypeLayout::LAYOUT_OPTIMIZE_SIZE_FAST);
//
//      REQUIRE(t.result == TypeResult::TYPE_SUCCESS);
//      REQUIRE(t.success.type != nullptr);
//      CHECK(t.success.type->type_align == 1);
//      CHECK(t.success.type->type_size == 0);
//    }
//
//    // Singleton
//    {
//      MemberInfo mems[] = {
//          {u8"e", u8"", type_of<uint64_t>(ctx)},
//      };
//
//      auto t = rt_type_create_runtime(
//          ctx, {u8"One", 3}, {mems, 1},
//          RuntimeTypeLayout::LAYOUT_OPTIMIZE_SIZE_FAST);
//
//      REQUIRE(t.result == TypeResult::TYPE_SUCCESS);
//      REQUIRE(t.success.type != nullptr);
//
//      CHECK(t.success.type->type_align == type_of<uint64_t>(ctx)->type_align);
//      CHECK(t.success.type->type_size == type_of<uint64_t>(ctx)->type_size);
//
//      auto r = rt_type_lookup(t.success.type, {u8"e", 2}, mems[0].type);
//      REQUIRE(r.result == ResultLookup::ResultKind::LOOKUP_FOUND);
//      CHECK(r.found.address_or_offset == 0);
//    }
//  }
//
//  SUBCASE("invalid layout enum returns TYPE_INVALID_PARAM")
//  {
//    MemberInfo mems[] = {
//        {u8"x", u8"", type_of<int32_t>(ctx)},
//    };
//
//    auto bad =
//        (RuntimeTypeLayout)((uint64_t)RuntimeTypeLayout::_RuntimeTypeLayout_end + 1);
//
//    auto t = rt_type_create_runtime(
//        ctx, {u8"BadLayout", 9}, {mems, 1}, bad, nullptr, nullptr);
//
//    CHECK(t.result == TypeResult::TYPE_INVALID_PARAM);
//  }
//
//  rt_destroy(ctx);
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: builtin creation invalid param")
//{
//  RuntimeContext* ctx = require_ctx();
//
//  auto t = rt_type_create_builtin(ctx, (BuiltInType)255); // out of range
//  CHECK(t.result == TypeResult::TYPE_INVALID_PARAM);
//
//  rt_destroy(ctx);
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: rt_type_create_fn validates return type "
//          "owner (void allowed)")
//{
//  RuntimeContext* ctx  = require_ctx();
//  RuntimeContext* ctx2 = require_ctx();
//
//  Type ret_other = type_of<int32_t>(ctx2);
//  Type arg       = type_of<int32_t>(ctx);
//  REQUIRE(ret_other != nullptr);
//  REQUIRE(arg != nullptr);
//
//  // Should reject: return type owned by other ctx
//  Type args[] = {arg};
//  auto bad    = rt_type_create_fn(ctx, ret_other, {args, 1});
//  CHECK(bad.result == TypeResult::TYPE_INVALID_OWNER);
//
//  // void return is ok (nullptr)
//  auto ok = rt_type_create_fn(ctx, nullptr, {args, 1});
//  CHECK(ok.result == TypeResult::TYPE_SUCCESS);
//
//  rt_destroy(ctx2);
//  rt_destroy(ctx);
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: rt_type_create duplicate name -> "
//          "TYPE_FAIL_EXISTS")
//{
//  RuntimeContext* ctx = require_ctx();
//
//  TypeResult t1 =
//      bind_type<Foo>(ctx, u8"FooDup", STDCOLT_RT_FIELD(Foo, x, u8"x", u8""));
//  REQUIRE(t1.result == TypeResult::TYPE_SUCCESS);
//
//  TypeResult t2 =
//      bind_type<Foo>(ctx, u8"FooDup", STDCOLT_RT_FIELD(Foo, x, u8"x", u8""));
//  REQUIRE(t2.result == TypeResult::TYPE_FAIL_EXISTS);
//  CHECK(t2.fail_exists.existing_type == t1.success.type);
//
//  rt_destroy(ctx);
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: lookup_fast vs lookup + prepared member")
//{
//  RuntimeContext* ctx = require_ctx();
//
//  auto ty_res = bind_type<Foo>(
//      ctx, u8"FooLook", STDCOLT_RT_FIELD(Foo, x, u8"x", u8""),
//      STDCOLT_RT_METHOD(Foo, getx, u8"getx", u8""));
//  REQUIRE(ty_res.result == TypeResult::TYPE_SUCCESS);
//  Type ty = ty_res.success.type;
//
//  Type expected = type_of<int>(ctx);
//
//  auto r1 = rt_type_lookup(ty, {u8"x", 1}, expected);
//  REQUIRE(r1.result == ResultLookup::LOOKUP_FOUND);
//
//  auto r2 = rt_type_lookup_fast(ty, {u8"x", 1}, expected);
//  REQUIRE(r2.result == ResultLookup::LOOKUP_FOUND);
//  CHECK(r2.found.address_or_offset == r1.found.address_or_offset);
//
//  PreparedMember pm = rt_prepare_member(ty, {u8"x", 1}, expected);
//  auto r3           = rt_resolve_prepared_member(pm);
//  REQUIRE(r3.result == ResultLookup::LOOKUP_FOUND);
//  CHECK(r3.found.address_or_offset == r1.found.address_or_offset);
//
//  // wrong type => mismatch
//  auto bad = rt_type_lookup_fast(ty, {u8"x", 1}, type_of<int64_t>(ctx));
//  CHECK(bad.result == ResultLookup::LOOKUP_MISMATCH_TYPE);
//
//  // not found
//  auto nf = rt_type_lookup(ty, {u8"nope", 4}, expected);
//  CHECK(nf.result == ResultLookup::LOOKUP_NOT_FOUND);
//
//  rt_destroy(ctx);
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: opaque registration set/get/overwrite")
//{
//  RuntimeContext* ctx = require_ctx();
//
//  Type ti = type_of<int32_t>(ctx);
//  Type tu = type_of<uint32_t>(ctx);
//  REQUIRE(ti != nullptr);
//  REQUIRE(tu != nullptr);
//
//  void* id = (void*)0x1234;
//  CHECK(rt_register_set_type(ctx, id, ti) == true);
//  CHECK(rt_register_get_type(ctx, id) == ti);
//
//  // overwrite
//  CHECK(rt_register_set_type(ctx, id, tu) == true);
//  CHECK(rt_register_get_type(ctx, id) == tu);
//
//  // invalid inputs
//  CHECK(rt_register_set_type(nullptr, id, tu) == false);
//  CHECK(rt_register_set_type(ctx, nullptr, tu) == false);
//  CHECK(rt_register_get_type(nullptr, id) == nullptr);
//
//  rt_destroy(ctx);
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: Value construction modes and destroy "
//          "clears header")
//{
//  RuntimeContext* ctx = require_ctx();
//
//  SUBCASE("empty is noop")
//  {
//    Value v{};
//    val_construct_empty(&v);
//    CHECK(v.header.type == nullptr);
//    CHECK(v.header.address == nullptr);
//    val_destroy(&v);
//    CHECK(v.header.type == nullptr);
//    CHECK(v.header.address == nullptr);
//  }
//
//  SUBCASE("SBO object clears header on destroy")
//  {
//    Value v{};
//    Type t_i32 = type_of<int32_t>(ctx);
//    REQUIRE(t_i32 != nullptr);
//
//    REQUIRE(val_construct(&v, t_i32) == ValueResultKind::VALUE_SUCCESS);
//    CHECK(v.header.type == t_i32);
//    CHECK(v.header.address != nullptr);
//
//    *reinterpret_cast<int32_t*>(v.header.address) = 123;
//
//    val_destroy(&v);
//    CHECK(v.header.type == nullptr);
//    CHECK(v.header.address == nullptr);
//  }
//
//  SUBCASE("heap object clears header on destroy")
//  {
//    Value v{};
//    Type big = type_of<uint8_t[512]>(ctx); // should exceed SBO
//    REQUIRE(big != nullptr);
//
//    REQUIRE(val_construct(&v, big) == ValueResultKind::VALUE_SUCCESS);
//    CHECK(v.header.type == big);
//    CHECK(v.header.address != nullptr);
//
//    val_destroy(&v);
//    CHECK(v.header.type == nullptr);
//    CHECK(v.header.address == nullptr);
//  }
//
//  rt_destroy(ctx);
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: Runtime type lifetime recursion: "
//          "destroy of member runs")
//{
//  RuntimeContext* ctx = require_ctx();
//  DtorCounter::dtors.store(0);
//
//  // bind DtorCounter as a named type so destroy_fn exists
//  auto dt = bind_type<DtorCounter>(
//      ctx, u8"DtorCounter", STDCOLT_RT_FIELD(DtorCounter, v, u8"v", u8""));
//  REQUIRE(dt.result == TypeResult::TYPE_SUCCESS);
//  Type t_dt = dt.success.type;
//
//  // runtime type with one member of that type
//  MemberInfo mems[] = {
//      {u8"m", u8"", t_dt},
//  };
//  auto outer = rt_type_create_runtime(
//      ctx, {u8"Outer", 5}, {mems, 1}, RuntimeTypeLayout::LAYOUT_AS_DECLARED);
//  REQUIRE(outer.result == TypeResult::TYPE_SUCCESS);
//  Type t_outer = outer.success.type;
//
//  // construct a Value holding Outer and placement-new the member
//  Value v{};
//  REQUIRE(val_construct(&v, t_outer) == ValueResultKind::VALUE_SUCCESS);
//
//  auto off = rt_type_lookup(t_outer, {u8"m", 2}, t_dt);
//  REQUIRE(off.result == ResultLookup::LOOKUP_FOUND);
//
//  void* member_addr = (uint8_t*)v.header.address + off.found.address_or_offset;
//  std::construct_at(reinterpret_cast<DtorCounter*>(member_addr));
//
//  val_destroy(&v);
//  CHECK(DtorCounter::dtors.load() == 1);
//
//  rt_destroy(ctx);
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: Copy rollback destroys already-copied "
//          "member on failure")
//{
//  RuntimeContext* ctx = require_ctx();
//  DtorCounter::dtors.store(0);
//
//  // Create a named type FailCopy (size 1) whose copy_fn always fails.
//  NamedLifetime fail_lt{};
//  fail_lt.move_fn = nullptr;
//  fail_lt.copy_fn =
//      +[](const TypeDesc*, void*, const void*) noexcept -> bool { return false; };
//  fail_lt.destroy_fn            = nullptr;
//  fail_lt.is_trivially_movable  = 1;
//  fail_lt.is_trivially_copyable = 0;
//
//  auto fail =
//      rt_type_create(ctx, {u8"FailCopy", 8}, {(Member*)nullptr, 0}, 1, 1, &fail_lt);
//  REQUIRE(fail.result == TypeResult::TYPE_SUCCESS);
//  Type t_fail = fail.success.type;
//
//  auto dt = bind_type<DtorCounter>(
//      ctx, u8"DtorCounter2", STDCOLT_RT_FIELD(DtorCounter, v, u8"v", u8""));
//  REQUIRE(dt.result == TypeResult::TYPE_SUCCESS);
//  Type t_dt = dt.success.type;
//
//  // Outer = { DtorCounter a; FailCopy b; }
//  MemberInfo mems[] = {
//      {u8"a", u8"", t_dt},
//      {u8"b", u8"", t_fail},
//  };
//  auto outer = rt_type_create_runtime(
//      ctx, {u8"OuterRollback", 12}, {mems, 2},
//      RuntimeTypeLayout::LAYOUT_AS_DECLARED);
//  REQUIRE(outer.result == TypeResult::TYPE_SUCCESS);
//  Type t_outer = outer.success.type;
//
//  // Create a source value and construct both members
//  Value src{};
//  REQUIRE(val_construct(&src, t_outer) == ValueResultKind::VALUE_SUCCESS);
//
//  auto off_a = rt_type_lookup(t_outer, {u8"a", 2}, t_dt);
//  auto off_b = rt_type_lookup(t_outer, {u8"b", 2}, t_fail);
//  REQUIRE(off_a.result == ResultLookup::LOOKUP_FOUND);
//  REQUIRE(off_b.result == ResultLookup::LOOKUP_FOUND);
//
//  void* a_addr = (uint8_t*)src.header.address + off_a.found.address_or_offset;
//  void* b_addr = (uint8_t*)src.header.address + off_b.found.address_or_offset;
//
//  std::construct_at(reinterpret_cast<DtorCounter*>(a_addr));
//  *reinterpret_cast<uint8_t*>(b_addr) = 7;
//
//  // Copy should fail and rollback should destroy already-copied `a`
//  Value dst{};
//  auto ok = val_construct_from_copy(&dst, &src);
//  CHECK(ok != ValueResultKind::VALUE_SUCCESS);
//  CHECK(dst.header.type == nullptr);
//  CHECK(dst.header.address == nullptr);
//  CHECK(DtorCounter::dtors.load() == 1);
//
//  val_destroy(&src);
//  rt_destroy(ctx);
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: Custom allocator override used by Value "
//          "(and arrays recurse to element allocator)")
//{
//  RuntimeContext* ctx = require_ctx();
//
//  g_counting.constructs.store(0);
//  g_counting.destructs.store(0);
//  g_counting.allocs.store(0);
//  g_counting.deallocs.store(0);
//
//  RecipeAllocator counting = make_global_counting_allocator_recipe();
//  auto phf                 = default_perfect_hash_function();
//
//  NamedLifetime triv_lt{};
//  triv_lt.move_fn               = nullptr;
//  triv_lt.copy_fn               = nullptr;
//  triv_lt.destroy_fn            = nullptr;
//  triv_lt.is_trivially_movable  = 1;
//  triv_lt.is_trivially_copyable = 1;
//
//  // Create named types with allocator override (construct called per type).
//  auto tn = rt_type_create(
//      ctx, {u8"AllocNamed", 10}, {(Member*)nullptr, 0}, 8, 64, &triv_lt, &counting,
//      &phf);
//  REQUIRE(tn.result == TypeResult::TYPE_SUCCESS);
//  Type t_named = tn.success.type;
//  REQUIRE(t_named != nullptr);
//  CHECK(g_counting.constructs.load() == 1);
//
//  // Force heap allocation: use a large size
//  auto tn_big = rt_type_create(
//      ctx, {u8"AllocNamedBig", 13}, {(Member*)nullptr, 0}, 8, 256, &triv_lt,
//      &counting, &phf);
//  REQUIRE(tn_big.result == TypeResult::TYPE_SUCCESS);
//  Type t_big = tn_big.success.type;
//  REQUIRE(t_big != nullptr);
//  CHECK(g_counting.constructs.load() == 2);
//
//  // 1) Value of the named type should allocate via override allocator (heap expected)
//  {
//    int a0 = g_counting.allocs.load();
//    int d0 = g_counting.deallocs.load();
//
//    Value v1{};
//    REQUIRE(val_construct(&v1, t_big) == ValueResultKind::VALUE_SUCCESS);
//    CHECK(g_counting.allocs.load() == a0 + 1);
//
//    val_destroy(&v1);
//    CHECK(g_counting.deallocs.load() == d0 + 1);
//  }
//
//  // 2) Array of named should recurse to element allocator and also use override allocator
//  {
//    auto arr_res = rt_type_create_array(ctx, t_big, 4);
//    REQUIRE(arr_res.result == TypeResult::TYPE_SUCCESS);
//    Type t_arr = arr_res.success.type;
//    REQUIRE(t_arr != nullptr);
//
//    int a1 = g_counting.allocs.load();
//    int d1 = g_counting.deallocs.load();
//
//    Value v2{};
//    REQUIRE(val_construct(&v2, t_arr) == ValueResultKind::VALUE_SUCCESS);
//    CHECK(g_counting.allocs.load() == a1 + 1);
//
//    val_destroy(&v2);
//    CHECK(g_counting.deallocs.load() == d1 + 1);
//  }
//
//  // 3) Builtin Value should not use counting allocator (SBO anyway; verify no counting alloc)
//  {
//    Type t_u64 = type_of<uint64_t>(ctx);
//    REQUIRE(t_u64 != nullptr);
//
//    int a2 = g_counting.allocs.load();
//
//    Value v3{};
//    REQUIRE(val_construct(&v3, t_u64) == ValueResultKind::VALUE_SUCCESS);
//    CHECK(g_counting.allocs.load() == a2);
//
//    val_destroy(&v3);
//  }
//
//  // Destroying ctx should destruct override allocator state for each named type created with it.
//  rt_destroy(ctx);
//  CHECK(g_counting.destructs.load() == 2);
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: lookup on non-named types returns "
//          "LOOKUP_EXPECTED_NAMED")
//{
//  RuntimeContext* ctx = require_ctx();
//
//  Type t_i32 = type_of<int32_t>(ctx);
//  Type t_ptr = type_of<int32_t*>(ctx);
//  Type t_arr = type_of<int32_t[3]>(ctx);
//  using F    = int (*)(int32_t);
//  Type t_fn  = type_of<F>(ctx);
//
//  REQUIRE(t_i32 != nullptr);
//  REQUIRE(t_ptr != nullptr);
//  REQUIRE(t_arr != nullptr);
//  REQUIRE(t_fn != nullptr);
//
//  Type expected = type_of<int32_t>(ctx);
//  REQUIRE(expected != nullptr);
//
//  for (Type t : {t_i32, t_ptr, t_arr, t_fn})
//  {
//    auto r = rt_type_lookup(t, {u8"x", 1}, expected);
//    CHECK(r.result == ResultLookup::ResultKind::LOOKUP_EXPECTED_NAMED);
//
//    auto rf = rt_type_lookup_fast(t, {u8"x", 1}, expected);
//    CHECK(rf.result == ResultLookup::ResultKind::LOOKUP_EXPECTED_NAMED);
//  }
//
//  rt_destroy(ctx);
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: rt_type_create_ptr/array/fn invalid "
//          "param and invalid owner matrices")
//{
//  RuntimeContext* ctx  = require_ctx();
//  RuntimeContext* ctx2 = require_ctx();
//
//  Type t_i32  = type_of<int32_t>(ctx);
//  Type t_i32b = type_of<int32_t>(ctx2);
//  REQUIRE(t_i32 != nullptr);
//  REQUIRE(t_i32b != nullptr);
//
//  SUBCASE("ptr: null pointee -> TYPE_INVALID_PARAM")
//  {
//    auto r = rt_type_create_ptr(ctx, nullptr, false);
//    CHECK(r.result == TypeResult::ResultKind::TYPE_INVALID_PARAM);
//  }
//
//  SUBCASE("ptr: pointee owned by other ctx -> TYPE_INVALID_OWNER")
//  {
//    auto r = rt_type_create_ptr(ctx, t_i32b, false);
//    CHECK(r.result == TypeResult::ResultKind::TYPE_INVALID_OWNER);
//  }
//
//  SUBCASE("array: null element -> TYPE_INVALID_PARAM")
//  {
//    auto r = rt_type_create_array(ctx, nullptr, 3);
//    CHECK(r.result == TypeResult::ResultKind::TYPE_INVALID_PARAM);
//  }
//
//  SUBCASE("array: element owned by other ctx -> TYPE_INVALID_OWNER")
//  {
//    auto r = rt_type_create_array(ctx, t_i32b, 3);
//    CHECK(r.result == TypeResult::ResultKind::TYPE_INVALID_OWNER);
//  }
//
//  SUBCASE("fn: args contains nullptr -> TYPE_INVALID_PARAM")
//  {
//    Type args[] = {t_i32, nullptr};
//    auto r      = rt_type_create_fn(ctx, t_i32, {args, 2});
//    CHECK(r.result == TypeResult::ResultKind::TYPE_INVALID_PARAM);
//  }
//
//  SUBCASE("fn: args contains type owned by other ctx -> TYPE_INVALID_OWNER")
//  {
//    Type args[] = {t_i32, t_i32b};
//    auto r      = rt_type_create_fn(ctx, t_i32, {args, 2});
//    CHECK(r.result == TypeResult::ResultKind::TYPE_INVALID_OWNER);
//  }
//
//  SUBCASE("fn: return type owned by other ctx -> TYPE_INVALID_OWNER")
//  {
//    Type args[] = {t_i32};
//    auto r      = rt_type_create_fn(ctx, t_i32b, {args, 1});
//    CHECK(r.result == TypeResult::ResultKind::TYPE_INVALID_OWNER);
//  }
//
//  rt_destroy(ctx2);
//  rt_destroy(ctx);
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: Value move steals storage and preserves "
//          "bits (SBO and heap)")
//{
//  RuntimeContext* ctx = require_ctx();
//
//  SUBCASE("SBO: int32_t moved value preserved; source becomes empty")
//  {
//    Type t_i32 = type_of<int32_t>(ctx);
//    REQUIRE(t_i32 != nullptr);
//
//    Value src{};
//    REQUIRE(val_construct(&src, t_i32) == ValueResultKind::VALUE_SUCCESS);
//    *reinterpret_cast<int32_t*>(src.header.address) = 123;
//
//    Value dst{};
//    val_construct_from_move(&dst, &src);
//
//    CHECK(src.header.type == nullptr);
//    CHECK(src.header.address == nullptr);
//
//    CHECK(dst.header.type == t_i32);
//    REQUIRE(dst.header.address != nullptr);
//    CHECK(*reinterpret_cast<int32_t*>(dst.header.address) == 123);
//
//    val_destroy(&dst);
//    val_destroy(&src); // noop
//  }
//
//  SUBCASE("heap: big array storage pointer is stolen; bytes preserved; source "
//          "becomes empty")
//  {
//    Type big = type_of<uint8_t[512]>(ctx);
//    REQUIRE(big != nullptr);
//
//    Value src{};
//    REQUIRE(val_construct(&src, big) == ValueResultKind::VALUE_SUCCESS);
//
//    auto* p = reinterpret_cast<uint8_t*>(src.header.address);
//    p[0]    = 0xAB;
//    p[511]  = 0xCD;
//
//    void* stolen_ptr = src.header.address;
//
//    Value dst{};
//    val_construct_from_move(&dst, &src);
//
//    CHECK(src.header.type == nullptr);
//    CHECK(src.header.address == nullptr);
//
//    CHECK(dst.header.type == big);
//    REQUIRE(dst.header.address != nullptr);
//    CHECK(dst.header.address == stolen_ptr);
//
//    auto* q = reinterpret_cast<uint8_t*>(dst.header.address);
//    CHECK(q[0] == 0xAB);
//    CHECK(q[511] == 0xCD);
//
//    val_destroy(&dst);
//    val_destroy(&src); // noop
//  }
//
//  rt_destroy(ctx);
//}
//
//TEST_CASE("stdcolt/extensions/runtime_type: val_construct_from_copy success + deep "
//          "rollback recursion")
//{
//  RuntimeContext* ctx = require_ctx();
//
//  SUBCASE("copy success: trivially copyable named type")
//  {
//    NamedLifetime triv_lt{};
//    triv_lt.move_fn               = nullptr;
//    triv_lt.copy_fn               = nullptr;
//    triv_lt.destroy_fn            = nullptr;
//    triv_lt.is_trivially_movable  = 1;
//    triv_lt.is_trivially_copyable = 1;
//
//    auto t_res = rt_type_create(
//        ctx, {u8"TrivNamed8", 10}, {(Member*)nullptr, 0}, 8, 8, &triv_lt);
//    REQUIRE(t_res.result == TypeResult::ResultKind::TYPE_SUCCESS);
//    Type t = t_res.success.type;
//    REQUIRE(t != nullptr);
//
//    Value src{};
//    REQUIRE(val_construct(&src, t) == ValueResultKind::VALUE_SUCCESS);
//    auto* ps = reinterpret_cast<uint8_t*>(src.header.address);
//    for (int i = 0; i < 8; ++i)
//      ps[i] = static_cast<uint8_t>(0xA0 + i);
//
//    Value dst{};
//    auto ok = val_construct_from_copy(&dst, &src);
//    CHECK(ok == ValueResultKind::VALUE_SUCCESS);
//    CHECK(dst.header.type == t);
//    REQUIRE(dst.header.address != nullptr);
//
//    auto* pd = reinterpret_cast<uint8_t*>(dst.header.address);
//    for (int i = 0; i < 8; ++i)
//      CHECK(pd[i] == static_cast<uint8_t>(0xA0 + i));
//
//    val_destroy(&dst);
//    val_destroy(&src);
//  }
//
//  SUBCASE("copy success: trivially copyable builtin array")
//  {
//    Type t_arr = type_of<uint32_t[4]>(ctx);
//    REQUIRE(t_arr != nullptr);
//
//    Value src{};
//    REQUIRE(val_construct(&src, t_arr) == ValueResultKind::VALUE_SUCCESS);
//
//    auto* a = reinterpret_cast<uint32_t*>(src.header.address);
//    a[0]    = 10;
//    a[1]    = 20;
//    a[2]    = 30;
//    a[3]    = 40;
//
//    Value dst{};
//    auto ok = val_construct_from_copy(&dst, &src);
//    CHECK(ok == ValueResultKind::VALUE_SUCCESS);
//    CHECK(dst.header.type == t_arr);
//
//    auto* b = reinterpret_cast<uint32_t*>(dst.header.address);
//    CHECK(b[0] == 10);
//    CHECK(b[1] == 20);
//    CHECK(b[2] == 30);
//    CHECK(b[3] == 40);
//
//    val_destroy(&dst);
//    val_destroy(&src);
//  }
//
//  SUBCASE("deep rollback: destroys already-copied members across nesting when inner "
//          "copy fails")
//  {
//    DtorCounter::dtors.store(0);
//
//    // FailCopy (copy always fails)
//    NamedLifetime fail_lt{};
//    fail_lt.move_fn = nullptr;
//    fail_lt.copy_fn =
//        +[](const TypeDesc*, void*, const void*) noexcept -> bool { return false; };
//    fail_lt.destroy_fn            = nullptr;
//    fail_lt.is_trivially_movable  = 1;
//    fail_lt.is_trivially_copyable = 0;
//
//    auto fail = rt_type_create(
//        ctx, {u8"FailCopyDeep", 12}, {(Member*)nullptr, 0}, 1, 1, &fail_lt);
//    REQUIRE(fail.result == TypeResult::ResultKind::TYPE_SUCCESS);
//    Type t_fail = fail.success.type;
//
//    // DtorCounter named type (non-trivial dtor via bind_type)
//    auto dt = bind_type<DtorCounter>(
//        ctx, u8"DtorCounterDeep", STDCOLT_RT_FIELD(DtorCounter, v, u8"v", u8""));
//    REQUIRE(dt.result == TypeResult::ResultKind::TYPE_SUCCESS);
//    Type t_dt = dt.success.type;
//
//    // Inner = { DtorCounter b; FailCopy c; }
//    MemberInfo inner_mems[] = {
//        {u8"b", u8"", t_dt},
//        {u8"c", u8"", t_fail},
//    };
//    auto inner_res = rt_type_create_runtime(
//        ctx, {u8"InnerDeep", 9}, {inner_mems, 2},
//        RuntimeTypeLayout::LAYOUT_AS_DECLARED);
//    REQUIRE(inner_res.result == TypeResult::ResultKind::TYPE_SUCCESS);
//    Type t_inner = inner_res.success.type;
//
//    // Outer = { DtorCounter a; Inner i; }
//    MemberInfo outer_mems[] = {
//        {u8"a", u8"", t_dt},
//        {u8"i", u8"", t_inner},
//    };
//    auto outer_res = rt_type_create_runtime(
//        ctx, {u8"OuterDeep", 9}, {outer_mems, 2},
//        RuntimeTypeLayout::LAYOUT_AS_DECLARED);
//    REQUIRE(outer_res.result == TypeResult::ResultKind::TYPE_SUCCESS);
//    Type t_outer = outer_res.success.type;
//
//    // Build src with constructed a and inner.b; set inner.c byte
//    Value src{};
//    REQUIRE(val_construct(&src, t_outer) == ValueResultKind::VALUE_SUCCESS);
//
//    auto off_a = rt_type_lookup(t_outer, {u8"a", 2}, t_dt);
//    auto off_i = rt_type_lookup(t_outer, {u8"i", 2}, t_inner);
//    REQUIRE(off_a.result == ResultLookup::ResultKind::LOOKUP_FOUND);
//    REQUIRE(off_i.result == ResultLookup::ResultKind::LOOKUP_FOUND);
//
//    void* a_addr = (uint8_t*)src.header.address + off_a.found.address_or_offset;
//    void* i_addr = (uint8_t*)src.header.address + off_i.found.address_or_offset;
//
//    auto off_b = rt_type_lookup(t_inner, {u8"b", 2}, t_dt);
//    auto off_c = rt_type_lookup(t_inner, {u8"c", 2}, t_fail);
//    REQUIRE(off_b.result == ResultLookup::ResultKind::LOOKUP_FOUND);
//    REQUIRE(off_c.result == ResultLookup::ResultKind::LOOKUP_FOUND);
//
//    void* b_addr = (uint8_t*)i_addr + off_b.found.address_or_offset;
//    void* c_addr = (uint8_t*)i_addr + off_c.found.address_or_offset;
//
//    std::construct_at(reinterpret_cast<DtorCounter*>(a_addr));
//    std::construct_at(reinterpret_cast<DtorCounter*>(b_addr));
//    *reinterpret_cast<uint8_t*>(c_addr) = 7;
//
//    // Copy should fail at inner.c and rollback should destroy copies of a and b in dst attempt
//    Value dst{};
//    auto ok = val_construct_from_copy(&dst, &src);
//    CHECK(ok != ValueResultKind::VALUE_SUCCESS);
//    CHECK(dst.header.type == nullptr);
//    CHECK(dst.header.address == nullptr);
//
//    CHECK(DtorCounter::dtors.load() == 2); // destroyed rollback copies (a', b')
//
//    // Destroy src and ensure its two dtors also run
//    val_destroy(&src);
//    CHECK(DtorCounter::dtors.load() == 4);
//  }
//
//  rt_destroy(ctx);
//}
