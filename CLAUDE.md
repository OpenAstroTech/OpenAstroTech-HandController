# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Firmware for the OpenAstroTech Hand Controller: a device that sits between a PC client (ASCOM/OATControl/NINA) and an OAT/OAM mount. It forwards LX200 commands from the PC to the mount, and on-device input (analog joystick, buttons, OLED menu) can issue its own commands. Target hardware is the Adafruit Feather RP2040 with USB Type-A Host (`adafruit_feather_usb_host`), overclocked to 120 MHz — required for PIO-USB bit-banging. Early-stage/prototype firmware.

## Commands

PlatformIO is the build system (VSCode + PlatformIO extension is the expected dev environment). Single environment: `rpi2040` (default).

- Build: `pio run`
- Upload to board: `pio run -t upload`
- Serial monitor (19200 baud): `pio device monitor`
- Clean: `pio run -t clean`
- Unit tests (host, no hardware): `pio test -e native`; single suite: `pio test -e native -f test_joystick`

The platform is pulled from a fork (`maxgerhardt/platform-raspberrypi`) and library dependencies (Pico-PIO-USB, ClutchplateDude's SSD1306 OLED fork) download into `.pio/libdeps/` on first build — never edit anything under `.pio/`.

### Native unit tests

`pio test -e native` runs Unity tests on the host PC against the firmware's pure logic (everything except `main.cpp`, `SerialUSBHost.cpp`, `MenuSystem.cpp`, which need hardware). Each test lives in its own `test/<name>/main.cpp` (one runner per directory — files loose in `test/` root get linked into one program and collide). Every test includes `test/support/TestHarness.hpp`, which instantiates the real ArduinoCore-API `String` (vendored under `test/support/api/`) plus `millis()`/`analogRead()`/`Serial` test doubles with injectable state (`fake_millis`, `fake_analogRead`). The native env compiles with the machine's host GCC (Strawberry Perl's GCC 4.9 here): it needs `-std=gnu++11` and `-DHOST` — the vendored `Common.h` is patched to guard its weak `main()` declaration, which GCC 4.9/MinGW mislinks. New bugfixes in testable logic are expected to come with a failing test first (TDD).

There is no CI/lint config in this repo. Note: `src/README.md` is inherited from the OAT firmware project and references clang-format, matrix_build.py, and GitHub actions that do not exist in this repo.

## Architecture

### Dual-core split (RP2040)

- **Core1** (`setup1`/`loop1` in `src/main.cpp`): runs the TinyUSB **host** stack via PIO-USB (`USBHost.task()`). The mount plugs into the USB-A host port and appears as a CDC device (`Adafruit_USBH_CDC SerialHost`). Pin config lives in `src/usbh_helper.h` (D+ = pin 16, VBUS enable = pin 18).
- **Core0** (`setup`/`loop`): all application logic — job loop, controller, input, display.

### Three distinct serial roles

1. `Serial` — USB CDC device to the PC: LX200 commands in, replies out; also carries debug logging.
2. `mountSerial` (`SerialUsbHost` implementing `ISerial`) — connection to the mount over the USB host port.
3. `LOG()` output — goes to `Serial` (see Configuration below).

### Job pipeline (the heart of the app)

Everything flowing to the mount is a `MessageJob` (`src/MessageJob.hpp/.cpp`): an LX200 command string, a `JobSource` (`FromClient` or `FromController`), and a `CommandType` parsed in the constructor from a large lookup table in `MessageJob.cpp`: `NoReply`, `NumberReply` (single char), `StringReply` (`#`-terminated), `DoubleStringReply` (two `#`-terminated strings, e.g. `:SC#`). Commands are normalized during parsing: leading `:`, trailing `#`, and embedded spaces stripped.

Main loop (`loop()` in `src/main.cpp`):
1. `processSerialFromClient()` (`src/Serial.cpp`) accumulates characters until `#`, returns a job. A `0x06` ACK is answered directly with `P` without queueing.
2. `controller->timeslice()` may create jobs from on-device input (currently a stub returning `nullptr`).
3. Jobs are enqueued in `JobQueue` (intrusive doubly-linked list with a sentinel root; jobs deleted after completion).
4. `JobPipeline` (`src/JobPipeline.hpp`) runs the state machine: `AppIdle` → dequeue one job, write it to the mount (`NoReply` jobs complete immediately) → `AwaitingCommandReply` → `processSerialFromMount()` parses the reply according to the job's `CommandType` → route the reply back to its source: client jobs get `Serial.print(reply)` (with `:GX#` status replies also cached in the Controller via `setLastGX`), controller jobs go to `Controller::setReply()`. Exactly one job is in flight at a time; a silent/disconnected mount times out after `REPLY_TIMEOUT_MS` (2 s) instead of wedging. `main.cpp`'s `loop()` just enqueues jobs and calls `pipeline->tick(jobQueue)`.

`Controller` (`src/Controller.hpp/.cpp`) has its own internal `ControllerState` — not to be confused with the app-level `AppState` in `main.cpp`.

### Interfaces

- `ISerial` (`src/Serial.hpp`): serial port abstraction. Implementation: `SerialUsbHost` (`src/SerialUSBHost.hpp/.cpp`).
- `IInputDevice` (`src/InputDevice.hpp`): input abstraction with bit-flag `Capabilities` (`RawInput`, `KeyPress`) and `KeyPress` events. Implementation: `Joystick` (`src/Joystick.hpp`) — analog pins, 16-sample moving average, threshold-based direction keys with a 400 ms repeat interval.
- `Controller`: owns the input device, turns input into jobs, holds the last `GX` mount-status reply. Skeleton so far.

### Display stack (feature-gated, currently OFF)

`INFO_DISPLAY` in `src/configuration.hpp` (currently `0`) gates all display code at compile time. When enabled: `InfoDisplayRender` (base) → `SDD1306OLED128x64` (`src/SSD1306_128x64_Display.hpp`, SSD1306 I²C 128x64 at addr 0x3C) → `InfoDisplay` (`src/InfoDisplay.hpp`), which composes a `MenuSystem` (`src/MenuSystem.hpp/.cpp`): a linked-list menu tree with action items and number-input items. Menu navigation is driven by joystick `KeyPress` events in the main loop; rendering is throttled to ~50 ms.

### Configuration & debug logging

- `src/configuration.hpp` is the central switch panel: `DEBUG_LEVEL` (bitmask of levels from `src/Constants.hpp` — `DEBUG_SERIAL`, `DEBUG_MOUNT`, `DEBUG_JOBS`, etc.; `DEBUG_ANY` = all, `DEBUG_NONE` = release) and `INFO_DISPLAY`.
- `LOG(level, format, ...)` from `src/Utility.hpp` compiles away entirely when `DEBUG_LEVEL` is 0; use it instead of raw `Serial.print` for anything debug-related.
- `src/Version.h`: `VERSION` string — max two digits per component (e.g. `V0.1.0`, not `V0.123.4`); numbers are zero-padded for comparison (1.8 < 1.12).

## Conventions

- Mixed style: some classes are header-only (`.hpp` with inline bodies, e.g. `Joystick`, `MessageJob`, `InfoDisplay`), others are `.hpp`/`.cpp` pairs (`Controller`, `MenuSystem`, `Serial`, `SerialUSBHost`). Either is acceptable.
- Use the `F()` macro for string literals so they stay in flash.
- Mount-facing hardware access goes through the `ISerial` interface, not direct serial objects.
