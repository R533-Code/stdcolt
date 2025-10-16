/*****************************************************************/ /**
 * @file   debugging.cpp
 * @brief  Contains the implementation of `debugging.h`.
 * @date   October 2025
 *********************************************************************/
#include "debugging.h"
#include <stdcolt_macros/compiler.h>

#if STDCOLT_MSVC
  #include <intrin.h>
  #include <windows.h>
#elif defined(__APPLE__)
  #include <sys/types.h>
  #include <sys/sysctl.h>
#elif defined(__linux__)
  #include <cstring>
  #include <cstdio>
#endif

#include <csignal>

namespace stdcolt
{
  void breakpoint() noexcept
  {
#if STDCOLT_MSVC
    __debugbreak();
#elif defined(__has_builtin)
  #if __has_builtin(__builtin_debugtrap)
    __builtin_debugtrap();
  #elif __has_builtin(__builtin_trap)
    __builtin_trap();
  #else
    #if defined(SIGTRAP)
    std::raise(SIGTRAP);
    #else
    std::raise(SIGABRT);
    #endif
  #endif
#elif STDCOLT_GCC || STDCOLT_CLANG
  #if defined(__i386__) || defined(__x86_64__)
    __asm__ volatile("int3");
  #elif defined(SIGTRAP)
    std::raise(SIGTRAP);
  #else
    std::raise(SIGABRT);
  #endif
#else
  #if defined(SIGTRAP)
    std::raise(SIGTRAP);
  #else
    std::raise(SIGABRT);
  #endif
#endif
  }

  bool is_debugger_present() noexcept
  {
#if STDCOLT_MSVC
    return ::IsDebuggerPresent() != 0;
#elif defined(__APPLE__)
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, 0};
    mib[3]     = getpid();
    struct kinfo_proc info{};
    size_t size = sizeof(info);
    if (sysctl(mib, 4, &info, &size, nullptr, 0) == 0 && size == sizeof(info))
      return (info.kp_proc.p_flag & P_TRACED) != 0;
    return false;
#elif defined(__linux__)
    FILE* f = std::fopen("/proc/self/status", "r");
    if (!f)
      return false;
    char line[256];
    bool attached = false;
    while (std::fgets(line, sizeof(line), f))
    {
      if (std::strncmp(line, "TracerPid:", 10) == 0)
      {
        int pid = 0;
        std::sscanf(line + 10, "%d", &pid);
        attached = (pid != 0);
        break;
      }
    }
    std::fclose(f);
    return attached;
#else
    return false; // fallback
#endif
  }
} // namespace stdcolt
