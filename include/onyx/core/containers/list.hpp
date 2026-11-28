//
// Created by Dwayne Edwards on 2026-11-27.
//

#pragma once

#include "onyx/core/memory/arena.hpp"
#include "onyx/platform/types.hpp"

#include <cassert>
#include <span>
#include <type_traits>


namespace onyx
{
  template <typename T>
  struct List
  {
    static_assert(std::is_trivially_copyable_v<T>, "List<T> requires trivially copyable types");

    static constexpr usize INITIAL_CAPACITY = 8;
    static constexpr usize GROWTH_FACTOR    = 2;

    T*    data = nullptr;
    usize len  = 0;
    usize cap  = 0;

    bool
    reserve(Arena* arena, usize min_cap) noexcept
    {
      if (cap >= min_cap) return true;
      if (!arena) return false;

      usize new_cap =
          (cap == 0) ? INITIAL_CAPACITY : (cap > (SIZE_MAX / GROWTH_FACTOR) ? min_cap : (cap * GROWTH_FACTOR));
      if (new_cap < min_cap) new_cap = min_cap;

      if (new_cap > SIZE_MAX / sizeof(T)) [[unlikely]]
      {
        return false;
      }

      if (data != nullptr && arena->try_extend(data, cap * sizeof(T), new_cap * sizeof(T)))
      {
        cap = new_cap;
        return true;
      }

      T* new_data = arena->alloc<T>(new_cap);
      if (!new_data) [[unlikely]]
      {
        return false;
      }

      if (data != nullptr && len > 0)
      {
        std::memcpy(new_data, data, len * sizeof(T));
      }
      data = new_data;
      cap  = new_cap;
      return true;
    }

    bool
    push(Arena* arena, const T& value) noexcept
    {
      if (len >= cap)
      {
        if (!reserve(arena, len + 1)) [[unlikely]]
        {
          return false;
        }
      }
      data[len++] = value;
      return true;
    }

    T
    pop() noexcept
    {
      assert(len > 0 && "Cannot pop from empty List");
      return data[--len];
    }

    // O(1) unordered removal: replaces element at `idx` with the last element
    void
    pop_swap(usize idx) noexcept
    {
      assert(idx < len && "pop_swap index out of bounds");
      --len;
      if (idx != len)
      {
        data[idx] = data[len];
      }
    }

    void
    clear() noexcept
    {
      len = 0;
    }

    [[nodiscard]] constexpr bool
    is_empty() const noexcept
    {
      return len == 0;
    }

    T&
    last() noexcept
    {
      assert(len > 0 && "List is empty");
      return data[len - 1];
    }

    const T&
    last() const noexcept
    {
      assert(len > 0 && "List is empty");
      return data[len - 1];
    }

    T&
    operator[](usize idx) noexcept
    {
      assert(idx < len);
      return data[idx];
    }

    const T&
    operator[](usize idx) const noexcept
    {
      assert(idx < len);
      return data[idx];
    }

    T*
    begin() noexcept
    {
      return data;
    }

    T*
    end() noexcept
    {
      return data + len;
    }

    const T*
    begin() const noexcept
    {
      return data;
    }

    const T*
    end() const noexcept
    {
      return data + len;
    }

    [[nodiscard]] std::span<T>
    span() noexcept
    {
      return {data, len};
    }

    [[nodiscard]] std::span<const T>
    span() const noexcept
    {
      return {data, len};
    }
  };
} // namespace onyx
