/*****************************************************************/ /**
 * @file   tracing.h
 * @brief  Contains tracing macros.
 * 
 * @author Raphael Dib Nehme
 * @date   December 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_TRACING_TRACING
#define __HG_STDCOLT_TRACING_TRACING

#include <chrono>

#ifdef STDCOLT_ENABLE_TRACING
  #include <Tracy.hpp>

  #define STDCOLT_TRACE_THREAD_NAME(name) tracy::SetThreadName(name)
  /// @brief Traces the current function
  #define STDCOLT_TRACE_FN() ZoneScoped
  /// @brief Traces the current function (with color, 0xRRGGBB)
  #define STDCOLT_TRACE_FN_C(color) ZoneScopedC(color)
  /// @brief Traces a block (which is named)
  /// @code{.cpp}
  /// {
  ///   STDCOLT_TRACE_BLOCK("print_token")
  ///   for (auto& i : value.token_buffer())
  ///     print_token(i, value);
  /// }; // <- do not forget the semicolon!
  /// @endcode
  #define STDCOLT_TRACE_BLOCK(name) ZoneNamedN(, name, true)
  /// @brief Traces a block (which is named) (with color, 0xRRGGBB)
  /// @code{.cpp}
  /// {
  ///   STDCOLT_TRACE_BLOCK_C("print_token", 0xF0F0F0);
  ///   for (auto& i : value.token_buffer())
  ///     lng::print_token(i, value);
  /// }
  /// @endcode
  #define STDCOLT_TRACE_BLOCK_C(name, color) ZoneNamedNC(, name, color, true)
  /// @brief Traces a single expression
  #define STDCOLT_TRACE_EXPR(expr) \
    [&]() -> decltype(auto)        \
    {                              \
      ZoneScopedN(#expr);          \
      return (expr);               \
    }()
  /// @brief Traces a single expression (with color, 0xRRGGBB)
  #define STDCOLT_TRACE_EXPR_C(expr, color) \
    [&]() -> decltype(auto)                 \
    {                                       \
      ZoneScopedNC(#expr, color);           \
      return (expr);                        \
    }()

#else

  #define STDCOLT_TRACE_THREAD_NAME(name) (void)0
  /// @brief Traces the current function
  #define STDCOLT_TRACE_FN() \
    do                       \
    {                        \
    } while (0)
  /// @brief Traces the current function (with color, use clt::Color!)
  #define STDCOLT_TRACE_FN_C(color) \
    do                              \
    {                               \
    } while (0)
  /// @brief Traces a block (which is named)
  /// @code{.cpp}
  /// STDCOLT_TRACE_BLOCK("print_token")
  /// {
  ///   for (auto& i : value.token_buffer())
  ///     lng::print_token(i, value);
  /// }; // <- do not forget the semicolon!
  /// @endcode
  #define STDCOLT_TRACE_BLOCK(name)          (void)0
  /// @brief Traces a block (which is named) (with color, use clt::Color!)
  /// @code{.cpp}
  /// STDCOLT_TRACE_BLOCK_C("print_token", clt::Color::Chartreuse3)
  /// {
  ///   for (auto& i : value.token_buffer())
  ///     lng::print_token(i, value);
  /// }; // <- do not forget the semicolon!
  /// @endcode
  #define STDCOLT_TRACE_BLOCK_C(name, color) (void)0
  #define STDCOLT_TRACE_EXPR(expr)           expr
  #define STDCOLT_TRACE_EXPR_C(expr, color)  expr
#endif // STDCOLT_ENABLE_TRACING

namespace stdcolt
{
  /// @brief Check if the library is built with tracing enabled
  /// @return True if tracing is enabled
  consteval bool is_tracing_enabled() noexcept
  {
#ifndef STDCOLT_ENABLE_TRACING
    return false;
#else
    return true;
#endif
  }

  /// @brief Waits for the tracy profiler to be connected
  /// @param timeout The timeout in milliseconds after which to fail
  /// @return True if connection was successful, false otherwise
  bool wait_for_tracer(std::chrono::milliseconds timeout) noexcept;
  /// @brief Forces shutdown of the tracy profiler.
  /// This function waits for all the data to be transferred before returning.
  void shutdown_tracer() noexcept;
} // namespace lars

#endif // !__HG_STDCOLT_TRACING_TRACING
