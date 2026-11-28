//
// Created by Dwayne Edwards on 2026-11-27.
//

#include "onyx/core/containers/list.hpp"
#include "onyx/core/memory/arena.hpp"
#include "onyx/core/string.hpp"
#include "onyx/platform/types.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace onyx;

namespace Catch
{
  template <>
  struct StringMaker<onyx::String>
  {
    static std::string
    convert(onyx::String s)
    {
      return s.is_empty() ? std::string{"\"\""} : std::string{'"'} + std::string{s.to_sv()} + '"';
    }
  };
} // namespace Catch

TEST_CASE("List push, pop, pop_swap, last, indexing, span, and range iteration", "[list]")
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
    REQUIRE(list.cap >= List<u32>::INITIAL_CAPACITY);
    CHECK(list[0] == 0);
    CHECK(list[9] == 90);
    CHECK(list.last() == 90);
    CHECK(list.span().size() == 10);

    u32 sum = 0;
    for (u32 val: list)
    {
      sum += val;
    }
    CHECK(sum == 450);

    CHECK(list.pop() == 90);
    CHECK(list.len == 9);
    CHECK(list.last() == 80);

    // pop_swap replaces index 2 (20) with last element (80) in O(1)
    list.pop_swap(2);
    CHECK(list.len == 8);
    CHECK(list[2] == 80);
    CHECK(list.last() == 70);

    // pop_swap on the last element simply decrements len
    list.pop_swap(list.len - 1);
    CHECK(list.len == 7);
    CHECK(list.last() == 60);
    
    const auto& const_list = list;
    CHECK(const_list.last() == 60);

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

    const usize initial_cap = List<u64>::INITIAL_CAPACITY;
    const usize grown_cap   = initial_cap * List<u64>::GROWTH_FACTOR;

    REQUIRE(list.reserve(scratch, initial_cap));
    const u64* initial_ptr = list.data;
    CHECK(scratch->user_bytes_used() == initial_cap * sizeof(u64));

    REQUIRE(list.reserve(scratch, grown_cap));
    CHECK(list.data == initial_ptr);
    CHECK(scratch->user_bytes_used() == grown_cap * sizeof(u64));

    auto* blocker = scratch->alloc<u64>(1);
    *blocker      = 0xDEADBEEFULL;

    REQUIRE(list.push(scratch, 12345));
    REQUIRE(list.reserve(scratch, grown_cap * List<u64>::GROWTH_FACTOR));
    CHECK(list.data != initial_ptr);
    CHECK(list[0] == 12345);
    CHECK(*blocker == 0xDEADBEEFULL);
  }

  arena_release_thread_scratch();
}

TEST_CASE("List rejects null arenas and element overflow safely", "[list][overflow]")
{
  List<u64> list{};
  CHECK_FALSE(list.reserve(nullptr, 4));
  CHECK_FALSE(list.push(nullptr, 42));
  CHECK(list.is_empty());

  {
    ARENA_GET_SCRATCH(scratch);
    CHECK_FALSE(list.reserve(scratch, (SIZE_MAX / sizeof(u64)) + 1));
    CHECK(list.is_empty());
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
