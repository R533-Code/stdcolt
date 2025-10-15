#include "simd.h"

#if defined(__clang__) || defined(__GNUC__)
  #define CPU_TARGET(attrs) __attribute__((target(attrs)))
#else
  #define CPU_TARGET(attrs)
#endif

#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
  #include <immintrin.h>
#endif

#define _STDCOLT_SIMD_BINARY_DEFINE_OVERLOAD(name, type)              \
  void name(size_t n, const type* a, const type* b, type* o) noexcept \
  {                                                                   \
    return OptimalOverloads::get().name._##type(n, a, b, o);          \
  }

#define _STDCOLT_SIMD_BINARY_DEFINE_OVERLOAD_SET_INT(name) \
  _STDCOLT_SIMD_BINARY_DEFINE_OVERLOAD(name, uint8_t)      \
  _STDCOLT_SIMD_BINARY_DEFINE_OVERLOAD(name, uint16_t)     \
  _STDCOLT_SIMD_BINARY_DEFINE_OVERLOAD(name, uint32_t)     \
  _STDCOLT_SIMD_BINARY_DEFINE_OVERLOAD(name, uint64_t)     \
  _STDCOLT_SIMD_BINARY_DEFINE_OVERLOAD(name, int8_t)       \
  _STDCOLT_SIMD_BINARY_DEFINE_OVERLOAD(name, int16_t)      \
  _STDCOLT_SIMD_BINARY_DEFINE_OVERLOAD(name, int32_t)      \
  _STDCOLT_SIMD_BINARY_DEFINE_OVERLOAD(name, int64_t)

#define _STDCOLT_SIMD_BINARY_DEFINE_KERNEL(                                     \
    NAME, ISA_TAG, ATTRS, VECTYPE, UNROLL, BIN_FN, LD_FN, ST_FN)                \
  template<typename T>                                                          \
  CPU_TARGET(ATTRS)                                                             \
  static void NAME##_##ISA_TAG(size_t n, const T* a, const T* b, T* o) noexcept \
  {                                                                             \
    simd_binary_operator<                                                       \
        T, VECTYPE, UNROLL, BIN_FN<T>, LD_FN<T>, ST_FN<T>, default_add<T>>(     \
        n, a, b, o);                                                            \
  }

#define _STDCOLT_OPTIMAL_ASSIGN_ONE(VAR, FUNC, TYPE) (VAR._##TYPE = &FUNC<TYPE>)
#define _STDCOLT_OPTIMAL_ASSIGN_INT_SET(VAR, FUNC)    \
  do                                                  \
  {                                                   \
    _STDCOLT_OPTIMAL_ASSIGN_ONE(VAR, FUNC, uint8_t);  \
    _STDCOLT_OPTIMAL_ASSIGN_ONE(VAR, FUNC, uint16_t); \
    _STDCOLT_OPTIMAL_ASSIGN_ONE(VAR, FUNC, uint32_t); \
    _STDCOLT_OPTIMAL_ASSIGN_ONE(VAR, FUNC, uint64_t); \
    _STDCOLT_OPTIMAL_ASSIGN_ONE(VAR, FUNC, int8_t);   \
    _STDCOLT_OPTIMAL_ASSIGN_ONE(VAR, FUNC, int16_t);  \
    _STDCOLT_OPTIMAL_ASSIGN_ONE(VAR, FUNC, int32_t);  \
    _STDCOLT_OPTIMAL_ASSIGN_ONE(VAR, FUNC, int64_t);  \
  } while (0)

#if defined(_MSC_VER)
  #define _STDCOLT_DO_PRAGMA(x) __pragma(x)
#else
  #define _STDCOLT_DO_PRAGMA(x) _Pragma(#x)
#endif

#if defined(__GNUC__) && !defined(__clang__)
  #define _STDCOLT_NOVEC_LOOP_BEGIN      \
    _STDCOLT_DO_PRAGMA(GCC push_options) \
    _STDCOLT_DO_PRAGMA(GCC optimize("no-tree-vectorize"))
  #define _STDCOLT_NOVEC_LOOP_END _STDCOLT_DO_PRAGMA(GCC pop_options)
#elif defined(__clang__)
  #define _STDCOLT_NOVEC_LOOP_BEGIN _STDCOLT_DO_PRAGMA(clang loop vectorize(disable))
  #define _STDCOLT_NOVEC_LOOP_END
#elif defined(_MSC_VER)
  #define _STDCOLT_NOVEC_LOOP_BEGIN _STDCOLT_DO_PRAGMA(loop(no_vector))
  #define _STDCOLT_NOVEC_LOOP_END
