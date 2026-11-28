//
// Created by Dwayne Edwards on 2026-11-27.
//

#pragma once

#include "onyx/core/containers/list.hpp"
#include "onyx/core/memory/arena.hpp"
#include "onyx/platform/types.hpp"

#include <cstddef>
#include <cstring>
#include <format>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace onyx
{
  namespace internal
  {
    [[nodiscard]] constexpr usize
    cstr_len(const char* cstr) noexcept
    {
      if (!cstr) return 0;
      if (std::is_constant_evaluated())
      {
        usize n = 0;
        while (cstr[n] != '\0')
        {
          ++n;
        }
        return n;
      }
      return std::strlen(cstr);
    }

    [[nodiscard]] constexpr bool
    chars_equal(const char* a, const char* b, usize len) noexcept
    {
      if (std::is_constant_evaluated())
      {
        for (usize i = 0; i < len; ++i)
        {
          if (a[i] != b[i]) return false;
        }
        return true;
      }
      if (a == b) return true;
      return std::memcmp(a, b, len) == 0;
    }

    template <typename... Args>
    void
    format_into(char* dst, usize len, std::format_string<Args...> fmt_str, Args&&... args)
    {
      std::format_to_n(dst, static_cast<isize>(len), fmt_str, std::forward<Args>(args)...);
      dst[len] = '\0';
    }
  } // namespace internal

  struct String
  {
    const char* ptr = nullptr;
    usize       len = 0;

    constexpr String() noexcept = default;
    constexpr String(const char* data, usize size) noexcept : ptr(data), len(size) {}
    constexpr String(const char* cstr) noexcept : ptr(cstr), len(internal::cstr_len(cstr)) {}
    constexpr String(std::string_view sv) noexcept : ptr(sv.data()), len(sv.size()) {}

    [[nodiscard]] constexpr bool
    is_empty() const noexcept
    {
      return len == 0 || ptr == nullptr;
    }

    [[nodiscard]] static String
    from_bytes(const void* data, usize size) noexcept
    {
      return {static_cast<const char*>(data), size};
    }

    [[nodiscard]] constexpr std::string_view
    to_sv() const noexcept
    {
      return {ptr, len};
    }

    [[nodiscard]] std::span<const std::byte>
    to_bytes() const noexcept
    {
      return {reinterpret_cast<const std::byte*>(ptr), len};
    }

    [[nodiscard]] constexpr String
    to_slice(usize start, usize end) const noexcept
    {
      if (start > len) start = len;
      if (end > len) end = len;
      if (end < start) end = start;
      return {ptr + start, end - start};
    }

    [[nodiscard]] friend constexpr bool
    operator==(String lhs, String rhs) noexcept
    {
      if (lhs.len != rhs.len) return false;
      if (lhs.len == 0) return true;
      return internal::chars_equal(lhs.ptr, rhs.ptr, lhs.len);
    }

    // Arena-allocated null-terminated copy
    [[nodiscard]] String
    copy(Arena* arena) const noexcept
    {
      auto* dst = arena->alloc<char>(len + 1);
      if (!dst) return {};
      if (len > 0) std::memcpy(dst, ptr, len);
      dst[len] = '\0';
      return {dst, len};
    }

    // Type-safe C++20 std::format directly into an Arena (zero heap allocs, null-terminated)
    template <typename... Args>
    [[nodiscard]] static String
    format(Arena* arena, std::format_string<Args...> fmt_str, Args&&... args)
    {
      const usize out_len = std::formatted_size(fmt_str, std::forward<Args>(args)...);
      auto*       dst     = arena->alloc<char>(out_len + 1);
      if (!dst) return {};

      internal::format_into(dst, out_len, fmt_str, std::forward<Args>(args)...);
      return {dst, out_len};
    }
  };

  // =========================================================
  // Arena-Backed Dynamic String Builder
  // =========================================================
  struct StringBuilder
  {
    List<char> buf = {};

    void
    append(Arena* arena, String s) noexcept
    {
      if (s.len == 0) return;
      if (!buf.reserve(arena, buf.len + s.len + 1)) return;
      std::memcpy(buf.data + buf.len, s.ptr, s.len);
      buf.len += s.len;
      buf.data[buf.len] = '\0';
    }

    template <typename... Args>
    void
    append_format(Arena* arena, std::format_string<Args...> fmt_str, Args&&... args)
    {
      const usize add_len = std::formatted_size(fmt_str, std::forward<Args>(args)...);
      if (add_len == 0) return;
      if (!buf.reserve(arena, buf.len + add_len + 1)) return;

      internal::format_into(buf.data + buf.len, add_len, fmt_str, std::forward<Args>(args)...);
      buf.len += add_len;
    }

    [[nodiscard]] String
    view() const noexcept
    {
      return {buf.data, buf.len};
    }

    void
    clear() noexcept
    {
      buf.clear();
    }
  };

} // namespace onyx

template <> struct std::formatter<onyx::String, char> : std::formatter<std::string_view, char>
{
  template <typename FormatContext>
  auto
  format(const onyx::String& s, FormatContext& ctx) const
  {
    return std::formatter<std::string_view, char>::format(s.to_sv(), ctx);
  }
};
