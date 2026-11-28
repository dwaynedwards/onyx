//
// Created by Dwayne Edwards on 2026-11-28.
//

#pragma once

#include "onyx/core/memory/arena.hpp"
#include "onyx/core/string.hpp"
#include "onyx/platform/types.hpp"

#include <type_traits>


namespace onyx
{
  struct MapHash
  {
    // 32-bit FNV-1a & Murmur3 finalizer constants
    static constexpr u32 FNV32_OFFSET_BASIS = 0x811c'9dc5U;
    static constexpr u32 FNV32_PRIME        = 0x0100'0193U;
    static constexpr u32 MIX32_MUL1         = 0x7feb'352dU;
    static constexpr u32 MIX32_MUL2         = 0x846c'a68bU;

    // 64-bit FNV-1a & SplitMix64 finalizer constants
    static constexpr u64 FNV64_OFFSET_BASIS = 0xcbf2'9ce4'8422'2325ULL;
    static constexpr u64 FNV64_PRIME        = 0x0000'0100'0000'01b3ULL;
    static constexpr u64 MIX64_MUL1         = 0xbf58'476d'1ce4'e5b9ULL;
    static constexpr u64 MIX64_MUL2         = 0x94d0'49bb'1331'11ebULL;

    [[nodiscard]] static constexpr u32
    mix_u32(u32 x) noexcept
    {
      x ^= x >> 16;
      x *= MIX32_MUL1;
      x ^= x >> 15;
      x *= MIX32_MUL2;
      x ^= x >> 16;
      return x;
    }

    [[nodiscard]] static constexpr u32
    fnv1a_32(const void* data, usize len) noexcept
    {
      const auto* p = static_cast<const u8*>(data);
      u32         h = FNV32_OFFSET_BASIS;
      for (usize i = 0; i < len; ++i)
      {
        h ^= p[i];
        h *= FNV32_PRIME;
      }
      return mix_u32(h);
    }

    [[nodiscard]] static constexpr u64
    mix_u64(u64 x) noexcept
    {
      x ^= x >> 30;
      x *= MIX64_MUL1;
      x ^= x >> 27;
      x *= MIX64_MUL2;
      x ^= x >> 31;
      return x;
    }

    [[nodiscard]] static constexpr u64
    fnv1a_64(const void* data, usize len) noexcept
    {
      const auto* p = static_cast<const u8*>(data);
      u64         h = FNV64_OFFSET_BASIS;
      for (usize i = 0; i < len; ++i)
      {
        h ^= p[i];
        h *= FNV64_PRIME;
      }
      return mix_u64(h);
    }

    template <typename K>
    [[nodiscard]] static u64
    hash(const K& key) noexcept
    {
      if constexpr (std::is_same_v<K, String>)
      {
        return fnv1a_64(key.ptr, key.len);
      }
      else if constexpr (std::is_pointer_v<K>)
      {
        return mix_u64(static_cast<u64>(reinterpret_cast<uptr>(key)));
      }
      else if constexpr (std::is_integral_v<K> || std::is_enum_v<K>)
      {
        if constexpr (sizeof(K) <= sizeof(u32))
        {
          return mix_u64(mix_u32(static_cast<u32>(key)));
        }
        else
        {
          return mix_u64(static_cast<u64>(key));
        }
      }
      else
      {
        static_assert(std::has_unique_object_representations_v<K>,
                      "Struct keys with padding require a custom equality/hash or zeroed padding");
        return fnv1a_64(&key, sizeof(K));
      }
    }

    template <typename K>
    [[nodiscard]] static bool
    eq(const K& a, const K& b) noexcept
    {
      if constexpr (std::is_same_v<K, String>)
      {
        return a == b;
      }
      else if constexpr (requires { a == b; })
      {
        return a == b;
      }
      else
      {
        return std::memcmp(&a, &b, sizeof(K)) == 0;
      }
    }
  };

