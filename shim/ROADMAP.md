# Linux Shim Roadmap

## Phase 1: Compile & Link (COMPLETE)

**Goal:** Get the entire Mayhem firmware to compile, link, and run on x86_64 Linux.

**Status:** Done. The `portapack-linux` binary starts up, initializes the full
firmware stack (UI system, navigation, theme, event dispatcher), runs the
event loop, and shuts down cleanly on SIGTERM.

**Deliverables:**
- [x] ChibiOS RTOS shim (`ch.h` + `chibios_shim.cpp`) — pthreads-based
- [x] HAL shim (`hal.h` + `hal_shim.cpp`) — LPC43xx register stubs
- [x] FatFs shim (`ff.h` + `fatfs_shim.cpp`) — POSIX filesystem bridge
- [x] Hardware stubs (`hardware_stubs.cpp`) — radio, audio, LCD, touch, I2C
- [x] Build system (`CMakeLists.txt`) — compiles ~100 firmware source files
- [x] Linux entry point (`main_linux.cpp`) — signal handling, arg parsing
- [x] Firmware portability patches — 9 minimal fixes for ARM/x86_64 differences

---

## Phase 2: Display Rendering

**Goal:** See the PortaPack UI on screen.

**Approach:** Replace the LCD stub methods in `lcd::ILI9341` with a framebuffer
that renders to either an SDL2 window (local) or a WebSocket stream (remote).

**Tasks:**
- [ ] Create a 240x320 RGB565 framebuffer in memory
- [ ] Implement `ILI9341::fill_rectangle`, `draw_pixel`, `draw_bitmap`,
      `draw_glyph`, `render_line` to write into framebuffer
- [ ] Add SDL2 backend: open a window, blit framebuffer at 30fps
- [ ] Add WebSocket backend: stream framebuffer as PNG/raw frames
- [ ] Verify UI renders correctly (status bar, navigation, app views)

**Key files to modify:**
- `shim/drivers/hardware_stubs.cpp` — replace LCD no-ops with framebuffer writes
- `shim/CMakeLists.txt` — add SDL2 dependency
- New: `shim/display/framebuffer.cpp` — framebuffer management
- New: `shim/display/sdl2_backend.cpp` — SDL2 window rendering

---

## Phase 3: Filesystem Bridge

**Goal:** Real file I/O on the host filesystem.

**Approach:** The FatFs shim already maps to POSIX, but needs refinement for
full compatibility with the firmware's expectations.

**Tasks:**
- [ ] Verify all FatFs functions work correctly (especially `f_findfirst`/`f_findnext`)
- [ ] Handle TCHAR (UTF-16) to UTF-8 path conversion properly
- [ ] Create default SD card directory structure at `~/.portapack/`
- [ ] Populate with sample data files (frequencies, settings)
- [ ] Test file-heavy apps: Freqman, File Manager, Text Editor, Settings

---

## Phase 4: Input & Control

**Goal:** Interact with the UI using keyboard/mouse or WebSocket commands.

**Approach:** Map keyboard keys to PortaPack buttons and encoder, mouse
clicks to touch events.

**Tasks:**
- [ ] Keyboard input: arrow keys -> D-pad, Enter -> Select, Escape -> Back
- [ ] Encoder: scroll wheel or +/- keys
- [ ] Touch: mouse click position mapped to 240x320 screen coordinates
- [ ] SDL2 event loop integration with ChibiOS event system
- [ ] WebSocket command interface for remote control

**Key mapping:**
```
Arrow keys  -> Left/Right/Up/Down buttons
Enter       -> Select button
Escape      -> DFU/Back button
Mouse wheel -> Encoder rotation
Mouse click -> Touch event at (x, y)
```

---

## Phase 5: Audio Passthrough

**Goal:** Route audio through the host's sound system.

**Approach:** Replace `audio::dma` with ALSA or PulseAudio backend.

**Tasks:**
- [ ] Implement `audio::dma::tx_empty_buffer()` backed by ALSA/PulseAudio
- [ ] Implement `audio::dma::rx_buffer()` for microphone input
- [ ] Wire up audio codec virtual methods to host mixer controls
- [ ] Test with analog audio app, mic TX app

---

## Phase 6: SDR Integration (stretch goal)

**Goal:** Connect to real HackRF hardware for RF operations.

**Approach:** Use `libhackrf` to send/receive IQ samples, bridging
to the firmware's `SharedMemory` / `StreamBuffer` mechanism.

**Tasks:**
- [ ] Implement `m4_init` to load baseband processors as host threads
- [ ] Bridge `SharedMemory` messages between "M0" and "M4" threads
- [ ] Connect `libhackrf` for real RF TX/RX
- [ ] Test with receive apps (analog audio, ADS-B, POCSAG)

---

## Non-Goals

- **Cycle-accurate ARM emulation** — We're not emulating the MCU; we're
  running the application logic natively.
- **Real-time guarantees** — Linux is not an RTOS. Timing-sensitive
  baseband processing may not work identically.
- **External app loading** — The `.ppma` external app format is ARM binary;
  these can't run on x86_64 without emulation.
