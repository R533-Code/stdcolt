#ifndef __HG_STDCOLT_SIMD_SIMD
#define __HG_STDCOLT_SIMD_SIMD

#include <stdcolt_simd_export.h>
#include <cstdint>
#include <span>

#define STDCOLT_DECLARE_SIMD_BINARY_OP(FUNCNAME, TYPE)                   \
  namespace details                                                      \
  {                                                                      \
    STDCOLT_SIMD_EXPORT                                                  \
    void FUNCNAME(size_t size, const TYPE* a, const TYPE* b, TYPE* out); \
  }                                                                      \
  inline void FUNCNAME(                                                  \
      size_t size, const TYPE* a, const TYPE* b, TYPE* out) noexcept     \
  {                                                                      \
    return details::FUNCNAME(size, a, b, out);                           \
  }
#define STDCOLT_DECLARE_SIMD_UNARY_OP(FUNCNAME, TYPE)                  \
  namespace details                                                    \
  {                                                                    \
    STDCOLT_SIMD_EXPORT                                                \
    void FUNCNAME(size_t size, const TYPE* a, TYPE* out);              \
  }                                                                    \
  inline void FUNCNAME(size_t size, const TYPE* a, TYPE* out) noexcept \
  {                                                                    \
    return details::FUNCNAME(size, a, out);                            \
  }

#define STDCOLT_DECLARE_SIMD_BINARY_OP_FOR_INT(FUNCNAME) \
  STDCOLT_DECLARE_SIMD_BINARY_OP(FUNCNAME, uint8_t)      \
  STDCOLT_DECLARE_SIMD_BINARY_OP(FUNCNAME, uint16_t)     \
  STDCOLT_DECLARE_SIMD_BINARY_OP(FUNCNAME, uint32_t)     \
  STDCOLT_DECLARE_SIMD_BINARY_OP(FUNCNAME, uint64_t)     \
  STDCOLT_DECLARE_SIMD_BINARY_OP(FUNCNAME, int8_t)       \
  STDCOLT_DECLARE_SIMD_BINARY_OP(FUNCNAME, int16_t)      \
  STDCOLT_DECLARE_SIMD_BINARY_OP(FUNCNAME, int32_t)      \
  STDCOLT_DECLARE_SIMD_BINARY_OP(FUNCNAME, int64_t)

#define STDCOLT_DECLARE_SIMD_BINARY_OP_FOR_FP(FUNCNAME) \
  STDCOLT_DECLARE_SIMD_BINARY_OP(FUNCNAME, float)       \
  STDCOLT_DECLARE_SIMD_BINARY_OP(FUNCNAME, double)

#define STDCOLT_DECLARE_SIMD_UNARY_OP_FOR_INT(FUNCNAME) \
  STDCOLT_DECLARE_SIMD_UNARY_OP(FUNCNAME, uint8_t)      \
  STDCOLT_DECLARE_SIMD_UNARY_OP(FUNCNAME, uint16_t)     \
  STDCOLT_DECLARE_SIMD_UNARY_OP(FUNCNAME, uint32_t)     \
  STDCOLT_DECLARE_SIMD_UNARY_OP(FUNCNAME, uint64_t)     \
  STDCOLT_DECLARE_SIMD_UNARY_OP(FUNCNAME, int8_t)       \
  STDCOLT_DECLARE_SIMD_UNARY_OP(FUNCNAME, int16_t)      \
  STDCOLT_DECLARE_SIMD_UNARY_OP(FUNCNAME, int32_t)      \
  STDCOLT_DECLARE_SIMD_UNARY_OP(FUNCNAME, int64_t)

#define STDCOLT_DECLARE_SIMD_UNARY_OP_FOR_SINT(FUNCNAME) \
  STDCOLT_DECLARE_SIMD_UNARY_OP(FUNCNAME, int8_t)        \
  STDCOLT_DECLARE_SIMD_UNARY_OP(FUNCNAME, int16_t)       \
  STDCOLT_DECLARE_SIMD_UNARY_OP(FUNCNAME, int32_t)       \
  STDCOLT_DECLARE_SIMD_UNARY_OP(FUNCNAME, int64_t)

#define STDCOLT_DECLARE_SIMD_UNARY_OP_FOR_FP(FUNCNAME) \
  STDCOLT_DECLARE_SIMD_UNARY_OP(FUNCNAME, float)       \
  STDCOLT_DECLARE_SIMD_UNARY_OP(FUNCNAME, double)

namespace stdcolt::simd
{
  STDCOLT_DECLARE_SIMD_BINARY_OP_FOR_INT(add)
  STDCOLT_DECLARE_SIMD_BINARY_OP_FOR_FP(add)
  STDCOLT_DECLARE_SIMD_BINARY_OP_FOR_INT(sub)
  STDCOLT_DECLARE_SIMD_BINARY_OP_FOR_FP(sub)
  STDCOLT_DECLARE_SIMD_BINARY_OP_FOR_FP(mul)
  STDCOLT_DECLARE_SIMD_BINARY_OP_FOR_FP(div)
  STDCOLT_DECLARE_SIMD_UNARY_OP_FOR_SINT(neg)
  STDCOLT_DECLARE_SIMD_UNARY_OP_FOR_FP(neg)
  STDCOLT_DECLARE_SIMD_UNARY_OP_FOR_SINT(abs)
  STDCOLT_DECLARE_SIMD_UNARY_OP_FOR_FP(abs)
  STDCOLT_DECLARE_SIMD_UNARY_OP_FOR_FP(sqrt)
  STDCOLT_DECLARE_SIMD_UNARY_OP_FOR_FP(floor)
  STDCOLT_DECLARE_SIMD_UNARY_OP_FOR_FP(ceil)
  STDCOLT_DECLARE_SIMD_UNARY_OP_FOR_FP(trunc)
  
  STDCOLT_DECLARE_SIMD_UNARY_OP(rsqrt, float)
  STDCOLT_DECLARE_SIMD_UNARY_OP(rcp, float)
} // namespace stdcolt::simd

#undef STDCOLT_DECLARE_SIMD_BINARY_OP_FOR_FP
#undef STDCOLT_DECLARE_SIMD_BINARY_OP_FOR_INT
#undef STDCOLT_DECLARE_SIMD_BINARY_OP

#endif // !__HG_STDCOLT_SIMD_SIMD
