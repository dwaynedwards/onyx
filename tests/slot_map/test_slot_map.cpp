//
// Created by Dwayne Edwards on 2026-11-28.
//

#include "onyx/core/containers/slot_map.hpp"
#include "onyx/core/memory/arena.hpp"
#include "onyx/platform/types.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace onyx;

namespace
{
  struct EnemyTag
  {
  };
  struct Enemy
  {
    u32 id     = 0;
    f32 health = 100.0f;
  };

  using EnemyHandle = Handle<EnemyTag>;
} // namespace

TEST_CASE("SlotMap insert, lookup, dense iteration, and stale-handle invalidation", "[slot_map]")
{
  {
    ARENA_GET_SCRATCH(scratch);
    SlotMap<Enemy, EnemyTag> enemies{};
    REQUIRE(enemies.is_empty());

    EnemyHandle h0 = enemies.insert(scratch, Enemy{.id = 10, .health = 100.0f});
    EnemyHandle h1 = enemies.insert(scratch, Enemy{.id = 20, .health = 80.0f});
    EnemyHandle h2 = enemies.insert(scratch, Enemy{.id = 30, .health = 50.0f});

    REQUIRE(enemies.len() == 3);
    REQUIRE(h0.is_valid());
    REQUIRE(h1.is_valid());
    REQUIRE(h2.is_valid());

    CHECK(enemies.get(h0)->id == 10);
    CHECK(enemies.get(h1)->id == 20);
    CHECK(enemies.get(h2)->id == 30);

    // Dense iteration visits all 3 live enemies contiguously
    u32 id_sum = 0;
    for (const Enemy& e: enemies)
    {
      id_sum += e.id;
    }
    CHECK(id_sum == 60);

    // Remove middle element `h1`: swaps `h2` into dense index 1
    REQUIRE(enemies.remove(h1));
    REQUIRE(enemies.len() == 2);

    // Stale handle `h1` fails lookup & double-remove
    CHECK_FALSE(enemies.contains(h1));
    CHECK(enemies.get(h1) == nullptr);
    CHECK_FALSE(enemies.remove(h1));

    // Surviving handles `h0` and `h2` still resolve to their exact items
    REQUIRE(enemies.get(h0) != nullptr);
    REQUIRE(enemies.get(h2) != nullptr);
    CHECK(enemies.get(h0)->id == 10);
    CHECK(enemies.get(h2)->id == 30);

    // Re-inserting recycles `h1`'s slot index with an incremented generation
    EnemyHandle h3 = enemies.insert(scratch, Enemy{.id = 40, .health = 120.0f});
    CHECK(h3.index == h1.index);
    CHECK(h3.generation > h1.generation);
    CHECK(enemies.get(h1) == nullptr);
    CHECK(enemies.get(h3)->id == 40);

    // `handle_at_dense` recovers the live handle for every dense slot
    for (usize i = 0; i < enemies.len(); ++i)
    {
      EnemyHandle dh = enemies.handle_at_dense(i);
      REQUIRE(enemies.get(dh) == &enemies.data[i]);
    }

    // `clear()` invalidates all live handles
    enemies.clear();
    CHECK(enemies.is_empty());
    CHECK(enemies.get(h0) == nullptr);
    CHECK(enemies.get(h2) == nullptr);
    CHECK(enemies.get(h3) == nullptr);
  }

  arena_release_thread_scratch();
}

TEST_CASE("SlotMap handles null, out-of-bounds, and null-arena inputs safely", "[slot_map]")
{
  SlotMap<Enemy, EnemyTag> enemies{};

  // Null arena insert returns invalid handle
  CHECK(enemies.insert(nullptr, Enemy{.id = 1}).is_null());
  CHECK(enemies.is_empty());

  // Invalid / out-of-range handles on an empty SlotMap
  CHECK(enemies.get(EnemyHandle::invalid()) == nullptr);
  CHECK(enemies.get(EnemyHandle{.index = 0, .generation = 1}) == nullptr);
  CHECK(enemies.get(EnemyHandle{.index = 999, .generation = 1}) == nullptr);
  CHECK_FALSE(enemies.remove(EnemyHandle::invalid()));
  CHECK_FALSE(enemies.remove(EnemyHandle{.index = 0, .generation = 1}));
}

TEST_CASE("SlotMap skips generation 0 on u32 wrap-around", "[slot_map]")
{
  {
    ARENA_GET_SCRATCH(scratch);
    SlotMap<u32, EnemyTag> sm{};

    EnemyHandle h = sm.insert(scratch, 42);
    REQUIRE(h.is_valid());

    // Simulate slot generation reaching UINT32_MAX right before removal
    sm.slots[h.index].generation = UINT32_MAX;
    h.generation                 = UINT32_MAX;

    REQUIRE(sm.remove(h));
    // Must wrap past 0 directly to 1 (0 is reserved for null handles)
    CHECK(sm.slots[h.index].generation == 1);

    EnemyHandle recycled = sm.insert(scratch, 99);
    CHECK(recycled.index == h.index);
    CHECK(recycled.generation == 1);
    CHECK(recycled.is_valid());
    CHECK(sm.get(h) == nullptr);
    CHECK(*sm.get(recycled) == 99);
  }

  arena_release_thread_scratch();
}

TEST_CASE("SlotMap reserve, bulk churn, and const accessors work across reallocations", "[slot_map]")
{
  {
    ARENA_GET_SCRATCH(scratch);
    SlotMap<u64, EnemyTag> sm{};
    REQUIRE(sm.reserve(scratch, 256));

    List<EnemyHandle> handles{};
    for (u64 i = 0; i < 300; ++i)
    {
      EnemyHandle h = sm.insert(scratch, i * 10);
      REQUIRE(h.is_valid());
      REQUIRE(handles.push(scratch, h));
    }
    REQUIRE(sm.len() == 300);

    // Remove every third element (including index 0 and the tail element)
    for (usize i = 0; i < handles.len; i += 3)
    {
      REQUIRE(sm.remove(handles[i]));
    }
    REQUIRE(sm.len() == 200);

    // Verify const get(), span(), and const range-for iteration
    const auto& const_sm = sm;
    CHECK(const_sm.span().size() == 200);

    u64 count = 0;
    for (const u64& val: const_sm)
    {
      (void)val;
      ++count;
    }
    CHECK(count == 200);

    for (usize i = 0; i < handles.len; ++i)
    {
      if (i % 3 == 0)
      {
        CHECK(const_sm.get(handles[i]) == nullptr);
      }
      else
      {
        const u64* ptr = const_sm.get(handles[i]);
        REQUIRE(ptr != nullptr);
        CHECK(*ptr == i * 10);
      }
    }
  }

  arena_release_thread_scratch();
}
