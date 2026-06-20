# Architecture

## What this project is

ESP32-Bit-Pirate is a Bus-Pirate-style hardware hacking tool running on ESP32-S3 boards (M5Stack Cardputer, StickS3, T-Embed, T-Display, Waveshare S3-Geek, plain S3 DevKit, etc.). It exposes a CLI over USB serial, WebSocket (browser), or the device's own screen (Standalone Cardputer mode). It speaks ~24 hardware protocols (UART, I2C, SPI, 1Wire, JTAG, SubGHz, RFID, BLE, CAN, Ethernet, FM, Cell, OSC, etc.) and emulates several PC-side protocols (BusPirate/avrdude, flashrom/serprog, OpenOCD, Infrared Toy, SUMP logic analyser).

## Command data flow

```
TerminalInput (reads chars)
    → ActionDispatcher (routes raw string to command/instruction pipeline)
    → Controller (parses args, validates)
    → Service (drives hardware)
    → Controller (formats response)
    → TerminalView (prints output)
```

ActionDispatcher also handles: pipelines (`|`), repeat sequences, and bytecode instruction vectors.

## Module breakdown

### Interfaces (`src/Interfaces/`)
- `ITerminalView` — print/println/printPrompt/clear/welcome/waitPress
- `IDeviceView` — logo/show/drawLogicTrace/drawAnalogicTrace/drawWaterfall/horizontalSelection/…
- `IInput` — character-level input (read, available, waitPress)
- `IHostSerial` — serial abstraction (default USB CDC vs UART)

### Views (`src/Views/`)
Implement `ITerminalView` or `IDeviceView`.
- Terminal views: `SerialTerminalView`, `WebTerminalView`, `CardputerTerminalView`
- Device views: `M5DeviceView`, `CardputerDeviceView`, `TembedDeviceView`, `TdisplayDeviceView`, `WaveshareS3GeekDeviceView`, `NoScreenDeviceView`, `LGFXILI9341ScreenView`

### Inputs (`src/Inputs/`)
Implement `IInput`. Per-device keyboard/button drivers:
`CardputerInput`, `SerialTerminalInput`, `WebTerminalInput`, `StickInput`, `StampS3Input`, `TembedInput`, `TdisplayInput`, `WaveshareS3GeekInput`, `S3DevKitInput`

### Services (`src/Services/`)
Thin hardware-abstraction wrappers. No output, no parsing — just hardware calls.
Examples: `UartService`, `I2cService`, `SpiService`, `WifiService`, `SubGhzService`, `OSCService`, etc. (~35 services total)

### Controllers (`src/Controllers/`)
Parse text commands from `TerminalCommand`, call into Services, write results back to `ITerminalView`.
One Controller per protocol/mode (e.g. `UartController`, `WifiController`, `SubGhzController`, `ExpanderController`, etc.)

### ActionDispatcher (`src/Dispatchers/ActionDispatcher.h`)
Central event loop. Owns the `run()` forever-loop. Reads `getUserAction()`, handles escape sequences, tab completion, backspace. Dispatches to `dispatchCommand()`, `dispatchInstructions()`, `dispatchPipelineCommands()`.

### DependencyProvider (`src/Providers/DependencyProvider.h`)
Manual DI container — owns (by value) all services, controllers, transformers, managers, shells, and selectors. Injected into `ActionDispatcher`. Created on the heap in `main.cpp` because it's too large for the stack.

