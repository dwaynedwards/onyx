//
// Created by Dwayne Edwards on 2026-11-27.
//

#pragma once

#include "onyx/platform/platform.hpp"
#include "onyx/platform/types.hpp"

#include <atomic>
#include <cassert>
#include <cstring>
#include <initializer_list>
#include <memory_resource>
#include <new>
#include <type_traits>


// AddressSanitizer manual memory poisoning (must be included at global scope)
#if defined(__SANITIZE_ADDRESS__)
  #include <sanitizer/asan_interface.h>
  #define ARENA_POISON_MEMORY(addr, size)   __asan_poison_memory_region((addr), (size))
  #define ARENA_UNPOISON_MEMORY(addr, size) __asan_unpoison_memory_region((addr), (size))
  #define ARENA_IS_POISONED(addr)           (__asan_address_is_poisoned((addr)) != 0)
#elif defined(__has_feature)
  #if __has_feature(address_sanitizer)
    #include <sanitizer/asan_interface.h>
    #define ARENA_POISON_MEMORY(addr, size)   __asan_poison_memory_region((addr), (size))
    #define ARENA_UNPOISON_MEMORY(addr, size) __asan_unpoison_memory_region((addr), (size))
    #define ARENA_IS_POISONED(addr)           (__asan_address_is_poisoned((addr)) != 0)
  #else
    #define ARENA_POISON_MEMORY(addr, size)   ((void)0)
    #define ARENA_UNPOISON_MEMORY(addr, size) ((void)0)
    #define ARENA_IS_POISONED(addr)           (false)
  #endif
#else
  #define ARENA_POISON_MEMORY(addr, size)   ((void)0)
  #define ARENA_UNPOISON_MEMORY(addr, size) ((void)0)
  #define ARENA_IS_POISONED(addr)           (false)
#endif


#define ARENA_CONCAT_IMPL(a, b) a##b
#define ARENA_CONCAT(a, b)      ARENA_CONCAT_IMPL(a, b)

namespace onyx
{
  [[nodiscard]] extern usize
  memory_pagesize() noexcept;
  [[nodiscard]] extern void*
  memory_reserve(usize total_bytes, usize usable_bytes) noexcept;
  [[nodiscard]] extern bool
  memory_commit(void* addr, usize bytes) noexcept;
  [[nodiscard]] extern bool
  memory_decommit(void* addr, usize bytes) noexcept;
  extern void
  memory_release(void* base, usize total_bytes) noexcept;

  class ArenaResource;

  class Arena
  {
  public:
    static constexpr usize DEFAULT_RESERVE = 256ULL * 1024 * 1024;
    static constexpr usize DEFAULT_STEP    = 2ULL * 1024 * 1024;

    [[nodiscard]] static constexpr Arena
    empty() noexcept
    {
      return Arena{};
    }

    [[nodiscard]] static Arena
    create(usize reserve_size = DEFAULT_RESERVE, usize initial_commit = 0, usize step = DEFAULT_STEP,
           bool prefault = true) noexcept;

    [[nodiscard]] ONYX_ALWAYS_INLINE void*
    alloc(usize bytes, usize align = alignof(std::max_align_t)) noexcept
    {
      const uptr pre_commit = align_up(offset_, align);
      if (pre_commit > committed_end_ || bytes > committed_end_ - pre_commit) [[unlikely]]
      {
        if (bytes > UINTPTR_MAX - pre_commit) [[unlikely]]
        {
          return nullptr; // Arithmetic overflow
        }
        return commit_and_allocate(pre_commit, pre_commit + bytes);
      }

      offset_   = pre_commit + bytes;
      void* ptr = reinterpret_cast<void*>(pre_commit);
      ARENA_UNPOISON_MEMORY(ptr, bytes);
      return ptr;
    }

    template <typename T>
    [[nodiscard]] ONYX_ALWAYS_INLINE T*
    alloc(usize count = 1) noexcept
    {
      if (count > SIZE_MAX / sizeof(T)) [[unlikely]]
      {
        return nullptr;
      }
      return static_cast<T*>(alloc(sizeof(T) * count, alignof(T)));
    }

