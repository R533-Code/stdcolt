/*****************************************************************/ /**
 * @file   feature_detection.cpp
 * @brief  Contains the implementation of `feature_detection.h`.
 * @date   Oct 2025
 *********************************************************************/
#include "feature_detection.h"
#include <atomic>

#if defined(_WIN32)
  #define NOMINMAX
  #include <windows.h>
#endif

#if defined(__linux__) || defined(__ANDROID__)
  #include <sys/auxv.h>
  #include <asm/hwcap.h>
#endif

#if defined(__APPLE__)
  #include <TargetConditionals.h>
  #include <sys/types.h>
  #include <sys/sysctl.h>
#endif

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
  #include <intrin.h>
#elif (defined(__i386__) || defined(__x86_64__))
  #include <cpuid.h>
#endif

namespace stdcolt::simd
{
  static inline void cpuid(
      unsigned leaf, unsigned subleaf, unsigned& eax, unsigned& ebx, unsigned& ecx,
      unsigned& edx)
  {
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    int regs[4];
    __cpuidex(regs, static_cast<int>(leaf), static_cast<int>(subleaf));
    eax = static_cast<unsigned>(regs[0]);
    ebx = static_cast<unsigned>(regs[1]);
    ecx = static_cast<unsigned>(regs[2]);
    edx = static_cast<unsigned>(regs[3]);
#elif (defined(__i386__) || defined(__x86_64__))
    __cpuid_count(leaf, subleaf, eax, ebx, ecx, edx);
#else
    eax = ebx = ecx = edx = 0;
#endif
  }

  static inline unsigned long long xgetbv(unsigned idx)
  {
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    return _xgetbv(idx);
#elif (defined(__i386__) || defined(__x86_64__))
    unsigned eax, edx;
    __asm__ volatile(".byte 0x0f, 0x01, 0xd0" : "=a"(eax), "=d"(edx) : "c"(idx));
    return (static_cast<unsigned long long>(edx) << 32) | eax;
#else
    return 0;
#endif
  }

  static FeatureMask detect_x86()
  {
#if !(                                                           \
    defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) \
    || defined(_M_X64))
    return 0;
#else
    FeatureMask m = 0;

    unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
    cpuid(0, 0, eax, ebx, ecx, edx);
    unsigned max_leaf = eax;

    // Leaf 1: base features
    cpuid(1, 0, eax, ebx, ecx, edx);
    const bool sse2    = (edx & (1u << 26)) != 0;
    const bool sse3    = (ecx & (1u << 0)) != 0;
    const bool ssse3   = (ecx & (1u << 9)) != 0;
    const bool sse41   = (ecx & (1u << 19)) != 0;
    const bool sse42   = (ecx & (1u << 20)) != 0;
    const bool popcnt  = (ecx & (1u << 23)) != 0;
    const bool aesni   = (ecx & (1u << 25)) != 0;
    const bool osxsave = (ecx & (1u << 27)) != 0;
    const bool avx_hw  = (ecx & (1u << 28)) != 0;
    const bool f16c    = (ecx & (1u << 29)) != 0;
    const bool fma     = (ecx & (1u << 12)) != 0;

    if (sse2)
      m |= Feature::X86_SSE2;
    if (ssse3 || sse3)
      m |= Feature::X86_SSSE3; // treat as SSSE3 gate for simplicity
    if (sse41)
      m |= Feature::X86_SSE41;
    if (sse42)
      m |= Feature::X86_SSE42;
    if (popcnt)
      m |= Feature::X86_POPCNT;
    if (aesni)
      m |= Feature::X86_AESNI;
    if (f16c)
      m |= Feature::X86_F16C;
    if (fma)
      m |= Feature::X86_FMA;

    // OS support check for AVX state
    bool os_avx    = false;
    bool os_avx512 = false;
    if (osxsave)
    {
      unsigned long long xcr0 = xgetbv(0);
      const bool xmm          = (xcr0 & 0x2) != 0;
      const bool ymm          = (xcr0 & 0x4) != 0;
      os_avx                  = xmm && ymm;
      // For AVX-512 need Opmask(5), ZMM_Hi256(6), Hi16_ZMM(7)
      os_avx512 = os_avx && ((xcr0 & 0xE0) == 0xE0);
    }

    // Leaf 7.0: extended features
    if (max_leaf >= 7)
    {
      cpuid(7, 0, eax, ebx, ecx, edx);
      const bool bmi1       = (ebx & (1u << 3)) != 0;
      const bool avx2       = (ebx & (1u << 5)) != 0;
      const bool bmi2       = (ebx & (1u << 8)) != 0;
      const bool avx512f    = (ebx & (1u << 16)) != 0;
      const bool avx512dq   = (ebx & (1u << 17)) != 0;
      const bool avx512cd   = (ebx & (1u << 28)) != 0;
      const bool avx512bw   = (ebx & (1u << 30)) != 0;
      const bool avx512vl   = (ebx & (1u << 31)) != 0;
      const bool avx512vbmi = (ecx & (1u << 1)) != 0;
      const bool vaes       = (ecx & (1u << 9)) != 0;
      const bool vpclmulqdq = (ecx & (1u << 10)) != 0;
      const bool avx512vnni = (ecx & (1u << 11)) != 0;

      if (bmi1)
        m |= Feature::X86_BMI1;
      if (bmi2)
        m |= Feature::X86_BMI2;
      if (os_avx && avx_hw)
      {
        m |= Feature::X86_AVX;
        if (avx2)
          m |= Feature::X86_AVX2;
        if (vaes)
          m |= Feature::X86_VAES;
        if (vpclmulqdq)
          m |= Feature::X86_VPCLMULQDQ;
      }
      if (os_avx512 && avx512f)
      {
        m |= Feature::X86_AVX512F;
        if (avx512dq)
          m |= Feature::X86_AVX512DQ;
        if (avx512cd)
          m |= Feature::X86_AVX512CD;
        if (avx512bw)
          m |= Feature::X86_AVX512BW;
        if (avx512vl)
          m |= Feature::X86_AVX512VL;
        if (avx512vnni)
          m |= Feature::X86_AVX512VNNI;
        if (avx512vbmi)
          m |= Feature::X86_AVX512VBMI;
      }
    }

    return m;
