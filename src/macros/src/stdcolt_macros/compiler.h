/*****************************************************************/ /**
 * @file   compiler.h
 * @brief  Contains macros to abstract compiler differences.
 * 
 * @author Raphael Dib Nehme
 * @date   October 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_MACROS_COMPILER
#define __HG_STDCOLT_MACROS_COMPILER

#if defined(_MSC_VER)
  #define STDCOLT_MSVC 1
#else
  #define STDCOLT_MSVC 0
#endif

#if defined(__clang__)
  #define STDCOLT_CLANG 1
#else
  #define STDCOLT_CLANG 0
#endif

#if defined(__GNUC__) && !STDCOLT_CLANG
  #define STDCOLT_GCC 1
#else
  #define STDCOLT_GCC 0
#endif

#if STDCOLT_MSVC
  /// @brief Forces inlining of a function
  #define STDCOLT_FORCE_INLINE __forceinline
#elif STDCOLT_GCC || STDCOLT_CLANG
  /// @brief Forces inlining of a function
  #define STDCOLT_FORCE_INLINE inline __attribute__((always_inline))
#else
  /// @brief Forces inlining of a function
  #define STDCOLT_FORCE_INLINE inline
#endif

#if STDCOLT_MSVC
  /// @brief Forces no-inlining of a function
  #define STDCOLT_NO_INLINE __declspec(noinline)
#elif STDCOLT_GCC || STDCOLT_CLANG
  /// @brief Forces no-inlining of a function
  #define STDCOLT_NO_INLINE __attribute__((noinline))
#else
  /// @brief Forces no-inlining of a function
  #define STDCOLT_NO_INLINE
#endif

#if STDCOLT_MSVC
  /// @brief Exports a global
  #define STDCOLT_EXPORT __declspec(dllexport)
  /// @brief Imports a global
  #define STDCOLT_IMPORT __declspec(dllimport)
#elif STDCOLT_GCC || STDCOLT_CLANG
  /// @brief Exports a global
  #define STDCOLT_EXPORT __attribute__((visibility("default")))
  /// @brief Imports a global
  #define STDCOLT_IMPORT
#else
  /// @brief Exports a global
  #define STDCOLT_EXPORT
  /// @brief Imports a global
  #define STDCOLT_IMPORT
#endif

#define __STDCOLT_STRINGIFY(x) #x
#define STDCOLT_STRINGIFY(x)   __STDCOLT_STRINGIFY(x)

#if STDCOLT_GCC || STDCOLT_CLANG
  #define STDCOLT_DO_PRAGMA(x) _Pragma(#x)
#elif STDCOLT_MSVC
  #define STDCOLT_DO_PRAGMA(x) __pragma(x)
#endif

#if STDCOLT_MSVC && STDCOLT_CLANG
  #define STDCOLT_WARNING_PUSH \
    STDCOLT_DO_PRAGMA(warning(push)) STDCOLT_DO_PRAGMA(clang diagnostic push)
  #define STDCOLT_WARNING_POP \
    STDCOLT_DO_PRAGMA(warning(pop)) STDCOLT_DO_PRAGMA(clang diagnostic pop)
#elif STDCOLT_MSVC
  #define STDCOLT_WARNING_PUSH STDCOLT_DO_PRAGMA(warning(push))
  #define STDCOLT_WARNING_POP  STDCOLT_DO_PRAGMA(warning(pop))
#elif STDCOLT_CLANG
  #define STDCOLT_WARNING_PUSH STDCOLT_DO_PRAGMA(clang diagnostic push)
  #define STDCOLT_WARNING_POP  STDCOLT_DO_PRAGMA(clang diagnostic pop)
#elif STDCOLT_GCC
  #define STDCOLT_WARNING_PUSH STDCOLT_DO_PRAGMA(GCC diagnostic push)
  #define STDCOLT_WARNING_POP  STDCOLT_DO_PRAGMA(GCC diagnostic pop)
#else
  #define STDCOLT_WARNING_PUSH
  #define STDCOLT_WARNING_POP
#endif

#if STDCOLT_MSVC && STDCOLT_CLANG
  #define STDCOLT_DISABLE_MSVC_WARNING(code) \
    STDCOLT_DO_PRAGMA(warning(disable : code))
  #define STDCOLT_DISABLE_CLANG_WARNING(flag) \
    STDCOLT_DO_PRAGMA(clang diagnostic ignored STDCOLT_STRINGIFY(flag))
  #define STDCOLT_DISABLE_GCC_WARNING(flag) \
    STDCOLT_DO_PRAGMA(GCC diagnostic ignored STDCOLT_STRINGIFY(flag))
  #define STDCOLT_DISABLE_WARNING(code, flag)  \
    STDCOLT_DO_PRAGMA(warning(disable : code)) \
    STDCOLT_DO_PRAGMA(clang diagnostic ignored STDCOLT_STRINGIFY(flag))
#elif STDCOLT_MSVC
  #define STDCOLT_DISABLE_MSVC_WARNING(code) \
    STDCOLT_DO_PRAGMA(warning(disable : code))
  #define STDCOLT_DISABLE_CLANG_WARNING(flag)
  #define STDCOLT_DISABLE_GCC_WARNING(flag)
  #define STDCOLT_DISABLE_WARNING(code, flag) \
    STDCOLT_DO_PRAGMA(warning(disable : code))
#elif STDCOLT_CLANG
  #define STDCOLT_DISABLE_MSVC_WARNING(code)
  #define STDCOLT_DISABLE_CLANG_WARNING(flag) \
    STDCOLT_DO_PRAGMA(clang diagnostic ignored STDCOLT_STRINGIFY(flag))
  #define STDCOLT_DISABLE_GCC_WARNING(flag)
  #define STDCOLT_DISABLE_WARNING(code, flag) \
    STDCOLT_DO_PRAGMA(clang diagnostic ignored STDCOLT_STRINGIFY(flag))
#elif STDCOLT_GCC
  #define STDCOLT_DISABLE_MSVC_WARNING(code)
  #define STDCOLT_DISABLE_CLANG_WARNING(flag)
  #define STDCOLT_DISABLE_GCC_WARNING(flag) \
    STDCOLT_DO_PRAGMA(GCC diagnostic ignored STDCOLT_STRINGIFY(flag))
  #define STDCOLT_DISABLE_WARNING(code, flag) \
    STDCOLT_DO_PRAGMA(GCC diagnostic ignored STDCOLT_STRINGIFY(flag))
#else
  #define STDCOLT_DISABLE_MSVC_WARNING(code)
  #define STDCOLT_DISABLE_CLANG_WARNING(flag)
  #define STDCOLT_DISABLE_GCC_WARNING(flag)
  #define STDCOLT_DISABLE_WARNING(code, flag)
#endif

#if defined(__has_cpp_attribute)
  #define STDCOLT_HAS_CPP_ATTR(x) __has_cpp_attribute(x)
#else
  #define STDCOLT_HAS_CPP_ATTR(x) 0
#endif

#if STDCOLT_HAS_CPP_ATTR(nodiscard) >= 201603
  #define STDCOLT_NODISCARD [[nodiscard]]
#else
  #define STDCOLT_NODISCARD
#endif

#if STDCOLT_HAS_CPP_ATTR(deprecated) >= 201309
  #define STDCOLT_DEPRECATED(msg) [[deprecated(msg)]]
#elif STDCOLT_MSVC
  #define STDCOLT_DEPRECATED(msg) __declspec(deprecated(msg))
#elif STDCOLT_GCC || STDCOLT_CLANG
  #define STDCOLT_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
  #define STDCOLT_DEPRECATED(msg)
#endif

#if STDCOLT_HAS_CPP_ATTR(maybe_unused) >= 201603
  #define STDCOLT_MAYBE_UNUSED [[maybe_unused]]
#elif STDCOLT_GCC || STDCOLT_CLANG
  #define STDCOLT_MAYBE_UNUSED __attribute__((unused))
#else
  #define STDCOLT_MAYBE_UNUSED
#endif

#if STDCOLT_HAS_CPP_ATTR(fallthrough) >= 201603
  #define STDCOLT_FALLTHROUGH [[fallthrough]]
#elif STDCOLT_GCC || STDCOLT_CLANG
  #define STDCOLT_FALLTHROUGH __attribute__((fallthrough))
#else
  #define STDCOLT_FALLTHROUGH
#endif

#if STDCOLT_GCC || STDCOLT_CLANG
  #define STDCOLT_LIKELY(x)   __builtin_expect(!!(x), 1)
  #define STDCOLT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
  #define STDCOLT_LIKELY(x)   (x)
  #define STDCOLT_UNLIKELY(x) (x)
#endif

#if STDCOLT_MSVC
  #define STDCOLT_ASSUME(x)     __assume(x)
  #define STDCOLT_UNREACHABLE() __assume(0)
#elif STDCOLT_CLANG
  #define STDCOLT_ASSUME(x)     __builtin_assume(x)
  #define STDCOLT_UNREACHABLE() __builtin_unreachable()
#elif STDCOLT_GCC
  #define STDCOLT_ASSUME(x)      \
    do                           \
    {                            \
      if (!(x))                  \
        __builtin_unreachable(); \
    } while (0)
  #define STDCOLT_UNREACHABLE() __builtin_unreachable()
#else
  #define STDCOLT_ASSUME(x)     ((void)0)
  #define STDCOLT_UNREACHABLE() ((void)0)
#endif

#if STDCOLT_MSVC
  #define STDCOLT_FUNC __FUNCTION__
#else
  #define STDCOLT_FUNC __func__
#endif

#if STDCOLT_MSVC
  #define STDCOLT_RESTRICT __restrict
#elif STDCOLT_GCC || STDCOLT_CLANG
  #define STDCOLT_RESTRICT __restrict__
#else
  #define STDCOLT_RESTRICT
#endif

#endif // !__HG_STDCOLT_MACROS_COMPILER
