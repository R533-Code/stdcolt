/*****************************************************************/ /**
 * @file   tracing.cpp
 * @brief  Contains the implementation of `tracing.h`
 * 
 * @author Raphael Dib Nehme
 * @date   December 2025
 *********************************************************************/
#include "tracing.h"
#include <thread>

namespace stdcolt
{
  bool wait_for_tracer(std::chrono::milliseconds timeout) noexcept
  {
    STDCOLT_TRACE_FN_C(0x040404);
#ifndef STDCOLT_ENABLE_TRACING
    return false;
#else
    using namespace std::chrono_literals;

    auto start = std::chrono::steady_clock::now();
    while (true)
    {
      if (tracy::GetProfiler().IsConnected())
        return true;

      if (std::chrono::steady_clock::now() - start >= timeout)
        return false;

      std::this_thread::sleep_for(15ms);
    }
#endif
  }

  void shutdown_tracer() noexcept
  {
#ifdef STDCOLT_ENABLE_TRACING
    using namespace std::chrono_literals;

    if (!tracy::GetProfiler().IsConnected())
      return;
    tracy::GetProfiler().RequestShutdown();
    while (!tracy::GetProfiler().HasShutdownFinished())
      std::this_thread::sleep_for(15ms);
#endif
  }
} // namespace stdcolt