#else
  #define _STDCOLT_NOVEC_LOOP_BEGIN
  #define _STDCOLT_NOVEC_LOOP_END
#endif

namespace stdcolt::simd
{
  template<
      typename T, typename SIMD_T, size_t PREFERRED_UNROLL,
      SIMD_T (*BINARY_FN)(SIMD_T, SIMD_T), SIMD_T (*LOAD_FN)(const T*),
      void (*STORE_FN)(T*, SIMD_T),
      void (*SCALAR_FN)(size_t, const T*, const T*, T*)>
  static inline void simd_binary_operator(
      size_t n, const T* a, const T* b, T* o) noexcept
  {
    constexpr size_t VEC_BYTES = sizeof(SIMD_T);
    constexpr size_t VEC_ELEMS = VEC_BYTES / sizeof(T);
    static_assert(VEC_BYTES % sizeof(T) == 0, "SIMD_T must be multiple of T");
    static_assert(PREFERRED_UNROLL >= 1, "unroll >= 1");

    // Prologue: align destination to vector size (in bytes).
    const auto o_addr       = reinterpret_cast<std::uintptr_t>(o);
    const size_t head_bytes = (~o_addr + 1) & (VEC_BYTES - 1);
    size_t head_elems       = head_bytes / sizeof(T);
    if (head_elems > n)
      head_elems = n;
    if (head_elems)
    {
      SCALAR_FN(head_elems, a, b, o);
      a += head_elems;
      b += head_elems;
      o += head_elems;
      n -= head_elems;
    }

    // Core: unrolled vector loop with aligned stores, unaligned loads.
    const size_t BLOCK_ELEMS = VEC_ELEMS * PREFERRED_UNROLL;
    size_t i                 = 0;
    _STDCOLT_NOVEC_LOOP_BEGIN
    for (; i + BLOCK_ELEMS <= n; i += BLOCK_ELEMS)
    {
      SIMD_T av[PREFERRED_UNROLL];
      SIMD_T bv[PREFERRED_UNROLL];

      _STDCOLT_NOVEC_LOOP_BEGIN
      for (size_t j = 0; j < PREFERRED_UNROLL; ++j)
      {
        av[j] = LOAD_FN(a + i + j * VEC_ELEMS);
        bv[j] = LOAD_FN(b + i + j * VEC_ELEMS);
      }
      _STDCOLT_NOVEC_LOOP_END

      _STDCOLT_NOVEC_LOOP_BEGIN
      for (size_t j = 0; j < PREFERRED_UNROLL; ++j)
        STORE_FN(o + i + j * VEC_ELEMS, BINARY_FN(av[j], bv[j]));
      _STDCOLT_NOVEC_LOOP_END
    }
    _STDCOLT_NOVEC_LOOP_END

    // Remainder: single-vector steps, still aligned stores.
    _STDCOLT_NOVEC_LOOP_END
    for (; i + VEC_ELEMS <= n; i += VEC_ELEMS)
    {
      SIMD_T av = LOAD_FN(a + i);
      SIMD_T bv = LOAD_FN(b + i);
      STORE_FN(o + i, BINARY_FN(av, bv));
    }
    _STDCOLT_NOVEC_LOOP_END

    // Epilogue: scalar tail.
    if (i < n)
      SCALAR_FN(n - i, a + i, b + i, o + i);
  }

  struct OptimalOverloads
  {
    struct BinaryInt
    {
      void (*_uint8_t)(size_t, const uint8_t*, const uint8_t*, uint8_t*) noexcept;
      void (*_uint16_t)(
          size_t, const uint16_t*, const uint16_t*, uint16_t*) noexcept;
      void (*_uint32_t)(
          size_t, const uint32_t*, const uint32_t*, uint32_t*) noexcept;
      void (*_uint64_t)(
          size_t, const uint64_t*, const uint64_t*, uint64_t*) noexcept;
      void (*_int8_t)(size_t, const int8_t*, const int8_t*, int8_t*) noexcept;
      void (*_int16_t)(size_t, const int16_t*, const int16_t*, int16_t*) noexcept;
      void (*_int32_t)(size_t, const int32_t*, const int32_t*, int32_t*) noexcept;
      void (*_int64_t)(size_t, const int64_t*, const int64_t*, int64_t*) noexcept;
    };

    BinaryInt add;

  private:
    static OptimalOverloads OPTIMAL_OVERLOADS;

  public:
    static void build_optimal_overloads() noexcept;

    static const OptimalOverloads& get() noexcept
    {
      static volatile int CALL_ONCE = []()
      {
        build_optimal_overloads();
        return 0;
      }();
      (void)CALL_ONCE;
      return OPTIMAL_OVERLOADS;
    }
  };