    [[nodiscard]] ONYX_ALWAYS_INLINE void*
    alloc_zeroed(usize bytes, usize align = alignof(std::max_align_t)) noexcept
    {
      void* p = alloc(bytes, align);
      if (p)
      {
        std::memset(p, 0, bytes);
      }
      return p;
    }

    template <typename T>
    [[nodiscard]] ONYX_ALWAYS_INLINE T*
    alloc_zeroed(usize count = 1) noexcept
    {
      static_assert(std::is_trivially_copyable_v<T>);
      if (count > SIZE_MAX / sizeof(T)) [[unlikely]]
      {
        return nullptr;
      }
      return static_cast<T*>(alloc_zeroed(sizeof(T) * count, alignof(T)));
    }

    void
    reset() noexcept
    {
      if (!base_)
      {
        return;
      }
      const uptr start = reinterpret_cast<uptr>(base_) + sizeof(Header);
      if (offset_ > start)
      {
        ARENA_POISON_MEMORY(reinterpret_cast<void*>(start), static_cast<usize>(offset_ - start));
      }
      offset_ = start;
    }

    void
    reset_zeroed() noexcept
    {
      if (!base_)
      {
        return;
      }
      const uptr start = reinterpret_cast<uptr>(base_) + sizeof(Header);
      if (offset_ > start)
      {
        std::memset(reinterpret_cast<void*>(start), 0, offset_ - start);
        ARENA_POISON_MEMORY(reinterpret_cast<void*>(start), static_cast<usize>(offset_ - start));
      }
      offset_ = start;
    }

    [[nodiscard]] uptr
    save() const noexcept
    {
      return offset_;
    }

    void
    restore(uptr saved_offset) noexcept
    {
      if (!base_)
      {
        return;
      }
      assert(saved_offset >= reinterpret_cast<uptr>(base_) + sizeof(Header) && saved_offset <= offset_);
      if (offset_ > saved_offset)
      {
        ARENA_POISON_MEMORY(reinterpret_cast<void*>(saved_offset), static_cast<usize>(offset_ - saved_offset));
      }
      offset_ = saved_offset;
    }

    [[nodiscard]] bool
    is_valid() const noexcept
    {
      return base_ != nullptr;
    }

    [[nodiscard]] bool
    shares_buffer_with(const Arena& other) const noexcept
    {
      return base_ != nullptr && base_ == other.base_;
    }

    [[nodiscard]] usize
    user_bytes_used() const noexcept
    {
      return base_ ? (offset_ - (reinterpret_cast<uptr>(base_) + sizeof(Header))) : 0;
    }

    [[nodiscard]] usize
    total_bytes_used() const noexcept
    {
      return base_ ? (offset_ - reinterpret_cast<uptr>(base_)) : 0;
    }

    [[nodiscard]] usize
    bytes_committed() const noexcept
    {
      return header_ ? header_->committed.load(std::memory_order_relaxed) : 0;
    }

    [[nodiscard]] ArenaResource
    pmr() noexcept;

    void
    decommit() noexcept;
    void
    release() noexcept;

  private:
    Arena() noexcept = default;

    struct alignas(64) Header
    {
      std::atomic<usize> committed;
      usize              page_size;
      bool               prefault;
    };

    std::byte* base_          = nullptr;
    uptr       offset_        = 0;
    uptr       committed_end_ = 0;
    uptr       reserved_end_  = 0;
    usize      commit_step_   = 0;
    Header*    header_        = nullptr;

    static constexpr uptr
    align_up(uptr offset, usize alignment) noexcept
    {
      assert((alignment & (alignment - 1)) == 0 && "Alignment must be a power of two");
      return (offset + alignment - 1) & ~(alignment - 1);
    }

    ONYX_NOINLINE void*
    commit_and_allocate(uptr pre_commit, uptr post_commit) noexcept;
  };

