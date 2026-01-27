#include <doctest/doctest.h>
#include <stdcolt_simd/simd.h>
#include <vector>
#include <random>
#include <cstring>
#include <cmath>
#include <limits>
#include <type_traits>

#define ALL_INT_T \
  uint8_t, uint16_t, uint32_t, uint64_t, int8_t, int16_t, int32_t, int64_t
#define ALL_SINT_T int8_t, int16_t, int32_t, int64_t
#define ALL_FP_T   float, double

#define OVERLOAD_AS_LAMBDA(fn)                        \
  [](size_t n, const auto* a, const auto* b, auto* o) \
  {                                                   \
    using T = std::remove_cvref_t<decltype(*a)>;      \
    fn<T>(n, a, b, o);                                \
  }

#define OVERLOAD_UNARY_AS_LAMBDA(fn)             \
  [](size_t n, const auto* a, auto* o)           \
  {                                              \
    using T = std::remove_cvref_t<decltype(*a)>; \
    fn<T>(n, a, o);                              \
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

#define DEFINE_REF_AND_TEST_UNARY_FN(name, expr)     \
  template<class T>                                  \
  static void name(size_t n, const T* a, T* o)       \
  {                                                  \
    stdcolt::simd::name(n, a, o);                    \
  }                                                  \
  template<class T>                                  \
  static void ref_##name(size_t n, const T* a, T* o) \
  {                                                  \
    for (size_t i = 0; i < n; ++i)                   \
    {                                                \
      const T x = a[i];                              \
      o[i]      = static_cast<T>(expr);              \
    }                                                \
  }

// ---- UB-safe scalar refs for signed int neg/abs ----

template<class T>
static inline T ref_wrap_neg(T x) noexcept
{
  static_assert(std::is_integral_v<T>);
  using U = std::make_unsigned_t<T>;
  return static_cast<T>(U(0) - static_cast<U>(x));
}

template<class T>
static inline T ref_abs_sint(T x) noexcept
{
  static_assert(std::is_integral_v<T> && std::is_signed_v<T>);
  if (x >= 0)
    return x;
  if (x == std::numeric_limits<T>::min())
    return x; // preserve min
  return static_cast<T>(-x);
}

// ---- binary tests (unchanged) ----

template<class T, auto Func, auto Ref>
static void run_binop_case(
    size_t n, int off_a, int off_b, int off_o, std::mt19937& rng)
{
  const size_t pad = 64 / sizeof(T) + 16;
  std::vector<T> A(n + pad), B(n + pad), O(n + pad), R(n + pad);

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

  Ref(n, a, b, r);
  Func(n, a, b, o);

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

// ---- unary tests ----

template<class T, auto Func, auto Ref, class Fill>
static void run_unop_case(
    size_t n, int off_a, int off_o, std::mt19937& rng, Fill fill)
{
  const size_t pad = 64 / sizeof(T) + 16;
  std::vector<T> A(n + pad), O(n + pad), R(n + pad);

  fill(A, rng);

  T* o       = O.data() + off_o;
  const T* a = A.data() + off_a;
  T* r       = R.data() + off_o;

  Ref(n, a, r);
  Func(n, a, o);

  if constexpr (std::is_floating_point_v<T>)
  {
    const T eps = static_cast<T>(1e-6);
    for (size_t i = 0; i < n; ++i)
    {
      const T rr = r[i], oo = o[i];
      if (std::isnan(rr))
        CHECK(std::isnan(oo));
      else if (std::isinf(rr))
      {
        CHECK(std::isinf(oo));
        CHECK(std::signbit(rr) == std::signbit(oo));
      }
      else
        CHECK(std::fabs(oo - rr) <= eps * (std::fabs(rr) + 1));
    }
  }
  else
  {
    REQUIRE(std::memcmp(r, o, n * sizeof(T)) == 0);
  }
}

template<class T, auto Func, auto Ref, class Fill>
static void suite_for_unary_type(Fill fill)
{
  std::mt19937 rng(12345);
  const size_t sizes[] = {0,  1,  2,  3,   7,   15,  16,  17,   31,   32,  47,
                          63, 64, 65, 127, 128, 511, 512, 1000, 4000, 8000};
  const int offs[]     = {0, 1, 2, 4, 8};

  for (size_t n : sizes)
    for (int oa : offs)
      for (int oo : offs)
        run_unop_case<T, Func, Ref>(n, oa, oo, rng, fill);
}

template<auto Func, auto Ref, class Fill, class... Ts>
static void run_all_unary_suites(Fill fill)
{
  (suite_for_unary_type<Ts, Func, Ref>(fill), ...);
}

// Specialized unary test driver (no reference array, custom check per element)
template<class T, auto Func, class Fill, class Check>
static void run_unop_case_custom(
    size_t n, int off_a, int off_o, std::mt19937& rng, Fill fill, Check check)
{
  const size_t pad = 64 / sizeof(T) + 16;
  std::vector<T> A(n + pad), O(n + pad);

  fill(A, rng);

  T* o       = O.data() + off_o;
  const T* a = A.data() + off_a;

  Func(n, a, o);

  for (size_t i = 0; i < n; ++i)
    check(a[i], o[i]);
}

template<class T, auto Func, class Fill, class Check>
static void suite_for_unary_type_custom(Fill fill, Check check)
{
  std::mt19937 rng(12345);
  const size_t sizes[] = {0,  1,  2,  3,   7,   15,  16,  17,   31,   32,  47,
                          63, 64, 65, 127, 128, 511, 512, 1000, 4000, 8000};
  const int offs[]     = {0, 1, 2, 4, 8};

  for (size_t n : sizes)
    for (int oa : offs)
      for (int oo : offs)
        run_unop_case_custom<T, Func>(n, oa, oo, rng, fill, check);
}

// Refs for unary ops
template<class T>
static void ref_neg(size_t n, const T* a, T* o)
{
  for (size_t i = 0; i < n; ++i)
  {
    if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
      o[i] = ref_wrap_neg(a[i]);
    else
      o[i] = static_cast<T>(-a[i]);
  }
}

template<class T>
static void ref_abs(size_t n, const T* a, T* o)
{
  for (size_t i = 0; i < n; ++i)
  {
    if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
      o[i] = ref_abs_sint(a[i]);
    else if constexpr (std::is_floating_point_v<T>)
      o[i] = static_cast<T>(std::fabs(a[i]));
  }
}

DEFINE_REF_AND_TEST_UNARY_FN(sqrt, std::sqrt(x))
DEFINE_REF_AND_TEST_UNARY_FN(floor, std::floor(x))
DEFINE_REF_AND_TEST_UNARY_FN(ceil, std::ceil(x))
DEFINE_REF_AND_TEST_UNARY_FN(trunc, std::trunc(x))

// Adapters that call your SIMD functions
template<class T>
static void neg(size_t n, const T* a, T* o)
{
  stdcolt::simd::neg(n, a, o);
}
template<class T>
static void abs(size_t n, const T* a, T* o)
{
  stdcolt::simd::abs(n, a, o);
}
static void rcp(size_t n, const float* a, float* o)
{
  stdcolt::simd::rcp(n, a, o);
}
static void rsqrt(size_t n, const float* a, float* o)
{
  stdcolt::simd::rsqrt(n, a, o);
}

// Fillers
struct FillDefault
{
  template<class T>
  void operator()(std::vector<T>& A, std::mt19937& rng) const
  {
    if constexpr (std::is_floating_point_v<T>)
    {
      std::uniform_real_distribution<double> dist(-1000.0, 1000.0);
      for (auto& x : A)
        x = static_cast<T>(dist(rng));
    }
    else if constexpr (std::is_signed_v<T>)
    {
      std::uniform_int_distribution<int64_t> dist(
          std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
      for (auto& x : A)
        x = static_cast<T>(dist(rng));
    }
    else
    {
      std::uniform_int_distribution<uint64_t> dist(
          std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
      for (auto& x : A)
        x = static_cast<T>(dist(rng));
    }
  }
};

struct FillPositiveNonZero
{
  template<class T>
  void operator()(std::vector<T>& A, std::mt19937& rng) const
  {
    static_assert(std::is_floating_point_v<T>);
    std::uniform_real_distribution<double> dist(1e-3, 1000.0);
    for (auto& x : A)
      x = static_cast<T>(dist(rng));
  }
};

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

  SUBCASE("neg")
  {
    run_all_unary_suites<
        OVERLOAD_UNARY_AS_LAMBDA(neg), OVERLOAD_UNARY_AS_LAMBDA(ref_neg),
        FillDefault, ALL_SINT_T, ALL_FP_T>(FillDefault{});
  }

  SUBCASE("abs")
  {
    run_all_unary_suites<
        OVERLOAD_UNARY_AS_LAMBDA(abs), OVERLOAD_UNARY_AS_LAMBDA(ref_abs),
        FillDefault, ALL_SINT_T, ALL_FP_T>(FillDefault{});
  }

  SUBCASE("sqrt")
  {
    run_all_unary_suites<
        OVERLOAD_UNARY_AS_LAMBDA(sqrt), OVERLOAD_UNARY_AS_LAMBDA(ref_sqrt),
        FillPositiveNonZero, ALL_FP_T>(FillPositiveNonZero{});
  }

  SUBCASE("floor")
  {
    run_all_unary_suites<
        OVERLOAD_UNARY_AS_LAMBDA(floor), OVERLOAD_UNARY_AS_LAMBDA(ref_floor),
        FillDefault, ALL_FP_T>(FillDefault{});
  }

  SUBCASE("ceil")
  {
    run_all_unary_suites<
        OVERLOAD_UNARY_AS_LAMBDA(ceil), OVERLOAD_UNARY_AS_LAMBDA(ref_ceil),
        FillDefault, ALL_FP_T>(FillDefault{});
  }

  SUBCASE("trunc")
  {
    run_all_unary_suites<
        OVERLOAD_UNARY_AS_LAMBDA(trunc), OVERLOAD_UNARY_AS_LAMBDA(ref_trunc),
        FillDefault, ALL_FP_T>(FillDefault{});
  }

  // rcp_e: check x * rcp(x) ~= 1
  SUBCASE("rcp(float) fast")
  {
    constexpr float tol = 1.5e-3f; // raw estimate; adjust if needed

    suite_for_unary_type_custom<float, rcp>(
        FillPositiveNonZero{},
        [](float x, float y)
        {
          const float id = x * y;
          CHECK(std::fabs(id - 1.0f) <= tol);
        });
  }

  // rsqrt_e: check x * rsqrt(x)^2 ~= 1
  SUBCASE("rsqrt(float) fast")
  {
    constexpr float tol = 2.0e-3f; // raw estimate; adjust if needed

    suite_for_unary_type_custom<float, rsqrt>(
        FillPositiveNonZero{},
        [](float x, float y)
        {
          const float id = x * y * y;
          CHECK(std::fabs(id - 1.0f) <= tol);
        });
  }
}

#undef DEFINE_REF_AND_TEST_UNARY_FN
#undef DEFINE_REF_AND_TEST_FN
#undef OVERLOAD_UNARY_AS_LAMBDA
#undef OVERLOAD_AS_LAMBDA
#undef ALL_SINT_T
#undef ALL_INT_T
#undef ALL_FP_T
