/*****************************************************************/ /**
 * @file   allocator.cpp
 * @brief  Contains the implementation of `allocator.h`.
 * 
 * @author Raphael Dib Nehme
 * @date   November 2025
 *********************************************************************/
#include "allocator.h"
#include <atomic>
#include <cstdio>
#include <cinttypes>

namespace stdcolt::alloc
{
  /// @brief Function to call on allocation failure, must not be null.
  static std::atomic<alloc_fail_fn_t> ALLOC_FAIL_HOOK = &default_on_alloc_fail;

  alloc_fail_fn_t register_on_alloc_fail(alloc_fail_fn_t fn) noexcept
  {
    STDCOLT_pre(fn != nullptr, "expected non-null hook");
    return ALLOC_FAIL_HOOK.exchange(fn, std::memory_order_relaxed);
  }

  void default_on_alloc_fail(size_t size, const std::source_location& loc) noexcept
  {
    std::fprintf(
        stderr,
        "FATAL ERROR: Allocation failure of size %" PRIu64 "\n"
        "             from %s:%" PRIu64 "\n"
        "             in function `%s`.",
        (uint64_t)size, loc.file_name(), (uint64_t)loc.line(), loc.function_name());
    std::abort();
  }

  void handle_alloc_fail(size_t size, const std::source_location& loc) noexcept
  {
    if (auto fn = ALLOC_FAIL_HOOK.load(std::memory_order_relaxed))
      fn(size, loc);
    std::abort();
  }
} // namespace stdcolt::alloc
