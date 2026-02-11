# Linux Shim Architecture

## Overview

The Linux shim layer enables the PortaPack Mayhem firmware to compile and run
on x86_64 Linux. It replaces hardware-dependent subsystems (ChibiOS RTOS,
LPC43xx registers, FatFs filesystem) with Linux-native equivalents while
preserving the entire application logic, UI framework, and radio API unchanged.

## Design Principle

**Header interception via `-I` ordering.** The shim's `include/` directory is
added to the compiler include path *before* the firmware directories. When
firmware code writes `#include "ch.h"`, it picks up our shim `ch.h` instead of
the real ChibiOS header. No firmware source modifications are needed for this
mechanism.

```
shim/include/ch.h    --> intercepts ChibiOS RTOS
shim/include/hal.h   --> intercepts ChibiOS HAL + LPC43xx registers
shim/include/ff.h    --> intercepts FatFs filesystem
shim/include/ffconf.h --> intercepts FatFs configuration
```

## Layer Diagram

```
+----------------------------------------------------------+
|                   Application Logic                       |
|  (ui_navigation, apps/*, receiver_model, transmitter_model)|
+----------------------------------------------------------+
|                   UI Framework                            |
|  (ui_widget, ui_painter, ui_text, ui_menu, theme)         |
+----------------------------------------------------------+
|              Common / Protocol Libraries                  |
|  (message, adsb, pocsag, ais, morse, string_format)       |
+----------------------------------------------------------+
         |              |              |              |
    +---------+   +---------+   +---------+   +---------+
    | ChibiOS |   |  HAL /  |   |  FatFs  |   | Display |
    |  RTOS   |   | LPC43xx |   |   FS    |   |  LCD    |
    +---------+   +---------+   +---------+   +---------+
         |              |              |              |
    +---------+   +---------+   +---------+   +---------+
    |pthreads |   | Dummy   |   | POSIX   |   | Stub /  |
    |mutexes  |   | register|   | fopen   |   | SDL2    |
    |semaphores|  | structs |   | fread   |   | (Phase2)|
    +---------+   +---------+   +---------+   +---------+
```

## File Structure

```
shim/
  CMakeLists.txt            -- Build system (cmake)
  main_linux.cpp            -- Linux entry point, signal handling
  ARCHITECTURE.md           -- This file
  ROADMAP.md                -- Phase plan and status
  PORTABILITY.md            -- ARM-to-x86_64 porting notes

  include/
    ch.h                    -- ChibiOS RTOS replacement (pthreads)
    hal.h                   -- HAL + LPC43xx register type stubs
    ff.h                    -- FatFs API (backed by POSIX I/O)
    ffconf.h                -- FatFs configuration

  platform/
    chibios_shim.cpp        -- Thread, mutex, semaphore implementations
    chibios_cpp_shim.cpp    -- C++ new/delete via malloc/free
    hal_shim.cpp            -- SDC driver, LPC instances, HAL stubs
    fatfs_shim.cpp          -- FatFs -> POSIX filesystem bridge
    shared_memory_shim.cpp  -- Inter-core IPC (single-process on Linux)

  drivers/
    hardware_stubs.cpp      -- ~950 lines: radio, audio, LCD, touch,
                               i2cdev, battery, baseband, CPLD stubs
```

## Shim Components

### 1. ChibiOS RTOS (`ch.h` + `chibios_shim.cpp`)

Provides ChibiOS-compatible API using POSIX primitives:

| ChibiOS Concept | Linux Implementation |
|-----------------|---------------------|
| `Thread`        | `pthread_t` + event mutex/condvar |
| `Mutex`         | `pthread_mutex_t` |
| `Semaphore`     | `sem_t` (POSIX semaphore) |
| `chEvtWaitAny`  | `pthread_cond_wait` with event bitmask |
| `chThdSleep`    | `usleep()` |
| `chSysLock/Unlock` | Global pthread mutex |
| `chHeapAlloc/Free` | `malloc/free` |
| `systime_t`     | `clock_gettime(CLOCK_MONOTONIC)` scaled to 1kHz |

