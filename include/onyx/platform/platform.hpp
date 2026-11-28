//
// Created by Dwayne Edwards on 2026-11-27.
//

#pragma once

#include <string_view>

#include "types.hpp"


#define ONYX_COMPILER_CLANG 0
#define ONYX_COMPILER_GCC   0
#define ONYX_COMPILER_MSC   0

#define ONYX_OS_WINDOWS 0
#define ONYX_OS_LINUX   0
#define ONYX_OS_MACOS   0
#define ONYX_OS_ANDROID 0
#define ONYX_OS_IOS     0

#define ONYX_ARCH_X64   0
#define ONYX_ARCH_X86   0
#define ONYX_ARCH_ARM64 0
#define ONYX_ARCH_ARM32 0

#if defined(__clang__)
  #undef ONYX_COMPILER_CLANG
  #define ONYX_COMPILER_CLANG 1

  // OS
  #if defined(_WIN32)
    #undef ONYX_OS_WINDOWS
    #define ONYX_OS_WINDOWS 1
  #elif defined(__ANDROID__)
    #undef ONYX_OS_ANDROID
    #define ONYX_OS_ANDROID 1
  #elif defined(__linux__)
    #undef ONYX_OS_LINUX
    #define ONYX_OS_LINUX 1
  #elif defined(__APPLE__) && defined(__MACH__)
    #if defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
      #undef ONYX_OS_IOS
      #define ONYX_OS_IOS 1
    #elif defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
      #undef ONYX_OS_MACOS
      #define ONYX_OS_MACOS 1
    #endif
  #endif

  // Architecture
  #if defined(__x86_64__)
    #undef ONYX_ARCH_X64
    #define ONYX_ARCH_X64 1
  #elif defined(__i386__)
    #undef ONYX_ARCH_X86
    #define ONYX_ARCH_X86 1
  #elif defined(__aarch64__) || defined(__arm64__)
    #undef ONYX_ARCH_ARM64
    #define ONYX_ARCH_ARM64 1
  #elif defined(__arm__)
    #undef ONYX_ARCH_ARM32
    #define ONYX_ARCH_ARM32 1
  #endif

#elif defined(__GNUC__)
  #undef ONYX_COMPILER_GCC
  #define ONYX_COMPILER_GCC 1

  // OS
  #if defined(_WIN32)
    #undef ONYX_OS_WINDOWS
    #define ONYX_OS_WINDOWS 1 // MinGW / Cygwin
  #elif defined(__ANDROID__)
    #undef ONYX_OS_ANDROID
    #define ONYX_OS_ANDROID 1
  #elif defined(__linux__)
    #undef ONYX_OS_LINUX
    #define ONYX_OS_LINUX 1
  #endif

  // Architecture
  #if defined(__x86_64__)
    #undef ONYX_ARCH_X64
    #define ONYX_ARCH_X64 1
  #elif defined(__i386__)
    #undef ONYX_ARCH_X86
    #define ONYX_ARCH_X86 1
  #elif defined(__aarch64__)
    #undef ONYX_ARCH_ARM64
    #define ONYX_ARCH_ARM64 1
  #elif defined(__arm__)
    #undef ONYX_ARCH_ARM32
    #define ONYX_ARCH_ARM32 1
  #endif

#elif defined(_MSC_VER)
  #undef ONYX_COMPILER_MSC
  #define ONYX_COMPILER_MSC 1

  // OS
  #if defined(_WIN32)
    #undef ONYX_OS_WINDOWS
    #define ONYX_OS_WINDOWS 1
  #endif

  // Architecture
  #if defined(_M_X64)
    #undef ONYX_ARCH_X64
    #define ONYX_ARCH_X64 1
  #elif defined(_M_IX86)
    #undef ONYX_ARCH_X86
    #define ONYX_ARCH_X86 1
  #elif defined(_M_ARM64)
    #undef ONYX_ARCH_ARM64
    #define ONYX_ARCH_ARM64 1
  #elif defined(_M_ARM)
    #undef ONYX_ARCH_ARM32
    #define ONYX_ARCH_ARM32 1
  #endif
#endif

