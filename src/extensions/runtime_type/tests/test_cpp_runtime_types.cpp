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
    auto val = Any{};
    REQUIRE(val.is_empty());
    REQUIRE(val.type() == nullptr);
    REQUIRE(val.is_type<int>() == false);
    REQUIRE(val.as_type<int>() == nullptr);
    REQUIRE(val.reflect_name() == u8"");

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
    auto val_res1 = Any::make_in_place<uint32_t>(ctx, 10);
    REQUIRE(val_res1.has_value());
    auto& val = *val_res1;
    REQUIRE(val.is_empty() == false);
    REQUIRE(val.context() == ctx);
    REQUIRE(val.type() == type_of<uint32_t>(ctx));
    REQUIRE(val.reflect_name() == u8"");

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
  SUBCASE("basic array built-in")
  {
    static constexpr char STR[] = "hello";
    auto val_res1 =
        Any::make_in_place<char[sizeof(STR)]>(ctx, 'h', 'e', 'l', 'l', 'o', '\0');
    REQUIRE(val_res1.has_value());
    auto& val = *val_res1;
    REQUIRE(val.is_empty() == false);
    REQUIRE(val.context() == ctx);
    REQUIRE(val.type() == type_of<char[sizeof(STR)]>(ctx));
    REQUIRE(val.reflect_name() == u8"");

    auto int_val1 = val.as_type<int32_t>();
    REQUIRE(int_val1 == nullptr);

    auto int_val2 = val.as_type<char[sizeof(STR)]>();
    REQUIRE(int_val2 != nullptr);
    REQUIRE(doctest::String{int_val2} == doctest::String{STR});

    auto cpy_res = val.copy();
    REQUIRE(cpy_res.has_value());
    auto& cpy = *cpy_res;

    auto int_val3 = cpy.as_type<char[sizeof(STR)]>();
    REQUIRE(int_val3 != nullptr);
    REQUIRE(int_val3 != int_val2);
    REQUIRE(*int_val2 == *int_val3);
    auto span = val.as_span<char>();
    REQUIRE(span.size() == sizeof(STR));
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

    auto val_res = Any::make_in_place<CxxBindingBasicPOD>(ctx, 1, 0.2, 3, 4);
    REQUIRE(val_res.has_value());
    auto& val = *val_res;

    auto ptr = val.as_type<CxxBindingBasicPOD>();
    REQUIRE(val.reflect_name() == u8"CxxBindingBasicPOD");
    REQUIRE(ptr != nullptr);
    REQUIRE(ptr->a == 1);
    REQUIRE(ptr->b == 0.2);
    REQUIRE(ptr->c == 3);
    REQUIRE(ptr->d == 4);

    auto a_ptr = val.lookup_field<uint8_t>(u8"a");
    REQUIRE(a_ptr != nullptr);
    REQUIRE(*a_ptr == 1);
    *a_ptr = 10;

    REQUIRE(ptr->a == 10);

    auto span = val.as_span<CxxBindingBasicPOD>();
    REQUIRE(span.size() == 1);
    REQUIRE(span[0].a == 10);

    for (auto [name, desc, type, kind, _] : val.reflect())
    {
      REQUIRE(name.size() == 1);
      REQUIRE(type->kind == STDCOLT_EXT_RT_TYPE_KIND_BUILTIN);
    }
  }

  stdcolt_ext_rt_destroy(ctx);
}

