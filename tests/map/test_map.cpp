//
// Created by Dwayne Edwards on 2026-11-28.
//

#include "onyx/core/containers/map.hpp"
#include "onyx/core/handle.hpp"
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

namespace
{
  struct TextureTag
  {
  };
  struct EntityTag
  {
  };

  using TextureHandle = Handle<TextureTag>;
  using EntityHandle  = Handle<EntityTag>;

  static_assert(sizeof(TextureHandle) == 8);
  static_assert(std::is_trivially_copyable_v<TextureHandle>);
  static_assert(std::has_unique_object_representations_v<TextureHandle>);
  static_assert(!std::is_same_v<TextureHandle, EntityHandle>);
} // namespace

TEST_CASE("Map supports 32-bit and 64-bit keys and hash functions", "[map][hash]")
{
  // Compile-time hash mixing checks
  static_assert(MapHash::mix_u32(0) == 0);
  static_assert(MapHash::mix_u32(1) != MapHash::mix_u32(2));
  static_assert(MapHash::mix_u64(0) == 0);
  static_assert(MapHash::mix_u64(1) != MapHash::mix_u64(2));

  {
    ARENA_GET_SCRATCH(scratch);

    // Runtime 32-bit & 64-bit FNV-1a string hashing
    const char label[] = "Onyx";
    CHECK(MapHash::fnv1a_32(label, 4) != 0);
    CHECK(MapHash::fnv1a_64(label, 4) != 0);
    CHECK(MapHash::fnv1a_32("Onyx", 4) != MapHash::fnv1a_32("onyx", 4));
    CHECK(MapHash::fnv1a_64("Onyx", 4) != MapHash::fnv1a_64("onyx", 4));

    // 32-bit integer keys (`u32`)
    Map<u32, String> map32{};
    REQUIRE(map32.put(scratch, 0x0001'86a0U, "hundred_thousand"));
    REQUIRE(map32.put(scratch, 0x0003'0d40U, "two_hundred_thousand"));
    CHECK(*map32.get(0x0001'86a0U) == "hundred_thousand");
    CHECK(*map32.get(0x0003'0d40U) == "two_hundred_thousand");
    CHECK(map32.get(0x0004'93e0U) == nullptr);

    // 64-bit integer keys (`u64`)
    Map<u64, u32> map64{};
    REQUIRE(map64.put(scratch, 0x0000'0001'0000'0001ULL, 1));
    REQUIRE(map64.put(scratch, 0x0000'0002'0000'0001ULL, 2));
    CHECK(*map64.get(0x0000'0001'0000'0001ULL) == 1);
    CHECK(*map64.get(0x0000'0002'0000'0001ULL) == 2);
  }

  arena_release_thread_scratch();
}

TEST_CASE("Handle type safety, validity, and equality", "[handle]")
{
  TextureHandle null_handle = TextureHandle::invalid();
  CHECK_FALSE(null_handle.is_valid());
  CHECK(null_handle.is_null());
  CHECK_FALSE(static_cast<bool>(null_handle));

  TextureHandle h1{.index = 3, .generation = 1};
  TextureHandle h2{.index = 3, .generation = 2};
  CHECK(h1.is_valid());
  CHECK(static_cast<bool>(h1));
  CHECK(h1 != h2);
  CHECK(h1 == (TextureHandle{.index = 3, .generation = 1}));
}

TEST_CASE("Map supports String keys, Handle values, updates, and structured iteration", "[map][string][handle]")
{
  {
    ARENA_GET_SCRATCH(scratch);
    Map<String, TextureHandle> textures{};
    REQUIRE(textures.is_empty());

    REQUIRE(textures.put(scratch, "brick.png", TextureHandle{.index = 0, .generation = 1}));
    REQUIRE(textures.put(scratch, "grass.png", TextureHandle{.index = 1, .generation = 1}));
    REQUIRE(textures.len == 2);

    REQUIRE(textures.put(scratch, "brick.png", TextureHandle{.index = 0, .generation = 2}));
    REQUIRE(textures.len == 2);

    const TextureHandle* brick = textures.get("brick.png");
    REQUIRE(brick != nullptr);
    CHECK(*brick == (TextureHandle{.index = 0, .generation = 2}));
    CHECK(textures.contains("grass.png"));
    CHECK(textures.get("missing.png") == nullptr);

    u32 visited = 0;
    for (auto [name, handle]: textures)
    {
      CHECK_FALSE(name.is_empty());
      CHECK(handle.is_valid());
      ++visited;
    }
    CHECK(visited == 2);
  }

  arena_release_thread_scratch();
}

TEST_CASE("Map handles rehashing, collisions, backshift removal, and pointer/Handle keys", "[map]")
{
  {
    ARENA_GET_SCRATCH(scratch);

    SECTION("Rehashing and backshift deletion across 500 entries")
    {
      Map<u64, u64> map{};
      for (u64 i = 1; i <= 500; ++i)
      {
        REQUIRE(map.put(scratch, i * 17, i * 100));
      }
      REQUIRE(map.len == 500);
      REQUIRE(map.cap >= 1024);

      for (u64 i = 2; i <= 500; i += 2)
      {
        REQUIRE(map.remove(i * 17));
        CHECK_FALSE(map.contains(i * 17));
      }
      REQUIRE(map.len == 250);

      for (u64 i = 1; i <= 500; i += 2)
      {
        const u64* val = map.get(i * 17);
        REQUIRE(val != nullptr);
        CHECK(*val == i * 100);
      }

      map.clear();
      CHECK(map.is_empty());
      CHECK(map.get(17) == nullptr);
    }

    SECTION("Handle<Tag> and pointer keys hash and compare properly")
    {
      Map<EntityHandle, u32> entity_scores{};
      EntityHandle           e1{.index = 10, .generation = 1};
      EntityHandle           e2{.index = 10, .generation = 2};

      REQUIRE(entity_scores.put(scratch, e1, 50));
      REQUIRE(entity_scores.put(scratch, e2, 99));
      CHECK(*entity_scores.get(e1) == 50);
      CHECK(*entity_scores.get(e2) == 99);

      int               a = 1, b = 2;
      Map<int*, String> ptr_names{};
      REQUIRE(ptr_names.put(scratch, &a, "first"));
      REQUIRE(ptr_names.put(scratch, &b, "second"));
      CHECK(*ptr_names.get(&a) == "first");
      CHECK(*ptr_names.get(&b) == "second");
    }
  }

  arena_release_thread_scratch();
}
