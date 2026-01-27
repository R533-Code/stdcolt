#include <stdcolt_simd/simd.h>
#include <cstdint>
#include <algorithm>
#include <cmath>

#include <simdpp/simd.h>

#if defined(_MSC_VER)
  #include <simdpp/dispatch/get_arch_raw_cpuid.h>
  #define SIMDPP_USER_ARCH_INFO ::simdpp::get_arch_raw_cpuid()
#else
  #include <simdpp/dispatch/get_arch_gcc_builtin_cpu_supports.h>
  #define SIMDPP_USER_ARCH_INFO ::simdpp::get_arch_gcc_builtin_cpu_supports()
#endif

#define STDCOLT_DEFINE_SIMD_BINARY_OP(                               \
    FUNCNAME, TYPE, VSIZE, VTYPE, SIMD_OP, CPP_OP)                   \
  namespace SIMDPP_ARCH_NAMESPACE                                    \
  {                                                                  \
    void FUNCNAME(size_t n, const TYPE* a, const TYPE* b, TYPE* out) \
    {                                                                \
      using V            = VTYPE;                                    \
      constexpr size_t K = VSIZE;                                    \
      constexpr size_t U = UNROLL_FACTOR;                            \
      size_t i           = 0;                                        \
      for (; i + U * K <= n; i += U * K)                             \
      {                                                              \
        V va[U], vb[U], vc[U];                                       \
        for (size_t u = 0; u < U; ++u)                               \
        {                                                            \
          va[u] = simdpp::load_u<V>(a + i + u * K);                  \
          vb[u] = simdpp::load_u<V>(b + i + u * K);                  \
          vc[u] = simdpp::SIMD_OP(va[u], vb[u]);                     \
          simdpp::store_u(out + i + u * K, vc[u]);                   \
        }                                                            \
      }                                                              \
      for (; i + K <= n; i += K)                                     \
      {                                                              \
        V va = simdpp::load_u<V>(a + i);                             \
        V vb = simdpp::load_u<V>(b + i);                             \
        V vc = simdpp::SIMD_OP(va, vb);                              \
        simdpp::store_u(out + i, vc);                                \
      }                                                              \
      for (; i < n; ++i)                                             \
        out[i] = static_cast<TYPE>(a[i] CPP_OP b[i]);                \
    }                                                                \
  }                                                                  \
  SIMDPP_MAKE_DISPATCHER_VOID4(FUNCNAME, size_t, const TYPE*, const TYPE*, TYPE*);

#define STDCOLT_DEFINE_SIMD_UNARY_OP(                        \
    FUNCNAME, TYPE, VSIZE, VTYPE, SIMD_OP, TAIL_SCALAR_EXPR) \
  namespace SIMDPP_ARCH_NAMESPACE                            \
  {                                                          \
    void FUNCNAME(size_t n, const TYPE* a, TYPE* out)        \
    {                                                        \
      using V            = VTYPE;                            \
      constexpr size_t K = VSIZE;                            \
      constexpr size_t U = UNROLL_FACTOR;                    \
      size_t i           = 0;                                \
      for (; i + U * K <= n; i += U * K)                     \
      {                                                      \
        V va[U], vc[U];                                      \
        for (size_t u = 0; u < U; ++u)                       \
        {                                                    \
          va[u] = simdpp::load_u<V>(a + i + u * K);          \
          vc[u] = simdpp::SIMD_OP(va[u]);                    \
          simdpp::store_u(out + i + u * K, vc[u]);           \
        }                                                    \
      }                                                      \
      for (; i + K <= n; i += K)                             \
      {                                                      \
        V va = simdpp::load_u<V>(a + i);                     \
        V vc = simdpp::SIMD_OP(va);                          \
        simdpp::store_u(out + i, vc);                        \
      }                                                      \
      for (; i < n; ++i)                                     \
        out[i] = (TAIL_SCALAR_EXPR);                         \
    }                                                        \
  }                                                          \
  SIMDPP_MAKE_DISPATCHER_VOID3(FUNCNAME, size_t, const TYPE*, TYPE*);

