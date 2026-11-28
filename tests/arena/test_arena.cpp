//
// Created by Dwayne Edwards on 2026-11-27.
//

#include "onyx/core/memory/arena.hpp"
#include "onyx/platform/platform.hpp"
#include "onyx/platform/types.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace onyx;

// =========================================================
// Compile-Time Platform & Type Guarantees
// =========================================================
static_assert(COMPILER != Compiler::UNKNOWN);
static_assert(OS != Platform::UNKNOWN);
static_assert(ARCH != Arch::UNKNOWN);

// =========================================================
// Arena Unit Tests
// =========================================================
TEST_CASE("Arena allocation, alignment, and multi-page commit", "[arena]")
{
  const usize page  = memory_pagesize();
  Arena       arena = Arena::create(page * 16, page, page, true);

  REQUIRE(arena.is_valid());
  REQUIRE(arena.bytes_committed() == page);

  SECTION("Alignment is respected across mixed alloc types")
  {
    auto* a = arena.alloc<u8>(1);
    auto* b = arena.alloc<u64>(1);

    REQUIRE(reinterpret_cast<uptr>(b) % alignof(u64) == 0);
    *a = 0xAB;
    *b = 0x123456789ABCDEF0ULL;
    CHECK(*a == 0xAB);
    CHECK(*b == 0x123456789ABCDEF0ULL);
  }

  SECTION("Growing past initial commit commits additional pages")
  {
    const usize big_count = (page * 4) / sizeof(u32);
    u32*        big_arr   = arena.alloc_zeroed<u32>(big_count);

    REQUIRE(big_arr != nullptr);
    CHECK(big_arr[0] == 0);
    CHECK(big_arr[big_count - 1] == 0);
    CHECK(arena.bytes_committed() >= page * 5);

    arena.decommit();
    CHECK(arena.bytes_committed() == page);
    CHECK(arena.user_bytes_used() == 0);
  }

  arena.release();
  REQUIRE_FALSE(arena.is_valid());
}

TEST_CASE("Arena tracks user and total byte usage accurately", "[arena]")
{
  Arena arena = Arena::create();
  REQUIRE(arena.user_bytes_used() == 0);
  REQUIRE(arena.total_bytes_used() > 0); // Includes internal Header metadata

  const usize header_overhead = arena.total_bytes_used();

  auto* a = arena.alloc<u64>(2); // 16 bytes
  (void)a;
  CHECK(arena.user_bytes_used() == 16);
  CHECK(arena.total_bytes_used() == header_overhead + 16);

  {
    ARENA_SCRATCH_SCOPED(&arena);
    auto* temp = arena.alloc<u8>(32);
    (void)temp;
    CHECK(arena.user_bytes_used() == 48);
  }

  CHECK(arena.user_bytes_used() == 16);

  arena.reset();
  CHECK(arena.user_bytes_used() == 0);
  CHECK(arena.total_bytes_used() == header_overhead);

  arena.release();
  CHECK(arena.user_bytes_used() == 0);
  CHECK(arena.total_bytes_used() == 0);
}

TEST_CASE("Arena value-copy sub-arena leaves parent cursor intact", "[arena]")
{
  Arena arena             = Arena::create();
  auto* persistent        = arena.alloc<u64>(1);
  *persistent             = 42;
  const usize used_before = arena.user_bytes_used();

  {
    Arena scratch = arena;
    REQUIRE(scratch.shares_buffer_with(arena));

    auto* temp = scratch.alloc<u64>(1024);
    temp[0]    = 999;
    CHECK(scratch.user_bytes_used() > used_before);
  }

  CHECK(arena.user_bytes_used() == used_before);
  CHECK(*persistent == 42);

  arena.release();
}