  template <typename K, typename V, typename Hasher = MapHash>
  struct Map
  {
    static_assert(std::is_trivially_copyable_v<K> && std::is_trivially_copyable_v<V>,
                  "Map<K, V> requires trivially copyable types");

    static constexpr usize INITIAL_CAPACITY = 16;
    static constexpr usize GROWTH_FACTOR    = 2;
    static constexpr usize CTRL_ALIGNMENT   = 64;
    static constexpr usize LOAD_FACTOR_NUM  = 3;
    static constexpr usize LOAD_FACTOR_DEN  = 4;

    static constexpr u8    SLOT_EMPTY   = 0x00;
    static constexpr u8    OCCUPIED_BIT = 0x80U;
    static constexpr u8    H2_MASK      = 0x7FU;
    static constexpr usize H1_SHIFT     = 7;

    u8*   ctrl = nullptr;
    K*    keys = nullptr;
    V*    vals = nullptr;
    usize len  = 0;
    usize cap  = 0;

    [[nodiscard]] constexpr bool
    is_empty() const noexcept
    {
      return len == 0;
    }

    bool
    reserve(Arena* arena, usize desired_elements) noexcept
    {
      if (!arena) return false;
      if (desired_elements > (SIZE_MAX / LOAD_FACTOR_DEN)) [[unlikely]]
      {
        return false;
      }

      const usize min_slots = (desired_elements * LOAD_FACTOR_DEN) / LOAD_FACTOR_NUM + 1;
      usize       target    = INITIAL_CAPACITY;
      while (target < min_slots)
      {
        if (target > (SIZE_MAX / GROWTH_FACTOR)) [[unlikely]]
        {
          return false;
        }
        target *= GROWTH_FACTOR;
      }

      if (target <= cap) return true;
      if (target > SIZE_MAX / sizeof(K) || target > SIZE_MAX / sizeof(V)) [[unlikely]]
      {
        return false;
      }

      u8* new_ctrl = static_cast<u8*>(arena->alloc_zeroed(target * sizeof(u8), CTRL_ALIGNMENT));
      K*  new_keys = arena->alloc<K>(target);
      V*  new_vals = arena->alloc<V>(target);
      if (!new_ctrl || !new_keys || !new_vals) [[unlikely]]
      {
        return false;
      }

      u8*         old_ctrl = ctrl;
      const K*    old_keys = keys;
      const V*    old_vals = vals;
      const usize old_cap  = cap;

      ctrl = new_ctrl;
      keys = new_keys;
      vals = new_vals;
      cap  = target;
      len  = 0;

      if (old_ctrl != nullptr)
      {
        const usize mask = target - 1;
        for (usize i = 0; i < old_cap; ++i)
        {
          if (old_ctrl[i] != SLOT_EMPTY)
          {
            const u64 h   = Hasher::hash(old_keys[i]);
            const u8  h2  = make_h2(h);
            usize     idx = static_cast<usize>(h >> H1_SHIFT) & mask;
            while (ctrl[idx] != SLOT_EMPTY)
            {
              idx = (idx + 1) & mask;
            }
            ctrl[idx] = h2;
            keys[idx] = old_keys[i];
            vals[idx] = old_vals[i];
            ++len;
          }
        }
      }

      return true;
    }

    bool
    put(Arena* arena, const K& key, const V& val) noexcept
    {
      if ((len + 1) * LOAD_FACTOR_DEN >= cap * LOAD_FACTOR_NUM)
      {
        if (!reserve(arena, len + 1)) [[unlikely]]
        {
          return false;
        }
      }

      const u64   h    = Hasher::hash(key);
      const u8    h2   = make_h2(h);
      const usize mask = cap - 1;
      usize       idx  = static_cast<usize>(h >> H1_SHIFT) & mask;

      for (;;)
      {
        const u8 c = ctrl[idx];
        if (c == SLOT_EMPTY)
        {
          ctrl[idx] = h2;
          keys[idx] = key;
          vals[idx] = val;
          ++len;
          return true;
        }
        if (c == h2 && Hasher::eq(keys[idx], key))
        {
          vals[idx] = val;
          return true;
        }
        idx = (idx + 1) & mask;
      }
    }

