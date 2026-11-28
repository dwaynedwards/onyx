//
// Created by Dwayne Edwards on 2026-11-27.
//

#include "onyx/platform/platform.hpp"

#if ONYX_OS_WINDOWS

  #include "onyx/core/memory/arena.hpp"

  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>

static_assert(onyx::OS == onyx::Platform::WINDOWS, "arena_win32.cpp can only be compiled on Windows.");

namespace onyx
{
  usize
  memory_pagesize() noexcept
  {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return static_cast<usize>(info.dwPageSize);
  }

  void*
  memory_reserve(usize total_bytes, [[maybe_unused]] usize usable_bytes) noexcept
  {
    return VirtualAlloc(nullptr, total_bytes, MEM_RESERVE, PAGE_NOACCESS);
  }

  bool
  memory_commit(void* addr, usize bytes) noexcept
  {
    return VirtualAlloc(addr, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr;
  }

  bool
  memory_decommit(void* addr, usize bytes) noexcept
  {
    return VirtualFree(addr, bytes, MEM_DECOMMIT) != 0;
  }

  void
  memory_release(void* base, [[maybe_unused]] usize total_bytes) noexcept
  {
    if (base) VirtualFree(base, 0, MEM_RELEASE);
  }

} // namespace onyx

#endif // ONYX_OS_WINDOWS
