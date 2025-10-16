/*****************************************************************/ /**
 * @file   debugging.h
 * @brief  Contains programmatic debugging utilities.
 * @date   October 2025
 *********************************************************************/
#ifndef __HG_STDCOLT_DEBUGGING_DEBUGGING
#define __HG_STDCOLT_DEBUGGING_DEBUGGING

#include <stdcolt_debugging_export.h>

namespace stdcolt
{
  STDCOLT_DEBUGGING_EXPORT
  /// @brief Attempts to pass control to the debugger
  void breakpoint() noexcept;

  STDCOLT_DEBUGGING_EXPORT
  /// @brief Checks whether the current process is running under the control of a debugger
  /// @return True if under the control of a debugger
  bool is_debugger_present() noexcept;

  /// @brief Pass control to the debugger only if running under one
  void breakpoint_if_debugging() noexcept
  {
    if (is_debugger_present())
      breakpoint();
  }
} // namespace stdcolt

#endif // !__HG_STDCOLT_DEBUGGING_DEBUGGING
