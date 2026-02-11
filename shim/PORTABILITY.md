# ARM-to-x86_64 Portability Notes

This document catalogs the portability issues encountered while compiling the
PortaPack Mayhem firmware (originally targeting ARM Cortex-M0/M4) on x86_64
Linux, and the solutions applied.

## 1. `size_t` Width Difference

**Problem:** On ARM (ILP32), `size_t` is `unsigned int` (4 bytes). On x86_64
(LP64), `size_t` is `unsigned long` (8 bytes). C++ name mangling encodes the
parameter types, so a function declared with `size_t` but stubbed with
`uint32_t` produces a different mangled symbol on x86_64.

**Symptom:** Undefined reference errors where the mangled name contains `m`
(unsigned long) but the stub provides `j` (unsigned int).

**Fix:** Match parameter types exactly to the declaration. When the header
uses `size_t`, the stub must also use `size_t`.

**Affected areas:**
- `audio::debug::reg_read(const size_t)`
- `radio::debug::first_if::register_read(const size_t)`
- `radio::debug::second_if::register_read(const size_t)`

## 2. `std::max` / `std::min` Template Deduction

**Problem:** `std::max(a, b)` requires both arguments to have the same type.
On ARM, `size_t == uint32_t` so `std::max(1, some_size_t)` works. On x86_64,
`size_t != uint32_t`, causing template argument deduction failure.

**Fix:** Use explicit template parameters: `std::max<size_t>(1, value)`.

**Affected files:**
| File | Line | Fix |
|------|------|-----|
| `message.hpp:790` | `std::max<size_t>(1, percent)` | size_t width |
| `ui_widget.cpp:3261` | `std::max<uint32_t>(1, ...)` | deduction failure |
| `iq_trim.cpp:66` | `std::max<uint64_t>(1, ...)` | deduction failure |
| `ui_record_view.cpp:295` | `std::min<size_t>(99, ...)` | deduction failure |
| `ui_debug.cpp:232,498` | `std::max<uint32_t>(0, ...)` | deduction failure |
| `ui_freqman.cpp:147` | `clip<size_t>(...)` | deduction failure |

## 3. Designated Initializers

**Problem:** C++20 designated initializers have restrictions that GCC
enforces differently at `-std=c++20` on x86_64. Mixed designated/positional
initializers and designated initializers on non-aggregate types cause errors.

**Fix:** Use positional initializers or remove the designated syntax.

**Affected files:**
- `message.hpp:1163` — Remove designated initializer on non-aggregate
- `ui_standalone_view.cpp` — Switch to positional initializers

## 4. `reinterpret_cast<uint32_t>(pointer)`

**Problem:** On ARM, pointers are 32-bit and can be cast to `uint32_t`
without data loss. On x86_64, pointers are 64-bit, so this cast truncates
and is an error in standard C++.

**Fix:** Compile with `-fpermissive` to downgrade truncation errors to
warnings. These casts appear in firmware code that computes checksums or
accesses memory-mapped hardware — neither is relevant on Linux.

**Example:** `reinterpret_cast<uint32_t>(&_textend)` in firmware init code.

## 5. `const` Internal Linkage

**Problem:** In C++, `const` variables at namespace scope have internal
linkage by default. If a `.cpp` file defines `const` arrays and another
translation unit references them via `extern` declarations, the linker
cannot find the symbols.

**Fix:** Use `extern const` in the definition to give external linkage.

**Affected:** CPLD configuration arrays in `hardware_stubs.cpp`:
```cpp
// Wrong: internal linkage, invisible to other TUs
const std::array<uint16_t, 896> block_0 = { ... };

// Correct: external linkage
extern const std::array<uint16_t, 896> block_0 = { ... };
```

## 6. Memory-Mapped I/O Pointers

**Problem:** The firmware accesses LPC43xx peripherals through fixed memory
addresses like `LPC_CGU` (defined as `(LPC_CGU_T*)0x40050000`). These are
invalid pointers on Linux.

**Fix:** Define dummy struct instances in the BSS segment and point the
macros to those:
```cpp
static LPC_CGU_T lpc_cgu_instance;
#define LPC_CGU (&lpc_cgu_instance)
```

Register reads return zero; writes are silently discarded.

## 7. `__I` Volatile Qualifier

**Problem:** ARM CMSIS defines `__I` as `volatile const` for read-only
registers. This prevents default construction of register structs needed
for the dummy instances.

**Fix:** Define `__I` as just `volatile` (dropping `const`) in the shim's
`hal.h`. Since the dummy registers are never actually connected to hardware,
the const qualifier serves no purpose.

## 8. SPI Flash Memory Access

**Problem:** `simple_checksum()` takes a `uint32_t buffer_address` and reads
memory at that address. On ARM, this accesses SPI flash. On Linux, address 0
causes a segfault.

**Fix:** Guard with `#ifdef LINUX_SHIM` to return 0 (no flash to checksum):
```cpp
uint32_t simple_checksum(uint32_t buffer_address, uint32_t length) {
#ifdef LINUX_SHIM
    (void)buffer_address; (void)length;
    return 0;
#else
    // ... original implementation ...
#endif
}
```

## 9. vtable Emission for Stub Classes

**Problem:** When stubbing out classes with virtual methods, the vtable is
emitted in the translation unit that defines the *first non-inline virtual
method*. If you re-define a class locally in a stub file instead of
implementing methods from the real header, the mangled vtable symbol differs.

**Fix:** Always `#include` the real firmware header and implement the virtual
methods from that declaration. Never re-define the class in the stub file.

**Affected classes:**
- `i2cdev::I2cDev_ADS1110`, `I2cDev_BH1750`, `I2cDev_BMX280`, `I2cDev_SHT3x`, `I2cDev_SHT4x`
- `CaptureThread`, `ReplayThread`, `UsbSerialThread`
- `BufferExchange`
- `ui::SDCardDebugView`

## 10. Compiler Flags Summary

Flags added for x86_64 compatibility:

| Flag | Purpose |
|------|---------|
| `-DLINUX_SHIM=1` | Gates Linux-specific code paths |
| `-fpermissive` | Allows pointer-to-int truncation (ARM legacy) |
| `-Wno-narrowing` | Suppresses narrowing conversion warnings |
| `-Wno-address-of-packed-member` | Packed struct warnings irrelevant on Linux |
| `-Wno-unused-variable` | Noise reduction for stubbed-out code paths |

## General Principles

1. **Match types exactly.** When a header says `size_t`, don't use `uint32_t`.
2. **Include real headers.** Don't re-declare classes in stub files.
3. **Guard, don't delete.** Use `#ifdef LINUX_SHIM` to skip hardware access.
4. **Minimize patches.** Each firmware modification is a merge conflict risk.
   Only patch when there's no shim-side workaround.
5. **Test the build on both targets.** A fix for x86_64 must not break ARM.
