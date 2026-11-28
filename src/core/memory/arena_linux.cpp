//
// Created by Dwayne Edwards on 2026-11-27.
//

#include "onyx/platform/platform.hpp"

#if ONYX_OS_LINUX

  #include "onyx/core/memory/arena.hpp"

  #include <sys/mman.h>
  #include <unistd.h>

static_assert(onyx::OS == onyx::Platform::LINUX, "arena_linux.cpp can only be compiled on Linux.");

namespace onyx
{
  usize
  memory_pagesize() noexcept
  {
    return static_cast<usize>(sysconf(_SC_PAGESIZE));
  }

  void*
  memory_reserve(usize total_bytes, usize usable_bytes) noexcept
  {
    void* ptr = mmap(nullptr, total_bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (ptr == MAP_FAILED) return nullptr;
  #if MADV_HUGEPAGE
    madvise(ptr, usable_bytes, MADV_HUGEPAGE);
  #endif
    return ptr;
  }

  bool
  memory_commit(void* addr, usize bytes) noexcept
  {
    return mprotect(addr, bytes, PROT_READ | PROT_WRITE) == 0;
  }

  bool
  memory_decommit(void* addr, usize bytes) noexcept
  {
    madvise(addr, bytes, MADV_DONTNEED);
    return mprotect(addr, bytes, PROT_NONE) == 0;
  }

  void
  memory_release(void* base, usize total_bytes) noexcept
  {
    if (base) munmap(base, total_bytes);
  }

} // namespace onyx

#endif // ONYX_OS_LINUX
