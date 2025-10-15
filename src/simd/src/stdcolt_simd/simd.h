#ifndef __HG_STDCOLT_SIMD_SIMD
#define __HG_STDCOLT_SIMD_SIMD

#include <stdcolt_simd/feature_detection.h>
#include <stdcolt_simd_export.h>
#include <cstdint>

#define _STDCOLT_SIMD_BINARY_OVERLOAD(name, type) \
  STDCOLT_SIMD_EXPORT                             \
  void name(size_t size, const type* a, const type* b, type* out) noexcept;

#define _STDCOLT_SIMD_BINARY_OVERLOAD_SET_INT(name) \
  _STDCOLT_SIMD_BINARY_OVERLOAD(name, uint8_t)      \
  _STDCOLT_SIMD_BINARY_OVERLOAD(name, uint16_t)     \
  _STDCOLT_SIMD_BINARY_OVERLOAD(name, uint32_t)     \
  _STDCOLT_SIMD_BINARY_OVERLOAD(name, uint64_t)     \
  _STDCOLT_SIMD_BINARY_OVERLOAD(name, int8_t)       \
  _STDCOLT_SIMD_BINARY_OVERLOAD(name, int16_t)      \
  _STDCOLT_SIMD_BINARY_OVERLOAD(name, int32_t)      \
  _STDCOLT_SIMD_BINARY_OVERLOAD(name, int64_t)

namespace stdcolt::simd
{
  /// @brief Rebuilds the cache of optimal overloads by re-querying `detect_features`.
  /// This is only useful if `override_detect_features` is used.
  /// @warning This function is not guaranteed to be thread safe.
  STDCOLT_SIMD_EXPORT
  void rebuild_optimal_overloads() noexcept;

  _STDCOLT_SIMD_BINARY_OVERLOAD_SET_INT(add);
} // namespace stdcolt::simd

#undef _STDCOLT_SIMD_BINARY_OVERLOAD
#undef _STDCOLT_SIMD_BINARY_OVERLOAD_SET_INT

#endif // !__HG_STDCOLT_SIMD_SIMD
