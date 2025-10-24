#include <doctest/doctest.h>
#include <stdcolt_type_erase/type_erase.h>

int allocation_count = 0;

void* malloc_count(size_t size)
{
  auto ptr = std::malloc(size);
  if (ptr != nullptr)
    ++allocation_count;
  return ptr;
}

void free_count(void* ptr)
{
  --allocation_count;
  std::free(ptr);
}

STDCOLT_TYPE_ERASE_DECLARE_TYPE(
    test, IsBinaryFunction, BinaryFunction,
    (stdcolt::type_erase::CustomizeABI{
        .inline_buffer_size    = 16,
        .is_copy_constructible = true,
        .is_move_constructible = true,
        .alloc_fn              = &malloc_count,
        .dealloc_fn            = &free_count}),
    STDCOLT_TYPE_ERASE_CONST_METHOD(double, apply, (double, a), (double, b)),
    STDCOLT_TYPE_ERASE_METHOD(double, apply, (double, a), (double, b)));

STDCOLT_TYPE_ERASE_DECLARE_TEMPLATE_TYPE(
    test, IsBinaryTFunction, BinaryTFunction,
    (stdcolt::type_erase::CustomizeABI{
        .inline_buffer_size    = 16,
        .is_copy_constructible = true,
        .is_move_constructible = true,
        .alloc_fn              = &malloc_count,
        .dealloc_fn            = &free_count}),
    (typename, T), STDCOLT_TYPE_ERASE_CONST_METHOD(T, apply, (T, a), (T, b)),
    STDCOLT_TYPE_ERASE_METHOD(T, apply, (T, a), (T, b)));

struct BinarySum
{
  double apply(double a, double b) const noexcept { return a + b; }
};

struct BinaryProduct
{
  double apply(double a, double b) const noexcept { return a * b; }
};

struct BinaryProductHuge
{
  char big_array[256];

  double apply(double a, double b) const noexcept { return a * b; }
};

TEST_CASE("colt_abi")
{
  using ABIBinaryFn         = test::BinaryFunction;
  using ABIBinaryFnRef      = test::BinaryFunctionRef;
  using ABIBinaryFnConstRef = test::BinaryFunctionConstRef;

  using ABIBinaryTFn         = test::BinaryTFunction<double>;
  using ABIBinaryTFnRef      = test::BinaryTFunctionRef<double>;
  using ABIBinaryTFnConstRef = test::BinaryTFunctionConstRef<double>;

  SUBCASE("non_const")
  {
    auto sum     = ABIBinaryFn{BinarySum{}};
    auto product = ABIBinaryFn{BinaryProduct{}};
    CHECK(sum.apply(2.0, 3.0) == 5.0);
    ABIBinaryFn cpy = std::move(sum);
    CHECK(product.apply(2.0, 3.0) == 6.0);
  }
  SUBCASE("const")
  {
    const auto sum     = ABIBinaryFn{BinarySum{}};
    const auto product = ABIBinaryFn{BinaryProduct{}};
    CHECK(sum.apply(2.0, 3.0) == 5.0);
    CHECK(product.apply(2.0, 3.0) == 6.0);
  }
  SUBCASE("copy_constructor")
  {
    auto sum = ABIBinaryFn{BinarySum{}};
    auto cpy = sum;
    CHECK(cpy.apply(2.0, 3.0) == 5.0);
    CHECK(sum.apply(2.0, 3.0) == 5.0);
  }
  SUBCASE("move_constructor")
  {
    auto obj = BinarySum{};
    auto sum = ABIBinaryFn{std::move(obj)};
    auto cpy = std::move(sum);
    CHECK(cpy.apply(2.0, 3.0) == 5.0);
  }
  SUBCASE("big_object")
  {
    {
      auto obj = BinaryProductHuge{};
      auto sum = ABIBinaryFn{std::move(obj)};
      auto cpy = std::move(sum);
      CHECK(cpy.apply(2.0, 3.0) == 6.0);
    }
    CHECK(allocation_count == 0);
  }
  SUBCASE("big_object_ref")
  {
    {
      auto obj = BinaryProductHuge{};
      auto sum = ABIBinaryFnRef{obj};
      auto cpy = std::move(sum);
      CHECK(cpy.apply(2.0, 3.0) == 6.0);
    }
    CHECK(allocation_count == 0);
  }
  SUBCASE("big_object_const_ref")
  {
    {
      auto obj = BinaryProductHuge{};
      auto sum = ABIBinaryFnConstRef{obj};
      auto cpy = std::move(sum);
      CHECK(cpy.apply(2.0, 3.0) == 6.0);
    }
    CHECK(allocation_count == 0);
  }

  SUBCASE("template non_const")
  {
    auto sum     = ABIBinaryTFn{BinarySum{}};
    auto product = ABIBinaryTFn{BinaryProduct{}};
    CHECK(sum.apply(2.0, 3.0) == 5.0);
    ABIBinaryTFn cpy = std::move(sum);
    CHECK(product.apply(2.0, 3.0) == 6.0);
  }
  SUBCASE("template const")
  {
    const auto sum     = ABIBinaryTFn{BinarySum{}};
    const auto product = ABIBinaryTFn{BinaryProduct{}};
    CHECK(sum.apply(2.0, 3.0) == 5.0);
    CHECK(product.apply(2.0, 3.0) == 6.0);
  }
  SUBCASE("template copy_constructor")
  {
    auto sum = ABIBinaryTFn{BinarySum{}};
    auto cpy = sum;
    CHECK(cpy.apply(2.0, 3.0) == 5.0);
    CHECK(sum.apply(2.0, 3.0) == 5.0);
  }
  SUBCASE("template move_constructor")
  {
    auto obj = BinarySum{};
    auto sum = ABIBinaryTFn{std::move(obj)};
    auto cpy = std::move(sum);
    CHECK(cpy.apply(2.0, 3.0) == 5.0);
  }
  SUBCASE("template big_object")
  {
    {
      auto obj = BinaryProductHuge{};
      auto sum = ABIBinaryTFn{std::move(obj)};
      auto cpy = std::move(sum);
      CHECK(cpy.apply(2.0, 3.0) == 6.0);
    }
    CHECK(allocation_count == 0);
  }
  SUBCASE("template big_object_ref")
  {
    {
      auto obj = BinaryProductHuge{};
      auto sum = ABIBinaryTFnRef{obj};
      auto cpy = std::move(sum);
      CHECK(cpy.apply(2.0, 3.0) == 6.0);
    }
    CHECK(allocation_count == 0);
  }
  SUBCASE("template big_object_const_ref")
  {
    {
      auto obj = BinaryProductHuge{};
      auto sum = ABIBinaryTFnConstRef{obj};
      auto cpy = std::move(sum);
      CHECK(cpy.apply(2.0, 3.0) == 6.0);
    }
    CHECK(allocation_count == 0);
  }
}