#endif
  }

  static FeatureMask detect_arm()
  {
#if !(                                                            \
    defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) \
    || defined(_M_ARM))
    return 0;
#else
    FeatureMask m = 0;

    // Defaults per platform
  #if defined(__APPLE__) && defined(__aarch64__)
    // Apple Silicon always has NEON. Other flags vary; be conservative.
    m |= Feature::ARM_NEON;
      // macOS 12+ exposes some sysctls but they are not stable; skip to avoid false positives.
  #endif

  #if (defined(__linux__) || defined(__ANDROID__)) \
      && (defined(__aarch64__) || defined(__arm__))
    unsigned long hwcap = 0, hwcap2 = 0;
    // getauxval returns 0 if key not found.
    hwcap = getauxval(AT_HWCAP);
    #ifdef AT_HWCAP2
    hwcap2 = getauxval(AT_HWCAP2);
    #endif

    // HWCAP (aarch64)
    #ifdef HWCAP_ASIMD
    if (hwcap & HWCAP_ASIMD)
      m |= Feature::ARM_NEON;
    #endif
    #ifdef HWCAP_ASIMDDP
    if (hwcap & HWCAP_ASIMDDP)
      m |= Feature::ARM_DOTPROD;
    #endif
    #ifdef HWCAP_I8MM // Some libcs define this in HWCAP (glibc 2.37+ puts it in HWCAP2; guard both)
    if (hwcap & HWCAP_I8MM)
      m |= Feature::ARM_I8MM;
    #endif
    #ifdef HWCAP_BF16
    if (hwcap & HWCAP_BF16)
      m |= Feature::ARM_BF16;
    #endif
    #ifdef HWCAP_SVE
    if (hwcap & HWCAP_SVE)
      m |= Feature::ARM_SVE;
    #endif
    #ifdef HWCAP_AES
    if (hwcap & HWCAP_AES)
      m |= Feature::ARM_AES;
    #endif
    #ifdef HWCAP_PMULL
    if (hwcap & HWCAP_PMULL)
      m |= Feature::ARM_PMULL;
    #endif
    #ifdef HWCAP_SHA1
    if (hwcap & HWCAP_SHA1)
      m |= Feature::ARM_SHA1;
    #endif
    #ifdef HWCAP_SHA2
    if (hwcap & HWCAP_SHA2)
      m |= Feature::ARM_SHA2;
    #endif
    #ifdef HWCAP_CRC32
    if (hwcap & HWCAP_CRC32)
      m |= Feature::ARM_CRC32;
    #endif

    // HWCAP2 extras
    #ifdef HWCAP2_SVE2
    if (hwcap2 & HWCAP2_SVE2)
      m |= Feature::ARM_SVE2;
    #endif
    #ifdef HWCAP2_I8MM
    if (hwcap2 & HWCAP2_I8MM)
      m |= Feature::ARM_I8MM;
    #endif
    #ifdef HWCAP2_BF16
    if (hwcap2 & HWCAP2_BF16)
      m |= Feature::ARM_BF16;
    #endif
  #endif

  #if defined(_WIN32) && defined(_M_ARM64)
    // Windows on ARM64: query what the OS exposes.
    // NEON
    #ifndef PF_ARM_NEON_INSTRUCTIONS_AVAILABLE
      #define PF_ARM_NEON_INSTRUCTIONS_AVAILABLE 19
    #endif
    if (IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE))
      m |= Feature::ARM_NEON;

    // AES and CRC32 if available
    #ifndef PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE
      #define PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE 30
    #endif
    #ifndef PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE
      #define PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE 31
    #endif
    if (IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE))
    {
      m |= Feature::ARM_AES | Feature::ARM_PMULL | Feature::ARM_SHA1
           | Feature::ARM_SHA2;
    }
    if (IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE))
    {
      m |= Feature::ARM_CRC32;
    }
  #endif

    return m;
#endif
  }

  static std::atomic<FeatureMask> OVERRIDE_MASK = ~(FeatureMask)0;

  void override_disabled_features(FeatureMask disable_mask) noexcept
  {
    OVERRIDE_MASK.store(~disable_mask, std::memory_order_relaxed);
  }

  FeatureMask detect_features() noexcept
  {
    // always cache the detected features as they should never
    // change during process execution
    static FeatureMask cached = []
    {
      FeatureMask m = 0;
      m |= detect_x86();
      m |= detect_arm();
      return m;
    }();
    return cached & OVERRIDE_MASK.load(std::memory_order_relaxed);
  }

} // namespace stdcolt::simd
