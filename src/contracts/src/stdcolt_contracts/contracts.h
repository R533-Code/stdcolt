/*****************************************************************/ /**
 * @file   contracts.h
 * @brief  Contains macros for assertions, pre/post conditions.
 * 
 * @author Raphael Dib Nehme
 * @date   Oct 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_CONTRACTS_CONTRACTS
#define __HG_STDCOLT_CONTRACTS_CONTRACTS

#include "stdcolt_contracts_export.h"
#include "stdcolt_contracts_config.h"
#include <cstdint>
#include <source_location>
#include <type_traits>
#include <optional>

// If STDCOLT_NO_SOURCE_LOCATION is specified, make use of default constructed
// source locations.
#ifdef STDCOLT_NO_SOURCE_LOCATION
  /// @brief The current source location
  #define STDCOLT_CURRENT_SOURCE_LOCATION std::source_location()
#else
  /// @brief The current source location
  #define STDCOLT_CURRENT_SOURCE_LOCATION std::source_location::current()
#endif // STDCOLT_NO_SOURCE_LOCATION

namespace stdcolt::contracts
{
  [[noreturn]] STDCOLT_CONTRACTS_EXPORT
      /// @brief Marks a branch as unreachable.
      /// Failure of upholding unreachability will trigger a contract violation.
      /// @param loc The source location
      void
      unreachable(
          const std::source_location& loc =
              STDCOLT_CURRENT_SOURCE_LOCATION) noexcept;

  /// @brief The contract kind
  enum class Kind : unsigned char
  {
    /// @brief Precondition
    Pre,
    /// @brief Postcondition
    Post,
    /// @brief Assertion
    Assert,
  };

  /// @brief A precondition failed at compile-time.
  /// @note The goal of this function is to be called in `consteval` functions!
  inline void precondition_failed_in_constexpr()
  {
    // A precondition failed at compile-time!
  }
  /// @brief A postcondition failed at compile-time.
  /// @note The goal of this function is to be called in `consteval` functions!
  inline void postcondition_failed_in_constexpr()
  {
    // A postcondition failed at compile-time!
  }
  /// @brief An assertion failed at compile-time.
  /// @note The goal of this function is to be called in `consteval` functions!
  inline void assertion_failed_in_constexpr()
  {
    // An assertion failed at compile-time!
  }
  /// @brief An invalid `kind` was passed to the violation handler.
  /// @note The goal of this function is to be called in `consteval` functions!
  inline void handler_failed_in_constexpr()
  {
    // An invalid `kind` was passed to the violation handler
  }

  /// @brief The type of a violation handler function
  using violation_handler_fn_t = void(
      const char*, const char*, Kind,
      const std::optional<std::source_location>&) noexcept;

  [[noreturn]] STDCOLT_CONTRACTS_EXPORT
      /// @brief The default runtime contract violation handler.
      /// Prints a stack trace and source code information, then aborts.
      /// @param expr The expression as a string
      /// @param explanation The explanation
      /// @param kind The kind of the violation
      /// @param loc The source location
      void
      default_runtime_violation_handler(
          const char* expr, const char* explanation, Kind kind,
          const std::optional<std::source_location>& loc) noexcept;

  STDCOLT_CONTRACTS_EXPORT
  /// @brief Calls the default runtime violation handler or a newly registered one.
  /// To replace the default runtime handler (`default_runtime_violation_handler`),
  /// call `register_violation_handler` with a non-null pointer.
  /// @param expr The expression as a string
  /// @param explanation The explanation
  /// @param kind The kind of the violation
  /// @param loc The source location
  void runtime_violation_handler(
      const char* expr, const char* explanation, Kind kind,
      const std::optional<std::source_location>& loc) noexcept;

  /// @brief The contract violation handler.
  /// This is the function to call on violation of contracts.
  /// At compile-time (due to C++ limitations), only a compilation error can be
  /// generated (no useful error message).
  /// At runtime, calls the runtime violation handler.
  /// @param expr The expression as a string
  /// @param explanation The explanation
  /// @param kind The violation kind
  /// @param loc The source code location
  constexpr void violation_handler(
      const char* expr, const char* explanation, Kind kind,
      const std::optional<std::source_location>& loc =
          STDCOLT_CURRENT_SOURCE_LOCATION) noexcept
  {
    using enum Kind;

    if (std::is_constant_evaluated())
    {
      // When in a constant evaluation context, calling
      // a non-constexpr function will result in a compilation error.
      // We make use of this feature to halt compilation.
      switch (kind)
      {
      case Pre:
        precondition_failed_in_constexpr();
        break;
      case Post:
        postcondition_failed_in_constexpr();
        break;
      case Assert:
        assertion_failed_in_constexpr();
        break;
      default:
        handler_failed_in_constexpr();
        break;
      }
    }
    else
    {
      runtime_violation_handler(expr, explanation, kind, loc);
    }
  }

  STDCOLT_CONTRACTS_EXPORT
  /// @brief Replaces the current violation handler with a new one.
  /// The registered function should not return: if it is called
  /// then a violation happened and program execution should abort.
  /// If `fn` is `nullptr`, this function does nothing.
  /// @param fn The new violation handler
  /// @note This function is thread safe without synchronicity.
  void register_violation_handler(violation_handler_fn_t* fn) noexcept;
} // namespace stdcolt::contracts

/// @brief switch case with no default
#define switch_no_default(...)           \
  switch (__VA_ARGS__)                   \
  default:                               \
    if (true)                            \
    {                                    \
      stdcolt::contracts::unreachable(); \
    }                                    \
    else

/// @brief Precondition (checks that `cond` evaluates to true)
#define STDCOLT_pre(cond, explanation)                        \
  do                                                          \
  {                                                           \
    if (!static_cast<bool>(cond))                             \
      stdcolt::contracts::violation_handler(                  \
          #cond, explanation, stdcolt::contracts::Kind::Pre); \
  } while (false)
/// @brief Postcondition (checks that `cond` evaluates to true)
#define STDCOLT_post(cond, explanation)                       \
  do                                                          \
  {                                                           \
    if (!static_cast<bool>(cond))                             \
      stdcolt::contracts::violation_handler(                  \
          #cond, explanation, stdcolt::contracts::Kind::Pre); \
  } while (false)
/// @brief Assertion (checks that `cond` evaluates to true)
#define STDCOLT_assert(cond, explanation)                     \
  do                                                          \
  {                                                           \
    if (!static_cast<bool>(cond))                             \
      stdcolt::contracts::violation_handler(                  \
          #cond, explanation, stdcolt::contracts::Kind::Pre); \
  } while (false)

#ifdef STDCOLT_DEBUG
  /// @brief Precondition that is only evaluated on Debug config
  #define STDCOLT_debug_pre(cond, explanation) STDCOLT_pre(cond, explanation)
  /// @brief Postcondition that is only evaluated on Debug config
  #define STDCOLT_debug_post(cond, explanation) STDCOLT_post(cond, explanation)
  /// @brief Assertion that is only evaluated on Debug config
  #define STDCOLT_debug_assert(cond, explanation) STDCOLT_assert(cond, explanation)
#else
  /// @brief Precondition that is only evaluated on Debug config
  #define STDCOLT_debug_pre(cond, explanation) \
    do                                         \
    {                                          \
    } while (false)
  /// @brief Postcondition that is only evaluated on Debug config
  #define STDCOLT_debug_post(cond, explanation) \
    do                                          \
    {                                           \
    } while (false)
  /// @brief Assertion that is only evaluated on Debug config
  #define STDCOLT_debug_assert(cond, explanation) \
    do                                            \
    {                                             \
    } while (false)
#endif // STDCOLT_DEBUG

#endif // !__HG_STDCOLT_CONTRACTS_CONTRACTS