TEST_CASE("Arena::try_extend grows tip allocations in-place and rejects blocked ones", "[arena]")
{
  {
    ARENA_GET_SCRATCH(scratch);

    auto* buf = scratch->alloc<u64>(4); // 32 bytes
    REQUIRE(buf != nullptr);
    buf[0] = 10;
    buf[3] = 40;
    CHECK(scratch->user_bytes_used() == 32);

    // Extending while at the very tip of the arena succeeds in-place
    REQUIRE(scratch->try_extend(buf, 4 * sizeof(u64), 8 * sizeof(u64)));
    CHECK(scratch->user_bytes_used() == 64);
    buf[7] = 80;
    CHECK(buf[0] == 10);
    CHECK(buf[7] == 80);

    // Shrinking or equal size returns true without modifying offset
    CHECK(scratch->try_extend(buf, 8 * sizeof(u64), 8 * sizeof(u64)));
    CHECK(scratch->user_bytes_used() == 64);

    // Null pointer returns false
    CHECK_FALSE(scratch->try_extend(nullptr, 0, 16));

    // Allocate a blocker at the tip so `buf` is no longer the last allocation
    auto* blocker = scratch->alloc<u32>(1);
    *blocker      = 0xABCD1234U;

    // Extending a non-tip allocation returns false without touching `blocker`
    CHECK_FALSE(scratch->try_extend(buf, 8 * sizeof(u64), 16 * sizeof(u64)));
    CHECK(*blocker == 0xABCD1234U);
  }

  arena_release_thread_scratch();
}

TEST_CASE("Arena rejects integer overflow and uninitialized allocations safely", "[arena][overflow]")
{
  SECTION("Empty uninitialized arena returns nullptr without crashing")
  {
    Arena empty = Arena::empty();
    REQUIRE_FALSE(empty.is_valid());
    CHECK(empty.alloc(64) == nullptr);
    CHECK(empty.alloc<u64>(4) == nullptr);
    CHECK(empty.user_bytes_used() == 0);
    CHECK(empty.total_bytes_used() == 0);
  }

  SECTION("Byte and element count overflow return nullptr without wrapping")
  {
    Arena arena             = Arena::create();
    auto* valid_first       = arena.alloc<u64>(1);
    *valid_first            = 0xCAFEBABEULL;
    const usize used_before = arena.user_bytes_used();

    CHECK(arena.alloc(SIZE_MAX) == nullptr);
    CHECK(arena.alloc(UINTPTR_MAX - 8) == nullptr);
    CHECK(arena.alloc<u64>(SIZE_MAX) == nullptr);
    CHECK(arena.alloc<u64>((SIZE_MAX / sizeof(u64)) + 1) == nullptr);
    CHECK(arena.alloc_zeroed<u64>((SIZE_MAX / sizeof(u64)) + 1) == nullptr);

    CHECK(arena.user_bytes_used() == used_before);
    CHECK(*valid_first == 0xCAFEBABEULL);

    arena.release();
  }
}

TEST_CASE("ArenaScratch conflict avoidance", "[arena][scratch]")
{
  {
    ARENA_GET_SCRATCH(s1);
    REQUIRE(s1 != nullptr);
    REQUIRE(s1->is_valid());

    auto* p1 = s1->alloc<u32>(16);
    p1[0]    = 77;

    {
      ARENA_GET_SCRATCH(s2, s1);
      REQUIRE(s2 != nullptr);
      REQUIRE(s2->is_valid());
      REQUIRE_FALSE(s2->shares_buffer_with(*s1));

      auto* p2 = s2->alloc<u32>(16);
      p2[0]    = 88;
      CHECK(p1[0] == 77);
    }
  }

  arena_decommit_thread_scratch();
  arena_release_thread_scratch();
}

TEST_CASE("ArenaResource works with std::pmr containers", "[arena][pmr]")
{
  {
    ARENA_GET_SCRATCH(scratch);
    ArenaResource res = scratch->pmr();

    std::pmr::vector<u32> vec(&res);
    for (u32 i = 0; i < 500; ++i)
    {
      vec.push_back(i * 2);
    }

    REQUIRE(vec.size() == 500);
    CHECK(vec[250] == 500);
    CHECK(scratch->user_bytes_used() >= 500 * sizeof(u32));
  }

  arena_release_thread_scratch();
}

// =========================================================
// ASan Manual Poisoning Verification
// =========================================================
#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer))
TEST_CASE("ASan poisons memory on restore and unpoisons on alloc", "[arena][asan]")
{
  {
    ARENA_GET_SCRATCH(scratch);
    int* ptr = nullptr;

    {
      ARENA_SCRATCH_SCOPED(scratch);
      ptr = scratch->alloc<int>(1);
      REQUIRE_FALSE(ARENA_IS_POISONED(ptr));
      *ptr = 123;
    } // restore() re-poisons `ptr` here

    REQUIRE(ARENA_IS_POISONED(ptr));
  }

  arena_release_thread_scratch();
}
#endif
