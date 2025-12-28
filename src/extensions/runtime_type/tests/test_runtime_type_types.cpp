#include <doctest/doctest.h>
#include <stdcolt_runtime_type/runtime_type.h>

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

TEST_CASE("stdcolt/extensions/runtime_type")
{
  RuntimeTypeContext* ctx = rt_create();
  REQUIRE(ctx != nullptr);

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
    Type ty = bind_type<Foo>(
        ctx, u8"Foo", STDCOLT_RT_FIELD(Foo, x, u8"x", u8"field"),
        STDCOLT_RT_METHOD(Foo, add_ref, u8"add_ref", u8"method"),
        STDCOLT_RT_METHOD(Foo, getx, u8"getx", u8"const method"));

    REQUIRE(ty != nullptr);
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

  rt_destroy(ctx);
}
