/*****************************************************************/ /**
 * @file   contracts.cpp
 * @brief  Implementation of `constracts.h`
 * 
 * @author Raphael Dib Nehme
 * @date   Oct 2025
 *********************************************************************/
#include <atomic>
#include <cstdio>
#include <cpptrace/cpptrace.hpp>

#include <stdcolt_debugging/debugging.h>
#include "contracts.h"

namespace stdcolt::contracts
{
  static std::atomic<violation_handler_fn_t*> global_handler = nullptr;

  void unreachable(const std::source_location& loc) noexcept
  {
    violation_handler(
        "stdcolt::contracts::unreachable()", "An unreachable branch was hit.",
        Kind::Assert, loc);
    std::abort();
  }

  void default_runtime_violation_handler(
      const char* expr, const char* explanation, Kind kind,
      const std::optional<std::source_location>& loc_opt) noexcept
  {
    using enum Kind;

    const char* kind_str = "assertion";
    if (kind == Pre)
      kind_str = "precondition";
    else if (kind == Post)
      kind_str = "postcondition";

    bool with_color = false;
    std::string str = {};
    try
    {
      with_color = cpptrace::isatty(cpptrace::stderr_fileno);
      // if allocation fails, forget about the trace
      str = cpptrace::generate_trace(3).to_string(with_color);
      // ^^^ skip 3 traces:
      // `default_runtime_violation_handler`
      // `runtime_violation_handler`
      // `violation_handler`
    }
    catch (...)
    {
      with_color = false;
      // do nothing
    }
    if (with_color)
    {
      if (loc_opt.has_value())
      {
        auto& loc = *loc_opt;
        std::fprintf(
            stderr,
            "\x1b[41mFATAL ERROR:\x1b[0m\n  in "
            "\x1b[32m%s\x1b[0m:\x1b[34m%u\x1b[0m\n  "
            "in \x1b[33m%s\x1b[0m\n  \x1b[95m%s\x1b[0m: \x1b[96m%s\x1b[0m\n  "
            "\x1b[95mexplanation\x1b[0m: %s\n",
            loc.file_name(), static_cast<unsigned>(loc.line()), loc.function_name(),
            kind_str, expr, explanation);
      }
      else
      {
        std::fprintf(
            stderr,
            "\x1b[41mFATAL ERROR:\x1b[0m\n  \x1b[95m%s\x1b[0m: \x1b[96m%s\x1b[0m\n  "
            "\x1b[95mexplanation\x1b[0m: %s\n",
            kind_str, expr, explanation);
      }
    }
    else
    {
      if (loc_opt.has_value())
      {
        auto& loc = *loc_opt;
        std::fprintf(
            stderr, "FATAL ERROR:\n  %s:%u:%u: in %s\n  %s: %s\n  explanation: %s\n",
            loc.file_name(), static_cast<unsigned>(loc.line()),
            static_cast<unsigned>(loc.column()), loc.function_name(), kind_str, expr,
            explanation);
      }
      else
      {
        std::fprintf(
            stderr, "FATAL ERROR:\n  %s: %s\n  explanation: %s\n", kind_str, expr,
            explanation);
      }
    }
    if (!str.empty())
      std::fprintf(stderr, "\n  %s", str.c_str());

    std::fflush(stderr);
    stdcolt::breakpoint_if_debugging();
    std::abort();
  }

  void runtime_violation_handler(
      const char* expr, const char* explanation, Kind kind,
      const std::optional<std::source_location>& loc) noexcept
  {
    auto handler = global_handler.load(std::memory_order_relaxed);
    if (handler)
      handler(expr, explanation, kind, loc);
    else
      default_runtime_violation_handler(expr, explanation, kind, loc);
    // UB if the runtime violation handler returns.
  }

  void register_violation_handler(violation_handler_fn_t* fn) noexcept
  {
    if (fn != nullptr)
      global_handler.exchange(fn, std::memory_order_relaxed);
  }
} // namespace stdcolt::contracts
