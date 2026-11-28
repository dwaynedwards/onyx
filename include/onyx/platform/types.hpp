//
// Created by Dwayne Edwards on 2026-11-27.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>


namespace onyx
{
  using byte = std::byte;

  using u8  = std::uint8_t;
  using u16 = std::uint16_t;
  using u32 = std::uint32_t;
  using u64 = std::uint64_t;

  using i8  = std::int8_t;
  using i16 = std::int16_t;
  using i32 = std::int32_t;
  using i64 = std::int64_t;

  using f32 = float;
  using f64 = double;

  using b32 = i32;

  using usize = std::size_t;
  using isize = std::ptrdiff_t;
  using uptr  = std::uintptr_t;
  using iptr  = std::intptr_t;

  static_assert(sizeof(byte) == 1, "Expected unsigned 8-bit byte");

  static_assert(sizeof(u8) == 1 && std::is_unsigned_v<u8>, "Expected unsigned 8-bit u8");
  static_assert(sizeof(u16) == 2 && std::is_unsigned_v<u16>, "Expected unsigned 16-bit u16");
  static_assert(sizeof(u32) == 4 && std::is_unsigned_v<u32>, "Expected unsigned 32-bit u32");
  static_assert(sizeof(u64) == 8 && std::is_unsigned_v<u64>, "Expected unsigned 64-bit u64");

  static_assert(sizeof(i8) == 1 && std::is_signed_v<i8>, "Expected signed 8-bit i8");
  static_assert(sizeof(i16) == 2 && std::is_signed_v<i16>, "Expected signed 16-bit i16");
  static_assert(sizeof(i32) == 4 && std::is_signed_v<i32>, "Expected signed 32-bit i32");
  static_assert(sizeof(i64) == 8 && std::is_signed_v<i64>, "Expected signed 64-bit i64");

  static_assert(sizeof(f32) == 4, "Expected 32-bit f32");
  static_assert(sizeof(f64) == 8, "Expected 64-bit f64");

  static_assert(sizeof(usize) == 8 && std::is_unsigned_v<usize>, "Expected unsigned 64-bit usize");
  static_assert(sizeof(isize) == 8 && std::is_signed_v<isize>, "Expected signed 64-bit isize");
  static_assert(sizeof(uptr) == 8 && std::is_unsigned_v<uptr>, "Expected unsigned 64-bit uptr");
  static_assert(sizeof(iptr) == 8 && std::is_signed_v<iptr>, "Expected unsigned 64-bit iptr");

  static_assert(sizeof(usize) == sizeof(void*), "Expected size of pointer usize");
  static_assert(sizeof(isize) == sizeof(void*), "Expected size of pointer isize");
  static_assert(sizeof(uptr) == sizeof(void*), "Expected size of pointer uptr");
} // namespace onyx