    [[nodiscard]] V*
    get(const K& key) noexcept
    {
      if (cap == 0) return nullptr;
      const u64   h    = Hasher::hash(key);
      const u8    h2   = make_h2(h);
      const usize mask = cap - 1;
      usize       idx  = static_cast<usize>(h >> H1_SHIFT) & mask;

      for (;;)
      {
        const u8 c = ctrl[idx];
        if (c == SLOT_EMPTY) return nullptr;
        if (c == h2 && Hasher::eq(keys[idx], key))
        {
          return &vals[idx];
        }
        idx = (idx + 1) & mask;
      }
    }

    [[nodiscard]] const V*
    get(const K& key) const noexcept
    {
      return const_cast<Map*>(this)->get(key);
    }

    [[nodiscard]] bool
    contains(const K& key) const noexcept
    {
      return get(key) != nullptr;
    }

    bool
    remove(const K& key) noexcept
    {
      if (cap == 0 || len == 0) return false;
      const u64   h    = Hasher::hash(key);
      const u8    h2   = make_h2(h);
      const usize mask = cap - 1;
      usize       hole = static_cast<usize>(h >> H1_SHIFT) & mask;

      for (;;)
      {
        const u8 c = ctrl[hole];
        if (c == SLOT_EMPTY) return false;
        if (c == h2 && Hasher::eq(keys[hole], key))
        {
          usize scan = (hole + 1) & mask;
          while (ctrl[scan] != SLOT_EMPTY)
          {
            const usize home = static_cast<usize>(Hasher::hash(keys[scan]) >> H1_SHIFT) & mask;
            if (((hole - home) & mask) < ((scan - home) & mask))
            {
              ctrl[hole] = ctrl[scan];
              keys[hole] = keys[scan];
              vals[hole] = vals[scan];
              hole       = scan;
            }
            scan = (scan + 1) & mask;
          }
          ctrl[hole] = SLOT_EMPTY;
          --len;
          return true;
        }
        hole = (hole + 1) & mask;
      }
    }

    void
    clear() noexcept
    {
      if (ctrl != nullptr && cap > 0)
      {
        std::memset(ctrl, SLOT_EMPTY, cap * sizeof(u8));
        len = 0;
      }
    }

    // Range-based `for (auto [k, v] : map)` support with structured bindings
    template <bool IsConst>
    struct Iterator
    {
      using MapPtr = std::conditional_t<IsConst, const Map*, Map*>;
      using ValRef = std::conditional_t<IsConst, const V&, V&>;

      struct Entry
      {
        const K& key;
        ValRef   val;
      };

      MapPtr map;
      usize  idx;

      void
      advance() noexcept
      {
        while (idx < map->cap && map->ctrl[idx] == SLOT_EMPTY)
        {
          ++idx;
        }
      }

      Entry
      operator*() const noexcept
      {
        return {map->keys[idx], map->vals[idx]};
      }

      Iterator&
      operator++() noexcept
      {
        ++idx;
        advance();
        return *this;
      }

      bool
      operator!=(const Iterator& other) const noexcept
      {
        return idx != other.idx;
      }
    };

    auto
    begin() noexcept
    {
      Iterator<false> it{this, 0};
      it.advance();
      return it;
    }

    auto
    end() noexcept
    {
      return Iterator<false>{this, cap};
    }

    auto
    begin() const noexcept
    {
      Iterator<true> it{this, 0};
      it.advance();
      return it;
    }

    auto
    end() const noexcept
    {
      return Iterator<true>{this, cap};
    }

  private:
    [[nodiscard]] static constexpr u8
    make_h2(u64 h) noexcept
    {
      return static_cast<u8>(OCCUPIED_BIT | (h & H2_MASK));
    }
  };

} // namespace onyx
