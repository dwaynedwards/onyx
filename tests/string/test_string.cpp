//
// Created by Dwayne Edwards on 2026-11-27.
//

#include "onyx/core/memory/arena.hpp"
#include "onyx/core/string.hpp"
#include "onyx/platform/platform.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using namespace onyx;

// Teaches Catch2 how to print onyx::String values on failure instead of `{?}`
namespace Catch
{
  template <> struct StringMaker<onyx::String>
  {
    static std::string
    convert(onyx::String s)
    {
      return s.is_empty() ? std::string{"\"\""} : std::string{'"'} + std::string{s.to_sv()} + '"';
    }
  };
} // namespace Catch

// =========================================================
// Compile-Time Incompatible Type Rejection Checks
// =========================================================
static_assert(!std::is_constructible_v<String, int>);
static_assert(!std::is_constructible_v<String, bool>);
static_assert(!std::is_constructible_v<String, char>);
static_assert(!std::is_constructible_v<String, const char8_t*>);
static_assert(!std::is_constructible_v<String, const wchar_t*>);
static_assert(!std::is_constructible_v<String, void*>);

static_assert(!std::is_assignable_v<String&, int>);
static_assert(!std::is_assignable_v<String&, const char8_t*>);
static_assert(!std::is_assignable_v<String&, void*>);


// =========================================================
// Compile-Time Constructor, Equality & Slicing Guarantees
// =========================================================
inline constexpr String ENGINE_TITLE = "Onyx Engine";
inline constexpr String SV_TITLE     = std::string_view{"Onyx Engine v0.1"}.substr(0, 11);

static_assert(ENGINE_TITLE.len == 11);
static_assert(ENGINE_TITLE == SV_TITLE);
static_assert(ENGINE_TITLE.to_slice(0, 4) == "Onyx");
static_assert(ENGINE_TITLE.to_slice(5, 99) == "Engine");
static_assert("Onyx" == ENGINE_TITLE.to_slice(0, 4)); // Symmetric hidden-friend operator==

// =========================================================
// Runtime String & StringBuilder Unit Tests
// =========================================================
TEST_CASE("String constructors, conversions, slicing, equality, and arena copy", "[string]")
{
  {
    ARENA_GET_SCRATCH(scratch);

    // 1. Default, nullptr, and null-pointer literal 0 constructors
    String empty_default{};
    String empty_null{nullptr};
    String empty_zero{0};
    CHECK(empty_default.is_empty());
    CHECK(empty_null.is_empty());
    CHECK(empty_zero.is_empty());
    CHECK(empty_null.ptr == nullptr);
    CHECK(empty_null.len == 0);
    CHECK(empty_default == empty_null);
    CHECK(empty_default == empty_zero);


    // 2. Pointer + explicit length constructor
    const char raw[] = {'O', 'n', 'y', 'x', '!'};
    String     from_ptr_len{raw, 4};
    CHECK(from_ptr_len == "Onyx");

    // 3. C-string (literal & runtime pointer) and std::string_view constructors
    const char*      runtime_cstr = "Hello";
    std::string_view runtime_sv   = std::string_view{"Hello World"}.substr(0, 5);

    String a = "Hello";
    String b{runtime_cstr};
    String c{runtime_sv};

    CHECK(a == b);
    CHECK(a == c);
    CHECK(a.to_sv() == "Hello");

    // 4. Symmetric raw byte conversions (`from_bytes` / `to_bytes`)
    String from_raw = String::from_bytes(raw, 4);
    CHECK(from_raw == "Onyx");
    auto byte_span = from_raw.to_bytes();
    REQUIRE(byte_span.size() == 4);
    CHECK(byte_span[0] == std::byte{'O'});
    CHECK(byte_span[3] == std::byte{'x'});

    // 5. Slicing bounds clamping
    CHECK(a.to_slice(1, 4) == "ell");
    CHECK(a.to_slice(4, 2).is_empty());
    CHECK(a.to_slice(10, 20).is_empty());

    // 6. Identical-pointer fast-path
    String alias = a;
    CHECK(alias == a);

    // 7. Arena-allocated null-terminated copy
    String cloned = a.copy(scratch);
    CHECK(cloned == a);
    CHECK(cloned.ptr != a.ptr);
    CHECK(cloned.ptr[cloned.len] == '\0');
    CHECK(std::strlen(cloned.ptr) == cloned.len);
  }

  arena_release_thread_scratch();
}

TEST_CASE("String::format produces null-terminated arena strings", "[string]")
{
  {
    ARENA_GET_SCRATCH(scratch);

    String s1 = String::format(scratch, "{} v{}.{}", "Onyx", 0, 1);
    REQUIRE(s1 == "Onyx v0.1");
    CHECK(s1.ptr[s1.len] == '\0');

    // Custom std::formatter<onyx::String> works inside nested format calls
    String s2 = String::format(scratch, "[{}] on {}", s1, to_string(OS));
    REQUIRE(s2.len > s1.len);
    CHECK(s2.ptr[s2.len] == '\0');
    CHECK(s2.to_slice(0, 11) == "[Onyx v0.1]");
  }

  arena_release_thread_scratch();
}

TEST_CASE("StringBuilder appends strings, views, literals, and formatted text", "[string]")
{
  {
    ARENA_GET_SCRATCH(scratch);

    StringBuilder sb{};
    CHECK(sb.view().is_empty());

    sb.append(scratch, "Engine: ");
    sb.append(scratch, ENGINE_TITLE);
    sb.append(scratch, std::string_view{" | Mode: Debug"});
    sb.append_format(scratch, " | Compiler: {} | Port: {}", to_string(COMPILER), 8080);

    String result = sb.view();
    REQUIRE_FALSE(result.is_empty());
    CHECK(result.ptr[result.len] == '\0');
    CHECK(result.to_slice(0, 19) == "Engine: Onyx Engine");

    sb.clear();
    CHECK(sb.view().is_empty());
  }

  arena_release_thread_scratch();
}
