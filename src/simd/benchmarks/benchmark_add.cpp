#include <nanobench.h>
#include <stdcolt_simd/simd.h>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <vector>

using stdcolt::simd::add;

namespace
{

  template<class T>
  struct AlignedFree
  {
    void operator()(T* p) const noexcept
    {
#if defined(_MSC_VER)
      _aligned_free(p);
#else
      std::free(p);
#endif
    }
  };

  template<class T>
  using AlignedPtr = std::unique_ptr<T, AlignedFree<T>>;

  template<class T>
  AlignedPtr<T> alloc_aligned(std::size_t n, std::size_t align)
  {
    void* p = nullptr;
#if defined(_MSC_VER)
    p = _aligned_malloc(n * sizeof(T), align);
    if (!p)
      throw std::bad_alloc{};
#else
    if (posix_memalign(&p, align, n * sizeof(T)) != 0)
      throw std::bad_alloc{};
#endif
    return AlignedPtr<T>(reinterpret_cast<T*>(p));
  }

  template<class T>
  void fill(std::span<T> s, uint64_t seed)
  {
    ankerl::nanobench::Rng rng(seed);
    for (auto& x : s)
      x = static_cast<T>(rng());
  }

  template<class T>
  void bench_type(ankerl::nanobench::Bench& bench, std::string_view tag)
  {
    // Sizes cover small, L1/L2, and streaming.
    const std::size_t Ns[] = {256, 4096, 65536, 1'000'000};

    // Align to 64 for AVX-512 stores; pad +8 elements to allow offsets.
    for (std::size_t N : Ns)
    {
      const std::size_t pad = 8;
      auto A                = alloc_aligned<T>(N + pad, 64);
      auto B                = alloc_aligned<T>(N + pad, 64);
      auto O                = alloc_aligned<T>(N + pad, 64);

      fill<T>({A.get(), N + pad}, 1);
      fill<T>({B.get(), N + pad}, 2);
      std::memset(O.get(), 0, (N + pad) * sizeof(T));

      struct Case
      {
        int offA, offB, offO;
        const char* name;
      };
      const Case cases[] = {
          {0, 0, 0, "aligned"}, {1, 0, 0, "a+1"},   {0, 1, 0, "b+1"},
          {0, 0, 1, "o+1"},     {2, 3, 1, "mixed"},
      };

      for (auto c : cases)
      {
        T* o       = O.get() + c.offO;
        const T* a = A.get() + c.offA;
        const T* b = B.get() + c.offB;

        bench.complexityN(N).run(
            std::format("{} {} N={}", tag, c.name, N),
            [&]
            {
              add(N, a, b, o);
              ankerl::nanobench::doNotOptimizeAway(o);
            });
      }
    }
  }

} // namespace

void benchmark_add()
{
  ankerl::nanobench::Bench bench;
  bench.title("stdcolt::simd::add")
      .performanceCounters(false)
      .minEpochIterations(2000)
      .epochs(30);

  bench.warmup(
      ankerl::nanobench::Clock::now().time_since_epoch().count() & 1 ? 1 : 2);

  bench_type<std::uint8_t>(bench, "u8");
  bench_type<std::uint16_t>(bench, "u16");
  bench_type<std::uint32_t>(bench, "u32");
  bench_type<std::uint64_t>(bench, "u64");
}