// clang-format off
#define STDCOLT_DEFINE_SIMD_BINARY_OP_FOR_INT(FUNCNAME, SIMD_OP, CPP_OP)              \
  STDCOLT_DEFINE_SIMD_BINARY_OP(FUNCNAME, uint8_t,  SIZE_I8,  simdpp::uint8<SIZE_I8>,   SIMD_OP, CPP_OP) \
  STDCOLT_DEFINE_SIMD_BINARY_OP(FUNCNAME, uint16_t, SIZE_I16, simdpp::uint16<SIZE_I16>, SIMD_OP, CPP_OP) \
  STDCOLT_DEFINE_SIMD_BINARY_OP(FUNCNAME, uint32_t, SIZE_I32, simdpp::uint32<SIZE_I32>, SIMD_OP, CPP_OP) \
  STDCOLT_DEFINE_SIMD_BINARY_OP(FUNCNAME, uint64_t, SIZE_I64, simdpp::uint64<SIZE_I64>, SIMD_OP, CPP_OP) \
  STDCOLT_DEFINE_SIMD_BINARY_OP(FUNCNAME, int8_t,   SIZE_I8,  simdpp::int8<SIZE_I8>,    SIMD_OP, CPP_OP) \
  STDCOLT_DEFINE_SIMD_BINARY_OP(FUNCNAME, int16_t,  SIZE_I16, simdpp::int16<SIZE_I16>,  SIMD_OP, CPP_OP) \
  STDCOLT_DEFINE_SIMD_BINARY_OP(FUNCNAME, int32_t,  SIZE_I32, simdpp::int32<SIZE_I32>,  SIMD_OP, CPP_OP) \
  STDCOLT_DEFINE_SIMD_BINARY_OP(FUNCNAME, int64_t,  SIZE_I64, simdpp::int64<SIZE_I64>,  SIMD_OP, CPP_OP)

#define STDCOLT_DEFINE_SIMD_BINARY_OP_FOR_SINT(FUNCNAME, SIMD_OP, CPP_OP)              \
  STDCOLT_DEFINE_SIMD_BINARY_OP(FUNCNAME, int8_t,   SIZE_I8,  simdpp::int8<SIZE_I8>,    SIMD_OP, CPP_OP) \
  STDCOLT_DEFINE_SIMD_BINARY_OP(FUNCNAME, int16_t,  SIZE_I16, simdpp::int16<SIZE_I16>,  SIMD_OP, CPP_OP) \
  STDCOLT_DEFINE_SIMD_BINARY_OP(FUNCNAME, int32_t,  SIZE_I32, simdpp::int32<SIZE_I32>,  SIMD_OP, CPP_OP) \
  STDCOLT_DEFINE_SIMD_BINARY_OP(FUNCNAME, int64_t,  SIZE_I64, simdpp::int64<SIZE_I64>,  SIMD_OP, CPP_OP)

#define STDCOLT_DEFINE_SIMD_BINARY_OP_FOR_FP(FUNCNAME, SIMD_OP, CPP_OP)              \
  STDCOLT_DEFINE_SIMD_BINARY_OP(FUNCNAME, float,  SIZE_F32, simdpp::float32<SIZE_F32>, SIMD_OP, CPP_OP) \
  STDCOLT_DEFINE_SIMD_BINARY_OP(FUNCNAME, double, SIZE_F64, simdpp::float64<SIZE_F64>, SIMD_OP, CPP_OP)

#define STDCOLT_DEFINE_SIMD_UNARY_OP_FOR_INT(FUNCNAME, SIMD_OP, TAIL_EXPR)           \
  STDCOLT_DEFINE_SIMD_UNARY_OP(FUNCNAME, uint8_t,  SIZE_I8,  simdpp::uint8<SIZE_I8>,   SIMD_OP, TAIL_EXPR) \
  STDCOLT_DEFINE_SIMD_UNARY_OP(FUNCNAME, uint16_t, SIZE_I16, simdpp::uint16<SIZE_I16>, SIMD_OP, TAIL_EXPR) \
  STDCOLT_DEFINE_SIMD_UNARY_OP(FUNCNAME, uint32_t, SIZE_I32, simdpp::uint32<SIZE_I32>, SIMD_OP, TAIL_EXPR) \
  STDCOLT_DEFINE_SIMD_UNARY_OP(FUNCNAME, uint64_t, SIZE_I64, simdpp::uint64<SIZE_I64>, SIMD_OP, TAIL_EXPR) \
  STDCOLT_DEFINE_SIMD_UNARY_OP(FUNCNAME, int8_t,   SIZE_I8,  simdpp::int8<SIZE_I8>,    SIMD_OP, TAIL_EXPR) \
  STDCOLT_DEFINE_SIMD_UNARY_OP(FUNCNAME, int16_t,  SIZE_I16, simdpp::int16<SIZE_I16>,  SIMD_OP, TAIL_EXPR) \
  STDCOLT_DEFINE_SIMD_UNARY_OP(FUNCNAME, int32_t,  SIZE_I32, simdpp::int32<SIZE_I32>,  SIMD_OP, TAIL_EXPR) \
  STDCOLT_DEFINE_SIMD_UNARY_OP(FUNCNAME, int64_t,  SIZE_I64, simdpp::int64<SIZE_I64>,  SIMD_OP, TAIL_EXPR)

