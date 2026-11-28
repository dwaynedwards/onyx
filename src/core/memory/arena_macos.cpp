//
// Created by Dwayne Edwards on 2026-11-27.
//

#include "onyx/platform/platform.hpp"

#if ONYX_OS_MACOS

  #include "onyx/core/memory/arena.hpp"

  #include <mach/vm_statistics.h>
  #include <sys/mman.h>
  #include <unistd.h>

static_assert(onyx::OS == onyx::Platform::MACOS, "arena_macos.cpp can only be compiled on macOS.");

namespace onyx
{
  usize
  memory_pagesize() noexcept
  {
    return static_cast<usize>(sysconf(_SC_PAGESIZE));
  }

  void*
  memory_reserve(usize total_bytes, [[maybe_unused]] usize usable_bytes) noexcept
  {
    // VM_MAKE_TAG(240) tags allocations in macOS `vmmap` / Instruments
    void* ptr = mmap(nullptr, total_bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, VM_MAKE_TAG(240), 0);
    return (ptr == MAP_FAILED) ? nullptr : ptr;
  }

  bool
  memory_commit(void* addr, usize bytes) noexcept
  {
    return mprotect(addr, bytes, PROT_READ | PROT_WRITE) == 0;
  }

  bool
  memory_decommit(void* addr, usize bytes) noexcept
  {
    madvise(addr, bytes, MADV_FREE);
    return mprotect(addr, bytes, PROT_NONE) == 0;
  }

  void
  memory_release(void* base, usize total_bytes) noexcept
  {
    if (base) munmap(base, total_bytes);
  }

} // namespace onyx

#endif // ONYX_OS_MACOS
