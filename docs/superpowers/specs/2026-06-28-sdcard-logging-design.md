# SD Card Logging — Design Spec

**Date:** 2026-06-28

## Overview

Hook the ESP32 SDK's vprintf logging output to an SD card file. Logging lives in a dedicated
`LOG` mode. A 512-byte accumulation buffer flushes to SD on newline or when full, keeping SD
writes coarse-grained. Files rotate on each `begin()` call — once at boot (automatic on Cardputer
ADV) and once each time the user runs `on` after a prior `off`. This means running `on/off/on`
within a session rotates twice, consuming two slots. Keeping the four most recent log files is
the ceiling; the user controls `on`/`off`. Head/tail commands allow reading log files from the CLI.

---

## Architecture

New components:
- `src/Services/LogService.h/.cpp`
- `src/Controllers/LogController.h/.cpp`

Existing components modified:
- `src/Enums/ModeEnum.h` — add `LOG`
- `src/Providers/DependencyProvider.h/.cpp` — add `LogService`, `LogController`
- `src/Dispatchers/ActionDispatcher.cpp` — wire dispatch, auto-start on internal SD

---

## LogService

**Single responsibility:** manage the vprintf hook, the open file handle, and the ring buffer.
No terminal output, no command parsing.

### Public API

```cpp
LogService(SdService& sd)

void begin()       // rotate, open boot0.log, register vprintf hook
void end()         // flush, close file, restore original vprintf
bool isEnabled() const

size_t fileSize(uint8_t slot)                              // slot 0..3
std::string readChunk(uint8_t slot, size_t offset, size_t maxBytes)
```

### File layout

```
/logs/boot0.log   ← current boot (written to)
/logs/boot1.log   ← previous boot
/logs/boot2.log   ← two boots ago
/logs/boot3.log   ← three boots ago (oldest; deleted on next rotation)
```

### Rotation (in `begin()`)

1. If `/logs/boot3.log` exists, delete it.
2. Rename `boot2.log` → `boot3.log` (if exists).
3. Rename `boot1.log` → `boot2.log` (if exists).
4. Rename `boot0.log` → `boot1.log` (if exists).
5. Create fresh `/logs/boot0.log`.
6. `SdService::ensureDirectory("/logs")` before any of the above.

SdService has no rename — use `readBinaryFile` + `writeBinaryFile` + `deleteFile` to implement
rotation. Only rotate if SD is mounted; `begin()` returns silently (no-op) if
`!sdService.getSdState()`.

### vprintf hook

```cpp
// module-static in LogService.cpp
static LogService* activeLogger = nullptr;
static vprintf_like_t originalVprintf = nullptr;

static int sdLogVprintf(const char* fmt, va_list args) {
    // 1. Format into a local char[512] buffer via vsnprintf
    // 2. Append to a module-static char[512] accumulation buffer
    // 3. If accumulation buffer contains '\n' or is >= 512 bytes, flush to SD + file.flush()
    // 4. Also call originalVprintf so serial output is preserved
    return result;
}
```

`begin()` calls `esp_log_set_vprintf(sdLogVprintf)` and saves the return value into
`originalVprintf`. `end()` calls `esp_log_set_vprintf(originalVprintf)` and clears
`activeLogger`.

The hook is safe to call from any task because SD writes on ESP32 Arduino are not ISR-safe
anyway — same guarantee as existing SD usage in the codebase.

---

## LogController

**Single responsibility:** parse `TerminalCommand`, call `LogService`/`SdService`, write results
to `ITerminalView`.

### Constructor

```cpp
LogController(ITerminalView&, LogService&, SdService&)
```

### Commands (dispatched from `LOG` mode)

| Command | Description |
|---|---|
| `on` | `logService.begin()`. Prints error if SD not mounted. |
| `off` | `logService.end()`. Prints confirmation. |
| `head [n]` | Print first `n` lines (default 20) of `boot0.log`. |
| `tail [n]` | Print last `n` lines (default 20) of `boot0.log`. |
| `ls` | List all four log slots with file sizes. |
| `help` | Print available commands. |

### head/tail implementation

Both use `LogService::readChunk(0, offset, maxBytes)` (slot 0 = current boot), which delegates
to `SdService::readFileChunk`.

**head:** read from offset 0, up to a generous byte limit, then count newlines to cap at `n` lines.

**tail:** walk backward from `logService.fileSize(0)` in 512-byte chunks to find the
`n`-th-from-last newline, then call `readChunk` from that offset to the end.

### Mode entry/exit

Entering `LOG` mode does **not** automatically call `begin()` — the user controls that explicitly
with `on`/`off`. This keeps the behaviour predictable and avoids surprise rotation on every mode
visit.

---

## Wiring

### ModeEnum

Add `LOG` entry.

### DependencyProvider

```cpp
// .h — private members
LogService logService;
LogController logController;

// .cpp — constructor init list
logService(sdService),
logController(terminalView, logService, sdService),
```

Add getter methods.

### ActionDispatcher

In `dispatchCommand`, add a branch for `LOG` mode routing to `logController.handleCommand(cmd)`.

In `setup()`, after the SD card would be available on internal-SD devices:

```cpp
if (state.getHasInternalSdCard()) {
    provider.getSdService().configure(
        state.getSdCardClkPin(), state.getSdCardMisoPin(),
        state.getSdCardMosiPin(), state.getSdCardCsPin()
    );
    provider.getLogService().begin();
}
```

This makes logging start automatically at boot on Cardputer ADV without any user interaction.

---

## Error handling

- `begin()` is a no-op if SD is not mounted.
- `on` command prints `"Log: SD card not mounted"` and returns if SD unavailable.
- `head`/`tail`/`ls` print `"Log: not enabled"` if `begin()` was never called successfully.
- If a rotation step fails (e.g. write error), `begin()` still proceeds — best-effort.

---

## Out of scope

- Timestamp injection (ESP-IDF already prefixes log lines with a tick count).
- Log level filtering.
- Writing to LittleFS instead of SD.