  class ArenaResource final : public std::pmr::memory_resource
  {
  public:
    explicit ArenaResource(Arena* arena) noexcept : arena_(arena) {}

  private:
    Arena* arena_ = nullptr;

    [[nodiscard]]
    void*
    do_allocate(usize bytes, usize alignment) override
    {
      void* p = arena_ ? arena_->alloc(bytes, alignment) : nullptr;
      if (!p)
      {
        throw std::bad_alloc();
      }
      return p;
    }

    void
    do_deallocate(void* /*p*/, usize /*bytes*/, usize /*alignment*/) noexcept override
    {
    }

    [[nodiscard]] bool
    do_is_equal(const std::pmr::memory_resource& other) const noexcept override
    {
      if (this == &other)
      {
        return true;
      }
      const auto* o = dynamic_cast<const ArenaResource*>(&other);
      return o && o->arena_ == arena_;
    }
  };

  class ScratchArena
  {
  public:
    explicit ScratchArena(Arena* arena) noexcept : arena_(arena), saved_offset_(arena ? arena->save() : 0) {}

    ~ScratchArena() noexcept
    {
      if (arena_) arena_->restore(saved_offset_);
    }

    ScratchArena(const ScratchArena&) = delete;
    ScratchArena&
    operator=(const ScratchArena&) = delete;
    ScratchArena(ScratchArena&&)   = delete;
    ScratchArena&
    operator=(ScratchArena&&) = delete;

    [[nodiscard]] Arena*
    get() const noexcept
    {
      return arena_;
    }

    explicit
    operator Arena*() const noexcept
    {
      return arena_;
    }

    Arena*
    operator->() const noexcept
    {
      return arena_;
    }

  private:
    Arena* arena_        = nullptr;
    uptr   saved_offset_ = 0;

    static Arena*
    thread_scratch_pool() noexcept
    {
      static thread_local Arena pool[2] = {Arena::empty(), Arena::empty()};
      return pool;
    }

    friend ScratchArena arena_get_scratch(std::initializer_list<const Arena*>) noexcept;
    friend void
    arena_decommit_thread_scratch() noexcept;
    friend void
    arena_release_thread_scratch() noexcept;
  };

  [[nodiscard]] inline ScratchArena
  arena_get_scratch(std::initializer_list<const Arena*> conflicts = {}) noexcept
  {
    Arena* pool = ScratchArena::thread_scratch_pool();
    if (!pool[0].is_valid())
    {
      pool[0] = Arena::create();
      pool[1] = Arena::create();
    }

    for (usize i = 0; i < 2; ++i)
    {
      bool collides = false;
      for (const Arena* c: conflicts)
      {
        if (c != nullptr && pool[i].shares_buffer_with(*c))
        {
          collides = true;
          break;
        }
      }
      if (!collides)
      {
        return ScratchArena(&pool[i]);
      }
    }
    return ScratchArena(&pool[0]);
  }

  inline void
  arena_decommit_thread_scratch() noexcept
  {
    Arena* pool = ScratchArena::thread_scratch_pool();
    for (usize i = 0; i < 2; ++i)
    {
      if (pool[i].is_valid()) pool[i].decommit();
    }
  }

  inline void
  arena_release_thread_scratch() noexcept
  {
    Arena* pool = ScratchArena::thread_scratch_pool();
    for (usize i = 0; i < 2; ++i)
    {
      if (pool[i].is_valid()) pool[i].release();
    }
  }

} // namespace onyx

#define ARENA_SCRATCH_SCOPED(arena_ptr) onyx::ScratchArena ARENA_CONCAT(_sc_, __LINE__)(arena_ptr)

#define ARENA_GET_SCRATCH(var_name, ...)                                                                               \
  onyx::ScratchArena ARENA_CONCAT(_gsc_, __LINE__) = onyx::arena_get_scratch({__VA_ARGS__});                           \
  onyx::Arena*       var_name                      = ARENA_CONCAT(_gsc_, __LINE__).get()