  // clang-format off
  template<typename T>
  static inline void default_add(
      size_t size, const T* a, const T* b, T* out) noexcept
  {
    _STDCOLT_NOVEC_LOOP_BEGIN
    for (size_t i = 0; i < size; i++)
      out[i] = static_cast<T>(a[i] + b[i]);
    _STDCOLT_NOVEC_LOOP_END
  }
  template<typename T>
  static inline __m128i add128(__m128i x, __m128i y)
  {
    if constexpr (sizeof(T) == 1)
      return _mm_add_epi8(x, y);
    if constexpr (sizeof(T) == 2)
      return _mm_add_epi16(x, y);
    if constexpr (sizeof(T) == 4)
      return _mm_add_epi32(x, y);
    else
      return _mm_add_epi64(x, y);
  }
  template<typename T>
  static inline __m256i add256(__m256i x, __m256i y)
  {
    if constexpr (sizeof(T) == 1)
      return _mm256_add_epi8(x, y);
    if constexpr (sizeof(T) == 2)
      return _mm256_add_epi16(x, y);
    if constexpr (sizeof(T) == 4)
      return _mm256_add_epi32(x, y);
    else
      return _mm256_add_epi64(x, y);
  }
  template<typename T>
  static inline __m512i add512(__m512i x, __m512i y)
  {
    if constexpr (sizeof(T) == 1)
      return _mm512_add_epi8(x, y);
    if constexpr (sizeof(T) == 2)
      return _mm512_add_epi16(x, y);
    if constexpr (sizeof(T) == 4)
      return _mm512_add_epi32(x, y);
    else
      return _mm512_add_epi64(x, y);
  }
  template<typename T>
  static inline __m128i load128(const T* p) noexcept { return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p)); }
  template<typename T>
  static inline __m256i load256(const T* p) noexcept { return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p)); }
  template<typename T>
  static inline __m512i load512(const T* p) noexcept { return _mm512_loadu_si512(reinterpret_cast<const void*>(p)); }
  template<typename T>
  static inline void store128(T* p, __m128i v) noexcept { _mm_store_si128(reinterpret_cast<__m128i*>(p), v); }
  template<typename T>
  static inline void store256(T* p, __m256i v) noexcept { _mm256_store_si256(reinterpret_cast<__m256i*>(p), v); }
  template<typename T>
  static inline void store512(T* p, __m512i v) noexcept { _mm512_store_si512(reinterpret_cast<void*>(p), v); }
  _STDCOLT_SIMD_BINARY_DEFINE_KERNEL(add, sse2, "sse2", __m128i, 4, add128, load128, store128)
  _STDCOLT_SIMD_BINARY_DEFINE_KERNEL(add, avx2, "avx2", __m256i, 4, add256, load256, store256)
  _STDCOLT_SIMD_BINARY_DEFINE_KERNEL(add, avx512, "avx512bw,avx512vl", __m512i, 8, add512, load512, store512)
  _STDCOLT_SIMD_BINARY_DEFINE_OVERLOAD_SET_INT(add)
  // clang-format on

  void OptimalOverloads::build_optimal_overloads() noexcept
  {
    using enum Feature;
    auto& ref    = OPTIMAL_OVERLOADS;
    const auto f = detect_features();

    if (has(f, X86_AVX512BW) && has(f, X86_AVX512VL))
      _STDCOLT_OPTIMAL_ASSIGN_INT_SET(ref.add, add_avx512);
    else if (has(f, X86_AVX2))
      _STDCOLT_OPTIMAL_ASSIGN_INT_SET(ref.add, add_avx2);
    else if (has(f, X86_SSE2))
      _STDCOLT_OPTIMAL_ASSIGN_INT_SET(ref.add, add_sse2);
    else
      _STDCOLT_OPTIMAL_ASSIGN_INT_SET(ref.add, default_add);
  }

  void rebuild_optimal_overloads() noexcept
  {
    OptimalOverloads::build_optimal_overloads();
  }

  // global optimal overload set
  OptimalOverloads OptimalOverloads::OPTIMAL_OVERLOADS = {};
} // namespace stdcolt::simd

#undef _STDCOLT_OPTIMAL_ASSIGN_ONE
#undef _STDCOLT_OPTIMAL_ASSIGN_INT_SET
#undef _STDCOLT_SIMD_BINARY_DEFINE_KERNEL
#undef _STDCOLT_SIMD_BINARY_DEFINE_OVERLOAD
#undef _STDCOLT_SIMD_BINARY_DEFINE_OVERLOAD_SET_INT