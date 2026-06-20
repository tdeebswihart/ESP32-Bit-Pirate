# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This project uses PlatformIO with the **pioarduino** fork of platform-espressif32 (not the official
PlatformIO espressif32 package). Do not switch to the official platform — it will break the build.

```bash
# Build for a specific target
pio run -e cardputer-adv

# Build and upload
pio run -e cardputer-adv --target upload

# Serial monitor (115200 baud, includes exception decoder)
pio device monitor -e cardputer-adv

# Common environments: cardputer, cardputer-adv, cardputer-adv-ili9341,
#   m5stack-sticks3, s3-devkit, s3-devkit-n16-r8, m5stack-stamps3,
#   atom-lite-s3, t-display-s3, waveshare-s3-geek, t-embed-s3,
#   t-embed-s3-cc1101, t-embed-s3-cc1101plus, xiao-esp32s3,
#   heltec_wifi_lora_32_V3
```

There are no unit tests and no linting step. `patch_osc_lib.py` runs automatically at build time
to fix `handleOsc`→`handleOSC` naming in the cnmat/OSC library.

## Architecture

### Runtime data flow

```
TerminalInput → ActionDispatcher → Controller → Service → Controller → TerminalView
```

`ActionDispatcher::run()` is the forever loop. `loop()` in `main.cpp` is intentionally empty.
`ActionDispatcher` also handles `|` pipelines, repeat sequences, and bytecode instruction vectors.

### Key objects

| Class | Role |
|---|---|
| `DependencyProvider` | Manual DI container — owns all services, controllers, transformers, managers, shells, selectors. Always heap-allocated (`new DependencyProvider(...)`) — too large for the stack. |
| `ActionDispatcher` | Central event loop. Reads input, dispatches to commands/instructions/pipelines. |
| `GlobalState` | Meyer's singleton. All runtime-mutable pin assignments, baud rates, frequencies, and mode flags. Defaults set from `#define` macros in `platformio.ini`; modified at runtime via setters. |

### Layer responsibilities

- **Services** (`src/Services/`) — hardware access only. No output, no text parsing. ~35 services.
- **Controllers** (`src/Controllers/`) — parse `TerminalCommand`, call services, write results to
  `ITerminalView`. One controller per protocol/mode.
- **Transformers** (`src/Transformers/`) — parse raw text into typed model objects
  (`TerminalCommand`, `Instruction` vector, etc.).
- **Shells** (`src/Shells/`) — multi-step interactive sub-modes that take over the input loop
  (e.g., `SpiFlashShell`, `I2cEepromShell`).
- **Adapters** (`src/Adapters/`) — emulate PC-side tool protocols over USB CDC (avrdude/BusPirate,
  flashrom/serprog, OpenOCD, Infrared Toy, SUMP logic analyser, etc.).
- **Views** (`src/Views/`) — implement `ITerminalView` or `IDeviceView`. Terminal views render CLI
  text; device views render to the physical screen (mode display, logic traces, waterfall).
- **Inputs** (`src/Inputs/`) — implement `IInput`. Per-device keyboard/button drivers.
- **Config** (`src/Config/`) — boot-time configurators: terminal type, WiFi mode, USB adapter mode.

### Adding a new protocol

1. Add entry to `ModeEnum` in `src/Enums/ModeEnum.h`
2. Create `src/Services/XxxService.h/.cpp` (hardware only)
3. Create `src/Controllers/XxxController.h/.cpp` (CLI + output)
4. Register both in `src/Providers/DependencyProvider.h/.cpp`
5. Wire into `ActionDispatcher` dispatch table
6. Add `#define` pin defaults to relevant `platformio.ini` environments and `GlobalState`

## Device / Pin Configuration

All pin assignments live in `platformio.ini` as `-D` build flags per environment and are read by
`GlobalState` at construction. They are `#ifndef`-guarded in source, so individual pins can be
overridden by adding flags to a custom environment without touching source files.

The `DEVICE_CARDPUTERADV`, `DEVICE_ILI9341_SCREEN`, `DEVICE_STICKS3`, `DEVICE_TEMBEDS3`, etc.
preprocessor macros gate device-specific code throughout `main.cpp` and the Views/Inputs layers.

`DEVICE_HOST_SERIAL_UART` switches the host serial from USB-CDC (`DefaultHostSerial`) to hardware
UART (`UartHostSerial`) — needed for boards like the Heltec LoRa 32 V3 that use a CP210x bridge.