### GlobalState (`src/States/GlobalState.h`)
Singleton (Meyer's singleton). Holds all runtime-mutable pin assignments, baud rates, frequencies, and mode flags for every protocol. Defaults set at construction from `#define` macros; modified at runtime via setters.

### Transformers (`src/Transformers/`)
Parse text → typed model objects.
- `TerminalCommandTransformer` — raw string → `TerminalCommand`
- `InstructionTransformer` — text → `vector<Instruction>` (bytecode)
- `ArgTransformer` — token parsing helpers
- `WebRequestTransformer`, `JsonTransformer` — web layer
- `InfraredRemoteTransformer`, `SubGhzTransformer`, `ProfileTransformer`, `AtTransformer`, `PinoutTransformer`

### Shells (`src/Shells/`)
Multi-step interactive sub-modes that take over the input loop.
Examples: `I2cEepromShell`, `SpiFlashShell`, `SdCardShell`, `UartAtShell`, `ModbusShell`, `CellCallShell`, `FmBroadcastShell`, `UniversalRemoteShell`, `MouseShell`, `UsbAdapterShell`, etc.

### Adapters (`src/Adapters/`)
Emulate PC-side tool protocols over USB CDC:
`AvrDudeBusPirateAdapter`, `FlashromSerprogAdapter`, `OpenOcdBusPirateAdapter`, `InfraredToyAdapter`, `SumpLogicAnalyzerAdapter`, `UsbUartBridgeAdapter`, `Bpio2Adapter`, `SubGhzRawCdcAdapter`

### Analyzers (`src/Analyzers/`)
`PinAnalyzer` (logic/analog trace), `BinaryAnalyzer`, `SubGhzAnalyzer`

### Managers (`src/Managers/`)
`CommandHistoryManager`, `UserInputManager`, `AliasManager`

### Config (`src/Config/`)
Boot-time configurators that consume `IDeviceView`, `IInput`, `NvsService`:
`BootModeConfigurator` (USB adapter mode), `TerminalTypeConfigurator`, `WifiTypeConfigurator`

### Models / Enums / Data
- `ModeEnum` — HIZ, OneWire, UART, HDUART, I2C, SPI, 2WIRE, 3WIRE, DIO, LED, INFRARED, USB, BLUETOOTH, WIFI, JTAG, I2S, CAN, ETHERNET, SUBGHZ, RFID, RF24, FM, CELL, EXPANDER
- `TerminalTypeEnum` — SerialPort, WiFiClient, WiFiAp, Standalone
- `Data/` — static lookup tables (known I2C addresses, IR protocol definitions, SubGHz protocols, flash DB, etc.)

### Servers (`src/Servers/`)
`HttpServer`, `WebSocketServer`, `DnsServer` — used only in WiFi terminal modes

### Vendors (`src/Vendors/`)
Third-party or vendored code: `PN532`, `wifi_atks`, `i2c_sniffer`, `MakeHex`, per-device WiFi setup helpers

## Build environments (platformio.ini)

| env | Board | Notes |
|-----|-------|-------|
| `cardputer` | m5stack-stamps3 | Basic Cardputer |
| `cardputer-adv` | m5stack-stamps3 | Advanced, more features |
| `cardputer-adv-ili9341` | m5stack-stamps3 | Cardputer ADV + external ILI9341 TFT |
| `m5stack-sticks3` | M5StickS3 | |
| `s3-devkit` | Generic ESP32-S3 DevKit | |
| `s3-devkit-n16-r8` | ESP32-S3 DevKit N16R8 | |
| `m5stack-stamps3` | M5Stack StampS3 | |
| `atom-lite-s3` | M5Stack AtomS3 | |
| `t-display-s3` | LilyGo T-Display-S3 | |
| `waveshare-s3-geek` | Waveshare ESP32-S3-Geek | |
| `t-embed-s3` | LilyGo T-Embed-S3 | |
| `t-embed-s3-cc1101` | T-Embed-S3 + CC1101 | SubGHz via CC1101 |
| `t-embed-s3-cc1101plus` | T-Embed-S3 + CC1101+ | |
| `xiao-esp32s3` | Seeed XIAO ESP32-S3 | |
| `heltec_wifi_lora_32_V3` | Heltec WiFi LoRa 32 V3 | |

## Key notes
- `DependencyProvider` is always heap-allocated (`new DependencyProvider(...)`) — stack overflow otherwise
- `loop()` in `main.cpp` is intentionally empty; `ActionDispatcher::run()` is the forever loop
- Platform: pioarduino fork of platform-espressif32 (not official PlatformIO espressif32)
- `patch_osc_lib.py` patches the cnmat/OSC library at build time (fixes `handleOSC` naming)

## Adding a new protocol

Follow this pattern:
1. Add a `ModeEnum` entry in `src/Enums/ModeEnum.h`
2. Create `src/Services/XxxService.h/.cpp` (hardware only, no output)
3. Create `src/Controllers/XxxController.h/.cpp` (CLI parsing + terminal output)
4. Register both in `src/Providers/DependencyProvider.h/.cpp`
5. Wire into `ActionDispatcher` dispatch table
6. Add build-flag pin defaults to relevant `platformio.ini` environments and `GlobalState`
