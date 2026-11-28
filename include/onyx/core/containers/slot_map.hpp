//
// Created by Dwayne Edwards on 2026-11-28.
//

#pragma once

#include "onyx/core/containers/list.hpp"
#include "onyx/core/handle.hpp"
#include "onyx/core/memory/arena.hpp"
#include "onyx/platform/types.hpp"

#include <cassert>
#include <span>
#include <type_traits>


namespace onyx
{
  template <typename T, typename Tag = T>
  struct SlotMap
  {
    static_assert(std::is_trivially_copyable_v<T>, "SlotMap<T, Tag> requires trivially copyable types");

    using HandleType = Handle<Tag>;

    static constexpr u32 INVALID_INDEX = UINT32_MAX;

    struct Slot
    {
      u32 dense_or_next = INVALID_INDEX; // Dense index when occupied; next free slot index when free
      u32 generation    = 1; // Always >= 1 (0 is reserved for null handles)
    };

    List<Slot> slots     = {}; // Indexed by Handle::index
    List<T>    data      = {}; // Tightly packed live elements (0 .. len - 1)
    List<u32>  erase_map = {}; // Parallel to `data`: maps dense index -> slot index
    u32        free_head = INVALID_INDEX; // Intrusive O(1) free-list head

    [[nodiscard]] constexpr usize
    len() const noexcept
    {
      return data.len;
    }

    [[nodiscard]] constexpr bool
    is_empty() const noexcept
    {
      return data.is_empty();
    }

    bool
    reserve(Arena* arena, usize min_cap) noexcept
    {
      return slots.reserve(arena, min_cap) && data.reserve(arena, min_cap) && erase_map.reserve(arena, min_cap);
    }

    [[nodiscard]] HandleType
    insert(Arena* arena, const T& value) noexcept
    {
      if (!arena) return {};
      if (data.len >= INVALID_INDEX) [[unlikely]]
      {
        return {};
      }

      const u32 dense_idx = static_cast<u32>(data.len);
      if (!data.push(arena, value)) [[unlikely]]
      {
        return {};
      }

      if (free_head != INVALID_INDEX)
      {
        const u32 slot_idx  = free_head;
        const u32 next_free = slots[slot_idx].dense_or_next;

        if (!erase_map.push(arena, slot_idx)) [[unlikely]]
        {
          (void)data.pop();
          return {};
        }

        free_head                     = next_free;
        slots[slot_idx].dense_or_next = dense_idx;
        return HandleType{.index = slot_idx, .generation = slots[slot_idx].generation};
      }

      if (slots.len >= INVALID_INDEX) [[unlikely]]
      {
        (void)data.pop();
        return {};
      }

      const u32 slot_idx = static_cast<u32>(slots.len);
      if (!slots.push(arena, Slot{.dense_or_next = dense_idx, .generation = 1})) [[unlikely]]
      {
        (void)data.pop();
        return {};
      }

      if (!erase_map.push(arena, slot_idx)) [[unlikely]]
      {
        (void)slots.pop();
        (void)data.pop();
        return {};
      }

      return HandleType{.index = slot_idx, .generation = 1};
    }

    [[nodiscard]] T*
    get(HandleType h) noexcept
    {
      if (!h.is_valid() || h.index >= slots.len) return nullptr;
      const Slot& slot = slots[h.index];
      if (slot.generation != h.generation || slot.dense_or_next >= data.len) return nullptr;
      return &data[slot.dense_or_next];
    }

    [[nodiscard]] const T*
    get(HandleType h) const noexcept
    {
      return const_cast<SlotMap*>(this)->get(h);
    }

    [[nodiscard]] bool
    contains(HandleType h) const noexcept
    {
      return get(h) != nullptr;
    }

    [[nodiscard]] HandleType
    handle_at_dense(usize dense_idx) const noexcept
    {
      assert(dense_idx < data.len);
      const u32 slot_idx = erase_map[dense_idx];
      return HandleType{.index = slot_idx, .generation = slots[slot_idx].generation};
    }