### 2. HAL + LPC43xx Registers (`hal.h` + `hal_shim.cpp`)

The firmware accesses LPC43xx peripheral registers through memory-mapped
pointers like `LPC_CGU`, `LPC_GPIO`, etc. On Linux, these point to dummy
struct instances in RAM. Register writes are silently discarded; reads
return zero.

Key design choice: `__I` is defined as `volatile` (not `volatile const`)
so that LPC register structs can be default-constructed in C++.

### 3. FatFs Filesystem (`ff.h` + `fatfs_shim.cpp`)

FatFs API calls are mapped to POSIX file operations:

| FatFs | POSIX |
|-------|-------|
| `f_open` | `fopen` |
| `f_read` | `fread` |
| `f_write` | `fwrite` |
| `f_close` | `fclose` |
| `f_lseek` | `fseek` |
| `f_stat` | `stat` |
| `f_opendir` | `opendir` |
| `f_readdir` | `readdir` |
| `f_mkdir` | `mkdir` |
| `f_unlink` | `remove` |
| `f_rename` | `rename` |

The SD card root maps to `~/.portapack/` on Linux.

### 4. Hardware Stubs (`hardware_stubs.cpp`)

Provides no-op or minimal implementations for all hardware-touching code:

- **LCD** (`lcd::ILI9341`): All drawing methods are no-ops (Phase 2 will add framebuffer)
- **Radio** (`radio::` namespace): All RF control functions are no-ops
- **Audio** (`audio::` namespace): Volume/codec control stubs
- **Touch** (`touch::` namespace): Returns null touch frames
- **I2C devices**: Stub implementations for MAX17055, PPmod, ADS1110, BH1750, BMX280, SHT3x, SHT4x
- **Battery**: Always reports "no battery detected"
- **Baseband**: `run_image`/`set_spectrum` are no-ops (no M4 core)
- **CPLD**: Empty configuration data arrays

## Firmware Patches

Minimal changes to upstream firmware files, all either guarded by
`#ifdef LINUX_SHIM` or fixing genuine 64-bit portability bugs:

| File | Change | Reason |
|------|--------|--------|
| `message.hpp:790` | `std::max<size_t>(1, percent)` | `size_t` width differs ARM vs x86_64 |
| `message.hpp:1163` | Remove designated initializer | C++20 non-aggregate restriction |
| `ui_widget.cpp:3261` | `std::max<uint32_t>(1, ...)` | Template deduction failure |
| `iq_trim.cpp:66` | `std::max<uint64_t>(1, ...)` | Template deduction failure |
| `ui_record_view.cpp:295` | `std::min<size_t>(99, ...)` | Template deduction failure |
| `ui_debug.cpp:232,498` | `std::max<uint32_t>(0, ...)` | Template deduction failure |
| `ui_freqman.cpp:147` | `clip<size_t>(...)` | Template deduction failure |
| `ui_standalone_view.cpp` | Positional initializers | Mixed designated/positional not allowed |
| `utility.cpp:259` | `#ifdef LINUX_SHIM` skip checksum | No SPI flash on Linux |

## Build System

```bash
cd shim/build
cmake ..
make -j$(nproc)
./portapack-linux [--sd-root PATH] [--port PORT] [--verbose]
```

The CMakeLists.txt compiles ~100 firmware source files organized into:
- `SHIM_SOURCES` — Shim platform code
- `COMMON_SOURCES` — `firmware/common/` (protocols, UI base, utilities)
- `APP_SOURCES` — `firmware/application/` (models, settings, file I/O)
- `UI_SOURCES` — `firmware/application/ui/` (widgets, menus)
- `APPS_SOURCES` — `firmware/application/apps/` (all app views)
- `PROTOCOL_SOURCES` — `firmware/application/protocols/` (APRS, RDS, etc.)