// =========================================================
// Compiler Attribute Macros
// =========================================================
#if ONYX_COMPILER_MSC
  #define ONYX_ALWAYS_INLINE __forceinline
  #define ONYX_NOINLINE      __declspec(noinline)
  #define ONYX_PRINTF_FMT(fmt_idx, arg_idx)
#elif ONYX_COMPILER_CLANG || ONYX_COMPILER_GCC
  #define ONYX_ALWAYS_INLINE                [[gnu::always_inline]] inline
  #define ONYX_NOINLINE                     [[gnu::noinline]]
  #define ONYX_PRINTF_FMT(fmt_idx, arg_idx) [[gnu::format(printf, fmt_idx, arg_idx)]]
#else
  #define ONYX_ALWAYS_INLINE inline
  #define ONYX_NOINLINE
  #define ONYX_PRINTF_FMT(fmt_idx, arg_idx)
#endif


namespace onyx
{

  enum class Compiler : u8 { UNKNOWN = 0, MSC, CLANG, GCC };

  enum class Platform : u8 { UNKNOWN = 0, WINDOWS, LINUX, MACOS, ANDROID, IOS };

  enum class Arch : u8 { UNKNOWN = 0, X64, X86, ARM64, ARM32 };

#if ONYX_COMPILER_CLANG
  inline constexpr Compiler COMPILER = Compiler::CLANG;
#elif ONYX_COMPILER_GCC
  inline constexpr Compiler COMPILER = Compiler::GCC;
#elif ONYX_COMPILER_MSC
  inline constexpr Compiler COMPILER = Compiler::MSC;
#else
  inline constexpr Compiler COMPILER = Compiler::UNKNOWN;
#endif

#if ONYX_OS_WINDOWS
  inline constexpr Platform OS = Platform::WINDOWS;
#elif ONYX_OS_ANDROID
  inline constexpr Platform OS = Platform::ANDROID;
#elif ONYX_OS_LINUX
  inline constexpr Platform OS = Platform::LINUX;
#elif ONYX_OS_MACOS
  inline constexpr Platform OS = Platform::MACOS;
#elif ONYX_OS_IOS
  inline constexpr Platform OS = Platform::IOS;
#else
  inline constexpr Platform OS = Platform::UNKNOWN;
#endif

#if ONYX_ARCH_X64
  inline constexpr Arch ARCH = Arch::X64;
#elif ONYX_ARCH_X86
  inline constexpr Arch ARCH = Arch::X86;
#elif ONYX_ARCH_ARM64
  inline constexpr Arch ARCH = Arch::ARM64;
#elif ONYX_ARCH_ARM32
  inline constexpr Arch ARCH = Arch::ARM32;
#else
  inline constexpr Arch ARCH = Arch::UNKNOWN;
#endif

  static_assert(COMPILER != Compiler::UNKNOWN, "Unsupported compiler detected.");
  static_assert(OS != Platform::UNKNOWN, "Unsupported operating system detected.");
  static_assert(ARCH != Arch::UNKNOWN, "Unsupported CPU architecture detected.");

  [[nodiscard]] constexpr std::string_view
  to_string(Compiler compiler) noexcept
  {
    using enum Compiler;
    switch (compiler)
    {
      case UNKNOWN: return "Unknown";
      case MSC: return "MSVC";
      case CLANG: return "Clang";
      case GCC: return "GCC";
    }
    return "Unknown";
  }

  [[nodiscard]] constexpr std::string_view
  to_string(Platform os) noexcept
  {
    using enum Platform;
    switch (os)
    {
      case UNKNOWN: return "Unknown";
      case WINDOWS: return "Windows";
      case LINUX: return "Linux";
      case MACOS: return "macOS";
      case ANDROID: return "Android";
      case IOS: return "iOS";
    }
    return "Unknown";
  }

  [[nodiscard]] constexpr std::string_view
  to_string(Arch arch) noexcept
  {
    using enum Arch;
    switch (arch)
    {
      case UNKNOWN: return "Unknown";
      case X64: return "x64";
      case X86: return "x86";
      case ARM64: return "ARM64";
      case ARM32: return "ARM32";
    }
    return "Unknown";
  }

} // namespace onyx
