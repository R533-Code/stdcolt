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
    auto val_res1 = Value::make_in_place<uint32_t>(ctx, 10);
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
    REQUIRE(val.reflect_name() == u8"CxxBindingBasicPOD");
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