    bool
    remove(HandleType h) noexcept
    {
      if (!h.is_valid() || h.index >= slots.len) return false;

      Slot& slot = slots[h.index];
      if (slot.generation != h.generation || slot.dense_or_next >= data.len) return false;

      const u32 removed_dense = slot.dense_or_next;
      const u32 last_dense    = static_cast<u32>(data.len - 1);

      if (removed_dense != last_dense)
      {
        const u32 moved_slot_idx            = erase_map[last_dense];
        slots[moved_slot_idx].dense_or_next = removed_dense;
      }

      data.pop_swap(removed_dense);
      erase_map.pop_swap(removed_dense);

      slot.generation += 1;
      if (slot.generation == 0)
      {
        slot.generation = 1;
      }
      slot.dense_or_next = free_head;
      free_head          = h.index;
      return true;
    }

    void
    clear() noexcept
    {
      for (usize i = 0; i < erase_map.len; ++i)
      {
        const u32 slot_idx = erase_map[i];
        slots[slot_idx].generation += 1;
        if (slots[slot_idx].generation == 0)
        {
          slots[slot_idx].generation = 1;
        }
        slots[slot_idx].dense_or_next = free_head;
        free_head                     = slot_idx;
      }
      data.clear();
      erase_map.clear();
    }

    T*
    begin() noexcept
    {
      return data.begin();
    }

    T*
    end() noexcept
    {
      return data.end();
    }

    const T*
    begin() const noexcept
    {
      return data.begin();
    }

    const T*
    end() const noexcept
    {
      return data.end();
    }

    [[nodiscard]] std::span<T>
    span() noexcept
    {
      return data.span();
    }

    [[nodiscard]] std::span<const T>
    span() const noexcept
    {
      return data.span();
    }
  };

  //
  // Example
  //
  // struct EntityTag {};
  // using EntityHandle = Handle<EntityTag>;
  //
  // struct Entity
  // {
  //   f32          x      = 0.0f;
  //   f32          y      = 0.0f;
  //   f32          vx     = 0.0f;
  //   f32          vy     = 0.0f;
  //   f32          health = 100.0f;
  //   EntityHandle target = EntityHandle::invalid(); // Safe reference to another entity!
  // };
  //
  // struct EntityWorld
  // {
  //   SlotMap<Entity, EntityTag> entities = {};
  //
  //   [[nodiscard]] EntityHandle
  //   spawn(Arena* arena, f32 x, f32 y, f32 vx = 0.0f, f32 vy = 0.0f) noexcept
  //   {
  //     return entities.insert(arena, Entity{.x = x, .y = y, .vx = vx, .vy = vy});
  //   }
  //
  //   void
  //   destroy(EntityHandle h) noexcept
  //   {
  //     (void)entities.remove(h);
  //   }
  //
  //   void
  //   tick(f32 dt) noexcept
  //   {
  //     // 1. Dense, cache-friendly physics & AI pass over ONLY live entities
  //     for (Entity& e : entities)
  //     {
  //       e.x += e.vx * dt;
  //       e.y += e.vy * dt;
  //
  //       // Safely check if `e.target` is still alive without dangling pointers!
  //       if (e.target.is_valid())
  //       {
  //         if (const Entity* target = entities.get(e.target))
  //         {
  //           // Chase `target->x, target->y`...
  //           (void)target;
  //         }
  //         else
  //         {
  //           // Target was destroyed in an earlier frame—clear the stale handle cleanly
  //           e.target = EntityHandle::invalid();
  //         }
  //       }
  //     }
  //
  //     // 2. Clean up dead entities while iterating backwards over dense slots
  //     for (usize i = entities.len(); i > 0; --i)
  //     {
  //       const usize dense_idx = i - 1;
  //       if (entities.data[dense_idx].health <= 0.0f)
  //       {
  //         EntityHandle dead_handle = entities.handle_at_dense(dense_idx);
  //         entities.remove(dead_handle);
  //       }
  //     }
  //   }
  // };
} // namespace onyx