TEST_CASE("stdcolt/extensions/runtime_type: C++ bindings (SharedAny/WeakAny)")
{
  auto ctx_res = stdcolt_ext_rt_create(nullptr, nullptr);
  REQUIRE(ctx_res.result == STDCOLT_EXT_RT_CTX_SUCCESS);
  auto ctx = ctx_res.data.success.context;

  // Reuse the same bound type from your Any tests (needed for reflect_name on named types)
  auto _CxxBindingBasicPOD = bind_type<CxxBindingBasicPOD>(
      ctx, u8"CxxBindingBasicPOD",
      STDCOLT_RT_FIELD(CxxBindingBasicPOD, a, u8"a", u8"Hello member a!"),
      STDCOLT_RT_FIELD(CxxBindingBasicPOD, b, u8"b", u8"Hello member b!"),
      STDCOLT_RT_FIELD(CxxBindingBasicPOD, c, u8"c", u8"Hello member c!"),
      STDCOLT_RT_FIELD(CxxBindingBasicPOD, d, u8"d", u8"Hello member d!"));
  REQUIRE(_CxxBindingBasicPOD.result == STDCOLT_EXT_RT_TYPE_SUCCESS);

  SUBCASE("SharedAny empty checks")
  {
    SharedAny s{};
    REQUIRE(s.is_empty());
    REQUIRE(s.type() == nullptr);
    REQUIRE(s.context() == nullptr);
    REQUIRE(s.as_type<int>() == nullptr);
    REQUIRE(s.is_type<int>() == false);

    WeakAny w{};
    REQUIRE(w.is_empty());
    auto locked = w.try_lock();
    REQUIRE(locked.is_empty());
  }

  SUBCASE("SharedAny basic built-in, copy increments strong, weak lock works")
  {
    auto s_res = SharedAny::make_in_place<uint32_t>(ctx, 10);
    REQUIRE(s_res.has_value());
    auto s1 = std::move(*s_res);

    REQUIRE(!s1.is_empty());
    REQUIRE(s1.context() == ctx);
    REQUIRE(s1.type() == type_of<uint32_t>(ctx));

    auto p1 = s1.as_type<uint32_t>();
    REQUIRE(p1 != nullptr);
    REQUIRE(*p1 == 10);

    // Copy: should alias same object (shared ownership)
    SharedAny s2 = s1;
    REQUIRE(!s2.is_empty());
    auto p2 = s2.as_type<uint32_t>();
    REQUIRE(p2 != nullptr);
    REQUIRE(p2 == p1);

    *p2 = 99;
    REQUIRE(*p1 == 99);

    // Weak observe + lock
    WeakAny w1{s1};
    REQUIRE(!w1.is_empty());
    auto s3 = w1.try_lock();
    REQUIRE(!s3.is_empty());
    auto p3 = s3.as_type<uint32_t>();
    REQUIRE(p3 != nullptr);
    REQUIRE(p3 == p1);
    REQUIRE(*p3 == 99);
  }

  SUBCASE("SharedAny move is bitwise move + empties source (no refcount traffic)")
  {
    auto s_res = SharedAny::make_in_place<uint32_t>(ctx, 123);
    REQUIRE(s_res.has_value());
    SharedAny a = std::move(*s_res);

    REQUIRE(!a.is_empty());
    auto pa = a.as_type<uint32_t>();
    REQUIRE(pa != nullptr);
    REQUIRE(*pa == 123);

    // Move-construct: moved-from becomes empty
    SharedAny b = std::move(a);
    REQUIRE(a.is_empty());
    REQUIRE(!b.is_empty());
    auto pb = b.as_type<uint32_t>();
    REQUIRE(pb != nullptr);
    REQUIRE(*pb == 123);

    // Move-assign: moved-from becomes empty
    SharedAny c{};
    c = std::move(b);
    REQUIRE(b.is_empty());
    REQUIRE(!c.is_empty());
    auto pc = c.as_type<uint32_t>();
    REQUIRE(pc != nullptr);
    REQUIRE(*pc == 123);
  }

  SUBCASE("WeakAny lock fails after last SharedAny is destroyed (expired)")
  {
    WeakAny w{};
    {
      auto s_res = SharedAny::make_in_place<uint32_t>(ctx, 7);
      REQUIRE(s_res.has_value());
      SharedAny s = std::move(*s_res);

      w = WeakAny{s};
      REQUIRE(!w.is_empty());

      auto alive = w.try_lock();
      REQUIRE(!alive.is_empty());
      REQUIRE(alive.as_type<uint32_t>() != nullptr);
      REQUIRE(*alive.as_type<uint32_t>() == 7);
    } // s destroyed here, strong reaches 0

    auto expired = w.try_lock();
    REQUIRE(expired.is_empty());
  }

  SUBCASE("WeakAny try_lock_consume empties weak on success")
  {
    auto s_res = SharedAny::make_in_place<uint32_t>(ctx, 55);
    REQUIRE(s_res.has_value());
    SharedAny s = std::move(*s_res);

    WeakAny w{s};
    REQUIRE(!w.is_empty());

    auto got = w.try_lock_consume();
    REQUIRE(!got.is_empty());
    REQUIRE(got.as_type<uint32_t>() != nullptr);
    REQUIRE(*got.as_type<uint32_t>() == 55);

    REQUIRE(w.is_empty()); // consumed
  }

  SUBCASE("WeakAny move is bitwise move + empties source")
  {
    auto s_res = SharedAny::make_in_place<uint32_t>(ctx, 1);
    REQUIRE(s_res.has_value());
    SharedAny s = std::move(*s_res);

    WeakAny w1{s};
    REQUIRE(!w1.is_empty());

    WeakAny w2 = std::move(w1);
    REQUIRE(w1.is_empty());
    REQUIRE(!w2.is_empty());

    WeakAny w3{};
    w3 = std::move(w2);
    REQUIRE(w2.is_empty());
    REQUIRE(!w3.is_empty());

    auto locked = w3.try_lock();
    REQUIRE(!locked.is_empty());
    REQUIRE(locked.as_type<uint32_t>() != nullptr);
    REQUIRE(*locked.as_type<uint32_t>() == 1);
  }

  SUBCASE("SharedAny member lookup + reflection on named type")
  {
    auto s_res = SharedAny::make_in_place<CxxBindingBasicPOD>(ctx, 1, 0.2, 3, 4);
    REQUIRE(s_res.has_value());
    auto s = std::move(*s_res);

    REQUIRE(!s.is_empty());
    REQUIRE(s.type() == type_of<CxxBindingBasicPOD>(ctx));

    auto ptr = s.as_type<CxxBindingBasicPOD>();
    REQUIRE(ptr != nullptr);
    REQUIRE(ptr->a == 1);
    REQUIRE(ptr->b == 0.2);
    REQUIRE(ptr->c == 3);
    REQUIRE(ptr->d == 4);

    // member lookup through wrapper
    auto a_ptr = s.lookup_field<uint8_t>(u8"a");
    REQUIRE(a_ptr != nullptr);
    REQUIRE(*a_ptr == 1);
    *a_ptr = 10;
    REQUIRE(ptr->a == 10);
  }

  stdcolt_ext_rt_destroy(ctx);
}
