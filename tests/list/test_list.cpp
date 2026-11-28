//
// Created by Dwayne Edwards on 2026-11-27.
//

#include "onyx/core/containers/list.hpp"
#include "onyx/core/memory/arena.hpp"
#include "onyx/core/string.hpp"
#include "onyx/platform/types.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace onyx;

TEST_CASE("List push, pop, indexing, span, and range iteration", "[list]")
{
  {
    ARENA_GET_SCRATCH(scratch);
    List<u32> list{};
    REQUIRE(list.is_empty());

    for (u32 i = 0; i < 10; ++i)
    {
      REQUIRE(list.push(scratch, i * 10));
    }

    REQUIRE(list.len == 10);
    REQUIRE(list.cap >= 10);
    CHECK(list[0] == 0);
    CHECK(list[9] == 90);
    CHECK(list.span().size() == 10);

    u32 sum = 0;
    for (u32 val: list)
    {
      sum += val;
    }
    CHECK(sum == 450);

    CHECK(list.pop() == 90);
    CHECK(list.len == 9);

    list.clear();
    CHECK(list.is_empty());
  }

  arena_release_thread_scratch();
}

TEST_CASE("List extends in-place when at the top of an Arena and relocates when blocked", "[list][arena]")
{
  {
    ARENA_GET_SCRATCH(scratch);
    List<u64> list{};

    REQUIRE(list.reserve(scratch, 8));
    const u64* initial_ptr = list.data;
    CHECK(scratch->user_bytes_used() == 8 * sizeof(u64));

    // Tip growth: extends in-place with zero relocation or wasted slack bytes
    REQUIRE(list.reserve(scratch, 16));
    CHECK(list.data == initial_ptr);
    CHECK(scratch->user_bytes_used() == 16 * sizeof(u64));

    // Block the tip of the arena
    auto* blocker = scratch->alloc<u64>(1);
    *blocker      = 0xDEADBEEFULL;

    // Non-tip growth: relocates cleanly while preserving existing elements
    REQUIRE(list.push(scratch, 12345));
    REQUIRE(list.reserve(scratch, 32));
    CHECK(list.data != initial_ptr);
    CHECK(list[0] == 12345);
    CHECK(*blocker == 0xDEADBEEFULL);
  }

  arena_release_thread_scratch();
}

TEST_CASE("List stores trivially copyable String slices cleanly", "[list][string]")
{
  {
    ARENA_GET_SCRATCH(scratch);
    List<String> names{};

    REQUIRE(names.push(scratch, "Onyx"));
    REQUIRE(names.push(scratch, String::format(scratch, "Entity_{}", 42)));

    REQUIRE(names.len == 2);
    CHECK(names[0] == "Onyx");
    CHECK(names[1] == "Entity_42");
  }

  arena_release_thread_scratch();
}
