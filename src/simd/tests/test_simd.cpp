#include <doctest/doctest.h>
#include <stdcolt_simd/simd.h>
#include <vector>
#include <random>
#include <cstring>
#include <cmath>
#include <limits>

#define ALL_INT_T \
  uint8_t, uint16_t, uint32_t, uint64_t, int8_t, int16_t, int32_t, int64_t
#define ALL_FP_T float, double
#define OVERLOAD_AS_LAMBDA(fn)                        \
  [](size_t n, const auto* a, const auto* b, auto* o) \
  {                                                   \
    using T = std::remove_cvref_t<decltype(*a)>;      \
    fn<T>(n, a, b, o);                                \
  }

#define DEFINE_REF_AND_TEST_FN(name, op)                         \
  template<class T>                                              \
  static void name(size_t n, const T* a, const T* b, T* o)       \
  {                                                              \
    stdcolt::simd::name(n, a, b, o);                             \
  }                                                              \
  template<class T>                                              \
  static void ref_##name(size_t n, const T* a, const T* b, T* o) \
  {                                                              \
    for (size_t i = 0; i < n; ++i)                               \
      o[i] = static_cast<T>(a[i] op b[i]);                       \
  }

template<class T, auto Func, auto Ref>
static void run_binop_case(
    size_t n, int off_a, int off_b, int off_o, std::mt19937& rng)
{
  const size_t pad = 64 / sizeof(T) + 16;
  std::vector<T> A(n + pad), B(n + pad), O(n + pad), R(n + pad);

  // random input
  if constexpr (std::is_floating_point_v<T>)
  {
    std::uniform_real_distribution<double> dist(-1000.0, 1000.0);
    for (auto& x : A)
      x = static_cast<T>(dist(rng));
    for (auto& x : B)
      x = static_cast<T>(dist(rng));
  }
  else if constexpr (std::is_signed_v<T>)
  {
    std::uniform_int_distribution<int64_t> dist(
        std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    for (auto& x : A)
      x = static_cast<T>(dist(rng));
    for (auto& x : B)
      x = static_cast<T>(dist(rng));
  }
  else
  {
    std::uniform_int_distribution<uint64_t> dist(
        std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    for (auto& x : A)
      x = static_cast<T>(dist(rng));
    for (auto& x : B)
      x = static_cast<T>(dist(rng));
  }

  T* o       = O.data() + off_o;
  const T* a = A.data() + off_a;
  const T* b = B.data() + off_b;
  T* r       = R.data() + off_o;

  Ref(n, a, b, r);  // reference
  Func(n, a, b, o); // SIMD function

  if constexpr (std::is_floating_point_v<T>)
  {
    const T eps = static_cast<T>(1e-6);
    for (size_t i = 0; i < n; ++i)
      CHECK(std::fabs(o[i] - r[i]) <= eps * (std::fabs(r[i]) + 1));
  }
  else
  {
    REQUIRE(std::memcmp(r, o, n * sizeof(T)) == 0);
  }
}

template<class T, auto Func, auto Ref>
static void suite_for_type()
{
  std::mt19937 rng(12345);
  const size_t sizes[] = {0,  1,  2,  3,   7,   15,  16,  17,   31,   32,  47,
                          63, 64, 65, 127, 128, 511, 512, 1000, 4000, 8000};
  const int offs[]     = {0, 1, 2, 4, 8};

  for (size_t n : sizes)
    for (int oa : offs)
      for (int ob : offs)
        for (int oo : offs)
          run_binop_case<T, Func, Ref>(n, oa, ob, oo, rng);
}

template<auto Func, auto Ref, class... Ts>
void run_all_suites()
{
  (suite_for_type<Ts, Func, Ref>(), ...);
}

DEFINE_REF_AND_TEST_FN(add, +)
DEFINE_REF_AND_TEST_FN(sub, -)
DEFINE_REF_AND_TEST_FN(mul, *)
DEFINE_REF_AND_TEST_FN(div, /)

TEST_CASE("stdcolt/simd")
{
  SUBCASE("add")
  {
    run_all_suites<
        OVERLOAD_AS_LAMBDA(add), OVERLOAD_AS_LAMBDA(ref_add), ALL_INT_T, ALL_FP_T>();
  }
  SUBCASE("sub")
  {
    run_all_suites<
        OVERLOAD_AS_LAMBDA(sub), OVERLOAD_AS_LAMBDA(ref_sub), ALL_INT_T, ALL_FP_T>();
  }
  SUBCASE("mul")
  {
    run_all_suites<OVERLOAD_AS_LAMBDA(mul), OVERLOAD_AS_LAMBDA(ref_mul), ALL_FP_T>();
  }
  SUBCASE("div")
  {
    run_all_suites<OVERLOAD_AS_LAMBDA(div), OVERLOAD_AS_LAMBDA(ref_div), ALL_FP_T>();
  }
}

#undef DEFINE_REF_AND_TEST_FN
#undef OVERLOAD_AS_LAMBDA
#undef ALL_INT_T
#undef ALL_FP_T