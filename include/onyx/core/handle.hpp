//
// Created by Dwayne Edwards on 2026-11-28.
//

#pragma once

#include "onyx/platform/types.hpp"


namespace onyx
{
  template <typename Tag>
  struct Handle
  {
    u32 index      = 0;
    u32 generation = 0; // 0 = null / invalid handle

    [[nodiscard]] static constexpr Handle
    invalid() noexcept
    {
      return {};
    }

    [[nodiscard]] constexpr bool
    is_valid() const noexcept
    {
      return generation != 0;
    }

    [[nodiscard]] constexpr bool
    is_null() const noexcept
    {
      return generation == 0;
    }

    [[nodiscard]] constexpr explicit
    operator bool() const noexcept
    {
      return is_valid();
    }

    friend constexpr bool
    operator==(Handle, Handle) noexcept = default;
  };

  //
  // Example usage:
  //
  // struct TextureTag {};
  // using TextureHandle = Handle<TextureTag>;
  //
  // struct Texture
  // {
  //   String name   = {};
  //   u32    width  = 0;
  //   u32    height = 0;
  //   u32    gpu_id = 0;
  // };
  //
  // struct TextureRegistry
  // {
  //   List<Texture>              textures    = {}; // Tightly packed data
  //   List<u32>                  generations = {}; // Parallel generation counter per slot
  //   List<u32>                  free_slots  = {}; // Recycled slot indices
  //   Map<String, TextureHandle> by_name     = {}; // Name -> Handle lookup
  //
  //   [[nodiscard]] TextureHandle
  //   load(Arena* arena, String name, u32 width, u32 height, u32 gpu_id) noexcept
  //   {
  //     // 1. Return existing handle if already loaded
  //     if (const TextureHandle* existing = by_name.get(name))
  //     {
  //       if (get(*existing) != nullptr)
  //       {
  //         return *existing;
  //       }
  //     }
  //
  //     // 2. Copy string into `arena` so the Map key outlives temporary caller strings
  //     String  owned_name = name.copy(arena);
  //     Texture tex{.name = owned_name, .width = width, .height = height, .gpu_id = gpu_id};
  //
  //     // 3. Reuse a freed slot or append a new one to `textures`
  //     u32 slot = 0;
  //     if (!free_slots.is_empty())
  //     {
  //       slot           = free_slots.pop();
  //       textures[slot] = tex;
  //     }
  //     else
  //     {
  //       slot = static_cast<u32>(textures.len);
  //       if (!textures.push(arena, tex) || !generations.push(arena, 1))
  //       {
  //         return {};
  //       }
  //     }
  //
  //     TextureHandle handle{.index = slot, .generation = generations[slot]};
  //     if (!by_name.put(arena, owned_name, handle))
  //     {
  //       return {};
  //     }
  //     return handle;
  //   }
  //
  //   // O(1) safe lookup: detects stale handles immediately
  //   [[nodiscard]] Texture*
  //   get(TextureHandle h) noexcept
  //   {
  //     if (!h.is_valid() || h.index >= textures.len) return nullptr;
  //     if (generations[h.index] != h.generation) return nullptr; // Stale handle!
  //     return &textures[h.index];
  //   }
  //
  //   // O(1) lookup by string name -> resolved safely through handle
  //   [[nodiscard]] Texture*
  //   find(String name) noexcept
  //   {
  //     const TextureHandle* h = by_name.get(name);
  //     return h ? get(*h) : nullptr;
  //   }
  //
  //   bool
  //   unload(TextureHandle h) noexcept
  //   {
  //     Texture* tex = get(h);
  //     if (!tex) return false;
  //
  //     by_name.remove(tex->name);
  //     generations[h.index] += 1; // Invalidates all old handles pointing to `h.index`
  //     if (generations[h.index] == 0) generations[h.index] = 1; // Skip 0 (reserved for null)
  //     *tex = Texture{};
  //     return true; // (And push `h.index` to `free_slots` when passing `arena`)
  //   }
  // };

} // namespace onyx
