# Arduino Command Shell

Serial command shell firmware for **ATmega328P boards** on PlatformIO.
Current board targets:
- **ATmega328P Xplained Mini**
- **Arduino Uno**

The project provides:
- Interactive UART shell (`arduino$ ` prompt)
- GPIO, timing, I2C, EEPROM, and low-level AVR register commands
- EEPROM-backed mini filesystem (`fs ...`)
- Startup script support (`/scripts/boot.sh`) with background LED blink task

![ARDUINO](./lib/shell.png?200)


## Hardware/Platform

- MCU: ATmega328P @ 16 MHz
- Framework: Arduino
- Serial monitor baud: **57600**

Supported board definitions:
- `atmega328p_xplained_mini`
  - Uses custom board file: `boards/atmega328p_xplained_mini.json`
  - Typical upload: `xplainedmini_isp` (EDBG over USB)
- `uno`
  - Uses PlatformIO built-in Arduino Uno board definition
  - Typical upload: Optiboot serial bootloader (`arduino` protocol)

## Project Layout

- `src/main.cpp`: boot sequence + main loop
- `src/shell.hpp`: shared constants and function declarations
- `src/shell_shared.cpp`: parsers, helpers, FS primitives, history, common state
- `src/shell_help.cpp`: top-level help/status text
- `src/shell_commands.cpp`: command dispatcher
- `src/shell_commands_*.cpp`: command groups by domain (`fs`, `i2c`, `eeprom`, `gpio`, `lowlevel`)
- `src/shell_io.cpp`: serial line input, echo, history (up/down arrows)
- `src/shell_startup.cpp`: startup script loader and background blink task
- `platformio.ini`: build/env config + feature switches
- `boards/atmega328p_xplained_mini.json`: custom board definition

## Prerequisites

- PlatformIO Core installed
- `pio` in `PATH` **or** use `~/.platformio/penv/bin/pio`

Examples below use the explicit binary path:
- `PIO=~/.platformio/penv/bin/pio`

## Build / Upload / Monitor

Select board in `platformio.ini` first:

```ini
[target]
board = atmega328p_xplained_mini
; board = uno
```

Build (single environment):

```bash
$PIO run -e avr
```

Upload:

```bash
$PIO run -e avr -t upload
```

Open monitor:

```bash
$PIO device monitor -e avr
```

If you prefer plain monitor command, keep baud at `57600`.

Notes:
- `ver` and `id` print board name based on the selected target.
- Upload transport is board-dependent (EDBG ISP for Xplained Mini, serial bootloader for Uno).

## Configuration (platformio.ini)

### Feature switches

In `[features]` (0 = disabled, 1 = enabled):

- `feature_i2c`
- `feature_eeprom`
- `feature_fs` (requires `feature_eeprom=1`)
- `feature_tone`
- `feature_lowlevel`

These map to compile-time flags (`FEATURE_*`) in `build_flags`.

### Board selection

Use a single environment and switch board in `[target]`:

```ini
[target]
board = atmega328p_xplained_mini
; board = uno
```

The selected board also controls compile-time board identification used by shell commands (`ver`, `id`).

### Firmware/version/baud

- `-DFW_VERSION="1.1.0"`
- `-DDEMO_BAUD=57600UL`

### EEPROM persistence across uploads

`board_hardware.eesave = yes` is enabled.

This keeps EEPROM content across uploads that involve chip erase operations (notably ISP flows). For Uno serial bootloader uploads, EEPROM is typically preserved as well unless explicitly erased by tooling or firmware commands.

## Shell Usage

Type `help` in the terminal for runtime-visible commands.

### Core

- `help`
- `status`
- `ver`
- `id`
- `echo <text>`
- `free`
- `uptime`
- `micros`
- `reset`

### GPIO / Timing

- `pinmode <pin> <in|out|pullup>`
- `digitalread <pin>`
- `digitalwrite <pin> <0|1>`
- `analogread <A0-A5>`
- `pwm <pin> <0-255>`
- `pulse <pin> <count> <high_ms> <low_ms>`
- `watch <pin>`
- `delay <ms>`
- `freq <pin> [ms]`
- `tone <pin> <freq> [ms]` / `notone <pin>` (when `feature_tone=1`)

### I2C (when enabled)

- `i2cscan`
- `i2cspeed <100k|400k>`
- `i2cread <addr> <n>`
- `i2cwrite <addr> <bytes...>`
- `i2cwr <addr> <reg> <bytes...>`
- `i2crr <addr> <reg> <n>`

### EEPROM (when enabled)

- `eepread <addr> [len]`
- `eepwrite <addr> <bytes...>`
- `eeperase confirm`

### EEPROM mini filesystem (when enabled)

- `fs help`
- `fs format confirm`
- `fs ls [path]`
- `fs cat <path>`
- `fs mkdir <path>`
- `fs touch <path>`
- `fs write <path> <text>`
- `fs rm <path>`
- `fs stat`

Notes:
- EEPROM size is 1024 bytes on ATmega328P.
- FS reserves metadata and uses the remaining space for file data.
- `eep*` commands operate on raw EEPROM and can destroy FS data.

### Low-level AVR (when enabled)

- `ddr <port> [value]`
- `port <port> [value]`
- `pin <port>`
- `peek <addr>`
- `poke <addr> <val>`
- `reg`

## Startup Script (`/scripts/boot.sh`)

On boot (when `feature_fs=1`):

1. FS is initialized if needed.
2. `/scripts` directory is ensured.
3. `/scripts/boot.sh` is auto-created if missing.
4. Script is parsed and executed.

Default auto-created script:

```sh
# Startup script
# blink <pin> <period_ms>
blink 13 1000
```

Supported script syntax right now:
- Blank lines and `#` comments
- `blink <pin> <period_ms>`

`blink` starts a non-blocking background task handled from `loop()`.

## Developer Notes

- Target has only **2 KB SRAM**. Keep stack usage low, especially in command handlers.
- Avoid large local buffers in deep call paths (`fs` commands are the most stack-sensitive).
- Prefer flash-stored strings/constants (`F(...)`, `PROGMEM`) over RAM globals where practical.
- Feature flags in `platformio.ini` are the primary way to trim footprint.
- `eep*` raw EEPROM commands and `fs` commands share the same EEPROM space. Mixing them can corrupt FS metadata/data.
- If behavior becomes unstable (garbled output/resets), check memory first:
  - Disable heavy features temporarily (`feature_* = 0`) to bisect.
  - Recheck stack-heavy paths in recently changed code.
  - Rebuild and verify memory report from PlatformIO output.

## Safety / Caveats

- `peek`/`poke` are intentionally dangerous and can crash the MCU. Use at your own risk.
- `reset` triggers watchdog reset immediately.
- `watch` and `pulse` can be interrupted with any key.
- If terminal output looks like garbage, check baud is `57600`.

## Some Commands

```text
help
fs stat
fs ls /
fs cat /scripts/boot.sh
```

Then edit/create files with `fs write ...` as needed.

## License

This project is licensed under the **MIT License** (see `LICENSE`).
It is provided **as-is**, without warranty of any kind.
