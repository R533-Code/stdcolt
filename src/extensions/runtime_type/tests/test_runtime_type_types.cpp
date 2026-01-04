#include <doctest/doctest.h>
#include <stdcolt_runtime_type/runtime_type.h>
#include <stdcolt_runtime_type/cpp_bindings.h>

using namespace stdcolt::ext::rt;

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

struct PaddingHeavy
{
  uint8_t a;  // 1
  uint64_t b; // 8
  uint16_t c; // 2
  uint32_t d; // 4
};

struct ManySmall
{
  uint8_t a;
  uint8_t b;
  uint16_t c;
  uint32_t d;
  uint64_t e;
};

static bool is_aligned(uintptr_t offset, uint32_t align)
{
  return align == 0 ? false : (offset % align) == 0;
}

TEST_CASE("stdcolt/extensions/runtime_type")
{
  auto ctx_res = rt_create();
  REQUIRE(ctx_res.result == RuntimeContextResult::RT_SUCCESS);
  auto ctx = ctx_res.success.context;

  SUBCASE("builtin + pointer constness")
  {
    Type ti = type_of<int32_t>(ctx);
    REQUIRE(ti != nullptr);
    CHECK(ti->kind == (uint64_t)TypeKind::KIND_BUILTIN);

    Type p1 = type_of<int32_t*>(ctx);
    Type p2 = type_of<int32_t*>(ctx);
    REQUIRE(p1 != nullptr);
    CHECK(p1 == p2);

    REQUIRE(p1->kind == (uint64_t)TypeKind::KIND_POINTER);
    CHECK(p1->kind_pointer.pointee_type == ti);
    CHECK(p1->kind_pointer.pointee_is_const == 0);

    Type pc = type_of<const int32_t*>(ctx);
    REQUIRE(pc != nullptr);
    REQUIRE(pc->kind == (uint64_t)TypeKind::KIND_POINTER);
    CHECK(pc->kind_pointer.pointee_type == ti);
    CHECK(pc->kind_pointer.pointee_is_const == 1);
    CHECK(pc != p1);

    Type v1 = type_of<void*>(ctx);
    Type v2 = type_of<const void*>(ctx);
    CHECK(v1 != v2);
  }

  SUBCASE("function type dedup")
  {
    using F = int (*)(int32_t, const int32_t*);
    Type t1 = type_of<F>(ctx);
    Type t2 = type_of<F>(ctx);
    REQUIRE(t1 != nullptr);
    CHECK(t1 == t2);

    using G = int (*)(int32_t, int32_t*);
    Type t3 = type_of<G>(ctx);
    REQUIRE(t3 != nullptr);
    CHECK(t1 != t3);
  }

  SUBCASE("bind_type creates named type with field + method and lookup works")
  {
    TypeResult ty_res = bind_type<Foo>(
        ctx, u8"Foo", STDCOLT_RT_FIELD(Foo, x, u8"x", u8"field"),
        STDCOLT_RT_METHOD(Foo, add_ref, u8"add_ref", u8"method"),
        STDCOLT_RT_METHOD(Foo, getx, u8"getx", u8"const method"));

    REQUIRE(ty_res.result == TypeResult::TYPE_SUCCESS);
    auto ty = ty_res.success.type;
    REQUIRE(ty->kind == (uint64_t)TypeKind::KIND_NAMED);

    SUBCASE("field lookup returns offsetof")
    {
      Type expected = type_of<int>(ctx);
      auto r        = rt_type_lookup(ty, {u8"x", 1}, expected);
      REQUIRE(r.result == LookupResult::ResultKind::LOOKUP_FOUND);
      CHECK(r.found.address_or_offset == (uintptr_t)offsetof(Foo, x));
    }

    SUBCASE("method lookup: ABI-lowered thunk is callable")
    {
      using ThunkSig = int (*)(void*, int*, const int*);
      Type expected  = type_of<ThunkSig>(ctx);

      auto r = rt_type_lookup(ty, {u8"add_ref", 7}, expected);
      REQUIRE(r.result == LookupResult::ResultKind::LOOKUP_FOUND);

      auto fn = (ThunkSig)r.found.address_or_offset;
      REQUIRE(fn != nullptr);

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
      Type expected  = type_of<ThunkSig>(ctx);

      auto r = rt_type_lookup(ty, {u8"getx", 4}, expected);
      REQUIRE(r.result == LookupResult::ResultKind::LOOKUP_FOUND);

      auto fn = (ThunkSig)r.found.address_or_offset;
      REQUIRE(fn != nullptr);

      Foo f{};
      f.x = 42;
      CHECK(fn(&f) == 42);
    }

    SUBCASE("lookup mismatch type returns LOOKUP_MISMATCH_TYPE")
    {
      Type wrong = type_of<int64_t>(ctx);
      auto r     = rt_type_lookup(ty, {u8"x", 1}, wrong);
      REQUIRE(r.result == LookupResult::ResultKind::LOOKUP_MISMATCH_TYPE);
      CHECK(r.mismatch_type.actual_type == type_of<int>(ctx));
    }
  }
  SUBCASE("rt_type_create_runtime layout optimize size fast: reduces or matches "
          "size vs declared")
  {
    MemberInfo mems[] = {
        {u8"a", u8"", type_of<uint8_t>(ctx)},
        {u8"b", u8"", type_of<uint64_t>(ctx)},
        {u8"c", u8"", type_of<uint16_t>(ctx)},
        {u8"d", u8"", type_of<uint32_t>(ctx)},
    };

    auto t_decl = rt_type_create_runtime(
        ctx, {u8"PaddingHeavy_decl", 17}, {mems, std::size(mems)},
        RuntimeTypeLayout::LAYOUT_AS_DECLARED);

    REQUIRE(t_decl.result == TypeResult::TYPE_SUCCESS);
    Type decl = t_decl.success.type;

    auto t_opt = rt_type_create_runtime(
        ctx, {u8"PaddingHeavy_opt", 16}, {mems, std::size(mems)},
        RuntimeTypeLayout::LAYOUT_OPTIMIZE_SIZE_FAST);

    REQUIRE(t_opt.result == TypeResult::TYPE_SUCCESS);
    Type opt = t_opt.success.type;

    CHECK(opt->type_size <= decl->type_size);
  }

  SUBCASE("rt_type_create_runtime layout optimize size fast: offsets are aligned "
          "for each member")
  {
    MemberInfo mems[] = {
        {u8"a", u8"", type_of<uint8_t>(ctx)},
        {u8"b", u8"", type_of<uint64_t>(ctx)},
        {u8"c", u8"", type_of<uint16_t>(ctx)},
        {u8"d", u8"", type_of<uint32_t>(ctx)},
    };

    auto t_opt = rt_type_create_runtime(
        ctx, {u8"PaddingHeavy_opt2", 17}, {mems, std::size(mems)},
        RuntimeTypeLayout::LAYOUT_OPTIMIZE_SIZE_FAST);

    REQUIRE(t_opt.result == TypeResult::TYPE_SUCCESS);
    Type ty = t_opt.success.type;

    // for each member, lookup by name and verify offset alignment
    for (auto& m : mems)
    {
      auto r = rt_type_lookup(ty, m.name, m.type);
      REQUIRE(r.result == LookupResult::ResultKind::LOOKUP_FOUND);

      const uint32_t a = m.type->type_align;
      CHECK(is_aligned(r.found.address_or_offset, a));
    }
  }

  SUBCASE("rt_type_create_runtime layout optimize size fast: empty and singleton")
  {
    // Empty members
    {
      auto t = rt_type_create_runtime(
          ctx, {u8"Empty", 5}, {}, RuntimeTypeLayout::LAYOUT_OPTIMIZE_SIZE_FAST);

      REQUIRE(t.result == TypeResult::TYPE_SUCCESS);
      CHECK(t.success.type->type_align == 1);
      CHECK(t.success.type->type_size == 0);
    }

    // Singleton
    {
      MemberInfo mems[] = {
          {u8"e", u8"", type_of<uint64_t>(ctx)},
      };

      auto t = rt_type_create_runtime(
          ctx, {u8"One", 3}, {mems, 1},
          RuntimeTypeLayout::LAYOUT_OPTIMIZE_SIZE_FAST);

      REQUIRE(t.result == TypeResult::TYPE_SUCCESS);
      CHECK(t.success.type->type_align == type_of<uint64_t>(ctx)->type_align);
      CHECK(t.success.type->type_size == type_of<uint64_t>(ctx)->type_size);

      auto r = rt_type_lookup(t.success.type, {u8"e", 2}, mems[0].type);
      REQUIRE(r.result == LookupResult::ResultKind::LOOKUP_FOUND);
      CHECK(r.found.address_or_offset == 0);
    }
  }

  SUBCASE("rt_type_create_runtime invalid layout enum returns TYPE_INVALID_PARAM")
  {
    MemberInfo mems[] = {
        {u8"x", u8"", type_of<int32_t>(ctx)},
    };

    // Force an invalid layout value
    auto bad =
        (RuntimeTypeLayout)((uint64_t)RuntimeTypeLayout::_RuntimeTypeLayout_end + 1);

    auto t = rt_type_create_runtime(
        ctx, {u8"BadLayout", 9}, {mems, 1}, bad, nullptr, nullptr);

    REQUIRE(t.result == TypeResult::TYPE_INVALID_PARAM);
  }

  SUBCASE(
      "rt_type_create_runtime optimize-size-fast: lookup still works for all names")
  {
    MemberInfo mems[] = {
        {u8"a", u8"", type_of<uint8_t>(ctx)},
        {u8"b", u8"", type_of<uint64_t>(ctx)},
        {u8"c", u8"", type_of<uint16_t>(ctx)},
        {u8"d", u8"", type_of<uint32_t>(ctx)},
    };

    auto t_opt = rt_type_create_runtime(
        ctx, {u8"LookupWorks", 10}, {mems, std::size(mems)},
        RuntimeTypeLayout::LAYOUT_OPTIMIZE_SIZE_FAST, nullptr, nullptr);

    REQUIRE(t_opt.result == TypeResult::TYPE_SUCCESS);
    Type ty = t_opt.success.type;

    // Ensure all declared names are found
    for (auto& m : mems)
    {
      auto r = rt_type_lookup(ty, m.name, m.type);
      CHECK(r.result == LookupResult::ResultKind::LOOKUP_FOUND);
    }
  }

  rt_destroy(ctx);
}