#define STDCOLT_DEFINE_SIMD_UNARY_OP_FOR_SINT(FUNCNAME, SIMD_OP, TAIL_EXPR)           \
  STDCOLT_DEFINE_SIMD_UNARY_OP(FUNCNAME, int8_t,   SIZE_I8,  simdpp::int8<SIZE_I8>,    SIMD_OP, TAIL_EXPR) \
  STDCOLT_DEFINE_SIMD_UNARY_OP(FUNCNAME, int16_t,  SIZE_I16, simdpp::int16<SIZE_I16>,  SIMD_OP, TAIL_EXPR) \
  STDCOLT_DEFINE_SIMD_UNARY_OP(FUNCNAME, int32_t,  SIZE_I32, simdpp::int32<SIZE_I32>,  SIMD_OP, TAIL_EXPR) \
  STDCOLT_DEFINE_SIMD_UNARY_OP(FUNCNAME, int64_t,  SIZE_I64, simdpp::int64<SIZE_I64>,  SIMD_OP, TAIL_EXPR)

#define STDCOLT_DEFINE_SIMD_UNARY_OP_FOR_FP(FUNCNAME, SIMD_OP, TAIL_EXPR_F32, TAIL_EXPR_F64) \
  STDCOLT_DEFINE_SIMD_UNARY_OP(FUNCNAME, float,  SIZE_F32, simdpp::float32<SIZE_F32>, SIMD_OP, TAIL_EXPR_F32) \
  STDCOLT_DEFINE_SIMD_UNARY_OP(FUNCNAME, double, SIZE_F64, simdpp::float64<SIZE_F64>, SIMD_OP, TAIL_EXPR_F64)

#define STDCOLT_DEFINE_SIMD_UNARY_OP_FOR_F32(FUNCNAME, SIMD_OP, TAIL_EXPR_F32) \
  STDCOLT_DEFINE_SIMD_UNARY_OP(FUNCNAME, float,  SIZE_F32, simdpp::float32<SIZE_F32>, SIMD_OP, TAIL_EXPR_F32)

// clang-format on

namespace stdcolt::simd::details
{
  static constexpr size_t UNROLL_FACTOR = 4;
  static constexpr size_t SIZE_I8       = SIMDPP_FAST_INT8_SIZE;
  static constexpr size_t SIZE_I16      = SIMDPP_FAST_INT16_SIZE;
  static constexpr size_t SIZE_I32      = SIMDPP_FAST_INT32_SIZE;
  static constexpr size_t SIZE_I64      = SIMDPP_FAST_INT64_SIZE;
  static constexpr size_t SIZE_F32      = SIMDPP_FAST_FLOAT32_SIZE;
  static constexpr size_t SIZE_F64      = SIMDPP_FAST_FLOAT64_SIZE;

  STDCOLT_DEFINE_SIMD_BINARY_OP_FOR_INT(add, add, +)
  STDCOLT_DEFINE_SIMD_BINARY_OP_FOR_FP(add, add, +)
  STDCOLT_DEFINE_SIMD_BINARY_OP_FOR_INT(sub, sub, -)
  STDCOLT_DEFINE_SIMD_BINARY_OP_FOR_FP(sub, sub, -)
  STDCOLT_DEFINE_SIMD_BINARY_OP_FOR_FP(mul, mul, *)
  STDCOLT_DEFINE_SIMD_BINARY_OP_FOR_FP(div, div, /)
  STDCOLT_DEFINE_SIMD_UNARY_OP_FOR_SINT(neg, neg, -a[i])
  STDCOLT_DEFINE_SIMD_UNARY_OP_FOR_FP(
      neg, neg, static_cast<float>(-a[i]), static_cast<double>(-a[i]))
  STDCOLT_DEFINE_SIMD_UNARY_OP_FOR_SINT(abs, abs, std::abs(a[i]))
  STDCOLT_DEFINE_SIMD_UNARY_OP_FOR_FP(
      abs, abs, static_cast<float>(std::abs(a[i])),
      static_cast<double>(std::abs(a[i])))
  STDCOLT_DEFINE_SIMD_UNARY_OP_FOR_FP(
      sqrt, sqrt, static_cast<float>(std::sqrt(a[i])),
      static_cast<double>(std::sqrt(a[i])))
  STDCOLT_DEFINE_SIMD_UNARY_OP_FOR_FP(
      floor, floor, static_cast<float>(std::floor(a[i])),
      static_cast<double>(std::floor(a[i])))
  STDCOLT_DEFINE_SIMD_UNARY_OP_FOR_FP(
      ceil, ceil, static_cast<float>(std::ceil(a[i])),
      static_cast<double>(std::ceil(a[i])))
  STDCOLT_DEFINE_SIMD_UNARY_OP_FOR_FP(
      trunc, trunc, static_cast<float>(std::trunc(a[i])),
      static_cast<double>(std::trunc(a[i])))
  STDCOLT_DEFINE_SIMD_UNARY_OP_FOR_F32(
      rsqrt, rsqrt_e, static_cast<float>(1.0f / std::sqrt(a[i])))
  STDCOLT_DEFINE_SIMD_UNARY_OP_FOR_F32(rcp, rcp_e, static_cast<float>(1.0f / a[i]))

} // namespace stdcolt::simd::details

#undef STDCOLT_DEFINE_SIMD_BINARY_OP