//
// Created by Dwayne Edwards on 2026-11-27.
//

#include "onyx/core/memory/arena.hpp"

#include "onyx/platform/types.hpp"


namespace onyx
{
  namespace
  {
    void
    prefault_range(byte* start, usize bytes, usize page_size) noexcept
    {
      volatile byte* p = start;
      for (usize i = 0; i < bytes; i += page_size)
      {
        p[i] = std::byte{0};
      }
    }
  } // namespace

  Arena
  Arena::create(usize reserve_size, usize initial_commit, usize step, bool prefault) noexcept
  {
    if (reserve_size == 0) reserve_size = DEFAULT_RESERVE;
    if (step == 0) step = DEFAULT_STEP;

    const usize page_size       = memory_pagesize();
    const usize aligned_reserve = static_cast<usize>(align_up(reserve_size, page_size));
    const usize aligned_step    = static_cast<usize>(align_up(step, page_size));

    initial_commit = (initial_commit == 0) ? aligned_step : static_cast<usize>(align_up(initial_commit, page_size));
    if (initial_commit < sizeof(Header)) initial_commit = static_cast<usize>(align_up(sizeof(Header), page_size));
    if (initial_commit > aligned_reserve) initial_commit = aligned_reserve;

    // Reserve an extra trailing guard page (remains PROT_NONE / PAGE_NOACCESS)
    void* ptr = memory_reserve(aligned_reserve + page_size, aligned_reserve);
    if (!ptr) return Arena{};
    if (!memory_commit(ptr, initial_commit))
    {
      memory_release(ptr, aligned_reserve + page_size);
      return Arena{};
    }

    auto* base = static_cast<byte*>(ptr);
    if (prefault) prefault_range(base, initial_commit, page_size);

    auto* header = new (ptr) Header{};
    header->committed.store(initial_commit, std::memory_order_relaxed);
    header->page_size = page_size;
    header->prefault  = prefault;

    const uptr base_addr = reinterpret_cast<uptr>(base);

    Arena arena{};
    arena.base_          = base;
    arena.offset_        = base_addr + sizeof(Header);
    arena.committed_end_ = base_addr + initial_commit;
    arena.reserved_end_  = base_addr + aligned_reserve;
    arena.commit_step_   = aligned_step;
    arena.header_        = header;

    if (arena.committed_end_ > arena.offset_)
    {
      ARENA_POISON_MEMORY(reinterpret_cast<void*>(arena.offset_),
                          static_cast<usize>(arena.committed_end_ - arena.offset_));
    }

    return arena;
  }

  ArenaResource
  Arena::pmr() noexcept
  {
    return ArenaResource(this);
  }

  void
  Arena::decommit() noexcept
  {
    if (!base_ || !header_) return;

    const usize page_size      = header_->page_size;
    const usize keep_committed = static_cast<usize>(align_up(sizeof(Header), page_size));
    const usize cur_com        = header_->committed.load(std::memory_order_relaxed);

    if (cur_com > keep_committed)
    {
      ARENA_UNPOISON_MEMORY(base_ + keep_committed, cur_com - keep_committed);
      if (memory_decommit(base_ + keep_committed, cur_com - keep_committed))
      {
        header_->committed.store(keep_committed, std::memory_order_release);
      }
    }

    const uptr base_addr = reinterpret_cast<uptr>(base_);
    const uptr start     = base_addr + sizeof(Header);
    if (offset_ > start)
    {
      ARENA_POISON_MEMORY(reinterpret_cast<void*>(start), static_cast<usize>(offset_ - start));
    }

    offset_        = start;
    committed_end_ = base_addr + keep_committed;
  }

  void
  Arena::release() noexcept
  {
    if (!base_) return;
    const usize total    = static_cast<usize>(reserved_end_ - reinterpret_cast<uptr>(base_)) + header_->page_size;
    byte*       base_ptr = base_;

    ARENA_UNPOISON_MEMORY(base_ptr, total);
    *this = Arena{};
    memory_release(base_ptr, total);
  }

  void*
  Arena::commit_and_allocate(uptr pre_commit, uptr post_commit) noexcept
  {
    if (!base_ || post_commit < pre_commit || post_commit > reserved_end_) [[unlikely]]
      return nullptr;

    const uptr base_addr = reinterpret_cast<uptr>(base_);
    usize      shared    = header_->committed.load(std::memory_order_acquire);

    if (post_commit > base_addr + shared)
    {
      auto new_commit = static_cast<usize>(align_up(static_cast<usize>(post_commit - base_addr), commit_step_));
      if (const auto max_res = static_cast<usize>(reserved_end_ - base_addr); new_commit > max_res)
      {
        new_commit = max_res;
      }

      while (shared < new_commit)
      {
        if (!memory_commit(base_ + shared, new_commit - shared)) return nullptr;
        const usize prev = shared;
        if (header_->committed.compare_exchange_weak(shared, new_commit, std::memory_order_release,
                                                     std::memory_order_acquire))
        {
          if (header_->prefault)
          {
            prefault_range(base_ + prev, new_commit - prev, header_->page_size);
          }
          ARENA_POISON_MEMORY(base_ + prev, new_commit - prev);
          shared = new_commit;
          break;
        }
      }
    }

    committed_end_ = base_addr + shared;
    offset_        = post_commit;

    void* ptr = reinterpret_cast<void*>(pre_commit);
    ARENA_UNPOISON_MEMORY(ptr, static_cast<usize>(post_commit - pre_commit));
    return ptr;
  }


} // namespace onyx
