#include <doctest/doctest.h>
#include <stdcolt_simd/simd.h>
#include <cstdint>
#include <vector>
#include <random>
#include <cstring>
#include <chrono>

template<class T>
static void ref_add(std::size_t n, const T* a, const T* b, T* o)
{
  for (std::size_t i = 0; i < n; ++i)
    o[i] = static_cast<T>(a[i] + b[i]);
}

template<class T>
static void run_add_case(
    std::size_t n, int off_a, int off_b, int off_o, std::mt19937& rng)
{
  const std::size_t pad = 64 / sizeof(T) + 16;
  std::vector<T> A(n + pad), B(n + pad), O(n + pad), R(n + pad);

  // generate random data
  if constexpr (std::is_signed_v<T>)
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

  ref_add(n, a, b, r);
  stdcolt::simd::add(n, a, b, o);

  REQUIRE(std::memcmp(r, o, n * sizeof(T)) == 0);
}

template<class T>
static void suite_for_type()
{
  std::mt19937 rng(12345);

  const std::size_t sizes[] = {0,  1,  2,  3,   7,   15,  16,  17,   31,   32,  47,
                               63, 64, 65, 127, 128, 511, 512, 1000, 4000, 8000};

  const int offs[] = {0, 1, 2, 4, 8};
  for (std::size_t n : sizes)
  {
    // try different combinations:
    for (int oa : offs)
    {
      for (int ob : offs)
      {
        for (int oo : offs)
        {
          run_add_case<T>(n, oa, ob, oo, rng);
        }
      }
    }
  }
}

template<class... Ts>
void run_all_suites()
{
  // no SUBCASE here; just call them
  (suite_for_type<Ts>(), ...);
}

TEST_CASE("stdcolt/simd/add")
{
  using namespace stdcolt;

  SUBCASE("scalar")
  {
    // disable all CPU features
    simd::override_disabled_features(~(simd::FeatureMask)0);
    simd::rebuild_optimal_overloads();
    auto start = std::chrono::high_resolution_clock::now();

    run_all_suites<
        std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t, std::int8_t,
        std::int16_t, std::int32_t, std::int64_t>();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    printf("  Scalar Implementation: %f ms\n", elapsed.count());
  }

  SUBCASE("simd")
  {
    // enable all CPU features
    simd::override_disabled_features(0);
    simd::rebuild_optimal_overloads();
    auto start = std::chrono::high_resolution_clock::now();

    run_all_suites<
        std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t, std::int8_t,
        std::int16_t, std::int32_t, std::int64_t>();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    printf("  SIMD Implementation: %f ms\n", elapsed.count());
  }
}
