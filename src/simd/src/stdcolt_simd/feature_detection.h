/*****************************************************************/ /**
 * @file   feature_detection.h
 * @brief  Contains `detect_features` for runtime detection of CPU features.
 * @date   Oct 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_SIMD_FEATURE_DETECTION
#define __HG_STDCOLT_SIMD_FEATURE_DETECTION

#include <cstdint>
#include "stdcolt_simd_export.h"

namespace stdcolt::simd
{
  /// @brief Mask to detect CPU features
  using FeatureMask = std::uint64_t;

  /// @brief The possible CPU features
  enum class Feature : FeatureMask
  {
    None = 0,

    // x86 / x86_64
    X86_SSE2       = 1ULL << 0,
    X86_SSSE3      = 1ULL << 1,
    X86_SSE41      = 1ULL << 2,
    X86_SSE42      = 1ULL << 3,
    X86_POPCNT     = 1ULL << 4,
    X86_AESNI      = 1ULL << 5,
    X86_FMA        = 1ULL << 6,
    X86_F16C       = 1ULL << 7,
    X86_AVX        = 1ULL << 8,
    X86_AVX2       = 1ULL << 9,
    X86_BMI1       = 1ULL << 10,
    X86_BMI2       = 1ULL << 11,
    X86_AVX512F    = 1ULL << 12,
    X86_AVX512DQ   = 1ULL << 13,
    X86_AVX512CD   = 1ULL << 14,
    X86_AVX512BW   = 1ULL << 15,
    X86_AVX512VL   = 1ULL << 16,
    X86_AVX512VNNI = 1ULL << 17,
    X86_AVX512VBMI = 1ULL << 18,
    X86_VAES       = 1ULL << 19,
    X86_VPCLMULQDQ = 1ULL << 20,

    // ARM / AArch64
    ARM_NEON    = 1ULL << 32,
    ARM_DOTPROD = 1ULL << 33,
    ARM_I8MM    = 1ULL << 34,
    ARM_BF16    = 1ULL << 35,
    ARM_SVE     = 1ULL << 36,
    ARM_SVE2    = 1ULL << 37,
    ARM_AES     = 1ULL << 38,
    ARM_PMULL   = 1ULL << 39,
    ARM_SHA1    = 1ULL << 40,
    ARM_SHA2    = 1ULL << 41,
    ARM_CRC32   = 1ULL << 42,
  };

  inline FeatureMask operator|(Feature a, Feature b)
  {
    return static_cast<FeatureMask>(a) | static_cast<FeatureMask>(b);
  }
  inline FeatureMask& operator|=(FeatureMask& m, Feature f)
  {
    m |= static_cast<FeatureMask>(f);
    return m;
  }
  inline bool has(FeatureMask m, Feature f)
  {
    return (m & static_cast<FeatureMask>(f)) != 0;
  }

  /// @brief Modifies the value returned by `detect_features`.
  /// The value returned by `detect_features` is bitwise ANDed with
  /// NOT of the mask set by `override_detect_features`. By default,
  /// the override mask is set to all zeros, which means that all the
  /// existing features are enabled.
  /// @param disable_mask The mask of features to disable
  /// @note This function is thread-safe.
  STDCOLT_SIMD_EXPORT
  void override_disabled_features(FeatureMask disable_mask) noexcept;

  /// @brief Detects the current supported CPU features.
  /// This function detects the supported features at runtime. This detection
  /// only happens once and is cached for subsequent calls. The returned value
  /// is always bitwise AND with the mask set by `override_detect_features`,
  /// which allows to forcefully assume a feature is not available.
  /// @return FeatureMask where the bits are set to mark an available feature
  /// @note This function is thread-safe.
  STDCOLT_SIMD_EXPORT
  FeatureMask detect_features() noexcept;
} // namespace stdcolt::simd

#endif // !__HG_STDCOLT_SIMD_FEATURE_DETECTION
