# Submarine Monitoring System — Project Handoff

> Context document for continuing this project with any AI model. Written 2026-09-07.
> Course project for the **Google Reichman Tech School embedded course**. Developer works on **macOS**.

---

## 1. What this project is

A two-part final project defined in `final project.pdf` (in the repo root):

- **Part 1 — LNC End Unit firmware** (STM32, C): a "Local Node Controller" inside a submarine with **nine software modules** (Monitor, Object Detection, Event, Log, Communication, Configuration, Init, Keep-Alive, Watchdog). It samples sensors, drives an RGB LED + buzzer on mode changes, persists config + rotating logs, feeds a hardware watchdog, and exchanges **TLV** (Tag-Length-Value) messages with a Central Computer over a transport-independent link. **This is the part actively being built.**
- **Part 2 — Fleet Management System** (C++): a separate OOP console app (research/combat submarines, missions, messaging, a 10-option menu). **COMPLETE** — built test-first in `fleet/` (repo). All 10 menu operations, both submarine types, associations, and mission-scoped messaging; 7 test suites pass via `make test`. Built with plain `clang++` + a Makefile + a tiny assert harness (no cmake/GoogleTest, since neither is available in this environment — trivial to port to GoogleTest if a submission needs it). Run the app: `cd fleet && make fleet && ./fleet`.

The spec also describes a Central Computer and a Ground Station (upstream of the LNC) — out of scope for the firmware itself but they define the protocol contract.

---

## 2. Two separate locations (IMPORTANT)

The work lives in **two different folders**:

1. **Git repo (backup + planning + host-tested core):**
   `/Users/tehilamenasheof/Claude/SUBMARINE_MONITORING_SYSTEM/`
   - `lnc/` — the **host-testable core** C modules + `Makefile` + `tools/decode_serial.py`
   - `lnc/src/core/` — pure logic (no hardware), unit-tested on the host
   - `docs/superpowers/plans/` — two implementation plans (C++ fleet, LNC firmware)
   - `architecture.html`, `class-diagram.html` — design docs (open in a browser)
   - `lnc/CUBEMX_SETUP.md`, `lnc/HARDWARE.md` — hardware setup notes
   - This folder **has git** — commit here.

2. **STM32CubeIDE project (the firmware that actually runs on the board):**
   `/Users/tehilamenasheof/STM32CubeIDE/workspace_1.19.0/Submarine/`
   - `Core/Inc/` — all LNC headers (copied from the repo core + platform headers)
   - `Core/Src/lnc/` — LNC sources: the core modules **plus** the STM32-specific platform + app code that is **ONLY here, not in git**
   - `Core/Src/main.c` — CubeMX-generated; LNC is hooked in via `USER CODE` sections
   - **This folder is NOT under git.** The platform/app files (`hal_*.c`, `transport_uart.c`, `lnc_app.c`, `DHT.c`) exist only here. (User chose not to back them up to the repo yet.)

The repo `lnc/src/core/*` and the project `Core/Src/lnc/*` core files are copies of each other. If you change core logic, keep them in sync (edit repo → re-copy, or edit project → copy back).

---

## 3. Hardware

- **Board:** Nucleo-**L476RG** (STM32L476RGT3), Cortex-M4 @ **80 MHz** (HSI16 → PLL ×10 ÷2, verified).
- **Clock:** SYSCLK = HCLK = PCLK1 = PCLK2 = 80 MHz, no bus prescalers.
- The developer's CubeIDE **workspace has many example projects** (`DHT_Ex`, `BuzzPlay`, `PWM`, `SDCard`, `FlashMemoryTask`, `RTC`, `Watchdog`, `Buttons`, `I2C`, `UART`, …) — these are the **course's reference drivers**. Mirror them rather than inventing hardware code.

### Pin / peripheral map (as configured in `Submarine.ioc`)

| Function | Pin(s) | Peripheral / handle | Reference project | Status |
|---|---|---|---|---|
| Central link (UART to host) | PA2/PA3 | USART2, `huart2`, 115200 8N1 (ST-Link VCP) | — | ✅ working |
| Temp + humidity | PB5 + TIM5 | DHT11, `htim5` | `DHT_Ex` | ✅ **real readings** |
| Buzzer / alarm | PB4 | TIM3_CH1 PWM, `htim3` | `BuzzPlay` | wired in code; LED/buzzer physical wiring **unconfirmed** |
| RGB LED | PC0/PC1/PC2 | GPIO out `LED_R/G/B` | — | wired in code; physical **unconfirmed** |
| Button (stop alarm) | PC13 | EXTI13 (on-board B1) | `RTC`/`Buttons` | code ready; NVIC EXTI enable **unconfirmed** |
| RTC | PC14/PC15 (LSE) | `hrtc` | `RTC` | ✅ working (epoch conversion) |
| Watchdog | — | IWDG, `hiwdg` (presc 32, reload 4095 ≈ 4 s) | `Watchdog` | ✅ refreshed every 500 ms |
| SD card (logs) | PA5/PA6/PA7 + PB6 CS | SPI1 + FATFS | `SDCard` | configured in CubeMX, **not wired in firmware yet** |
| **Light (LDR)** | PA1 | ADC1_IN6, `hadc1` | — | ✅ **real** (bright~4095, covered~1255) |
| **Battery (potentiometer)** | PA0 | ADC1_IN5, `hadc1` | — | ✅ **real** (0–4095) |
| **Object detection (sonar)** | *TBD* | timer input capture | — | ⚠️ **stubbed = not present** |

Note: SD's SPI1 takes PA5/PA6/PA7, which is why the RGB LED is on PC0/1/2 (not the usual on-board LD2 on PA5).

---

## 4. Firmware architecture

**Execution model:** a single **non-blocking timer-driven super-loop** (NO RTOS). `main.c` calls `lnc_app_init()` once, then `lnc_app_poll()` every loop iteration. Periodic work is gated by `sched_due(&last, period_ms, HAL_GetTick())`.

**Design split** (so the logic is host-testable):
- **Pure core** (`Core/Src/lnc/*.c`, also in repo `lnc/src/core/`) — no HAL includes. Unit-tested on host via `make test`.
- **Platform layer** (`Core/Src/lnc/hal_*.c`, `transport_uart.c`, `DHT.c`) — direct STM32 HAL, only in the STM32 project.
- **App** (`Core/Src/lnc/lnc_app.c`) — wires core to hardware in the super-loop.

**Core modules (all host-tested, in `lnc/src/core/`):**
`lnc_types.h` (enums + structs), `protocol.h` (TLV tag numbers), `bytes` (LE pack), `tlv` (codec), `lnc_limits` (mode classification — *renamed from `limits` to avoid clashing with the C stdlib `<limits.h>`*), `monitor_logic` (mode-change detection), `objectdet_logic` (object edge), `event_decide` (mode→LED/alarm actions), `logplan` (7-day rotation), `logline` (CSV log-line format), `commq` (TX priority), `config` (defaults/serialize/SET-command), `keepalive` + `records` (TLV builders), `command` (parses ALL management commands), `init_logic` (startup event), `sched` (super-loop timing helper).

**Platform modules (STM32 project only):**
- `hal_indicators.c` — `led_set(color)`, `alarm_set(on)` (TIM3 PWM), button flag + `HAL_GPIO_EXTI_Callback`.
- `hal_time.c` — `rtc_now()`/`rtc_set()` (epoch↔RTC calendar).
- `hal_watchdog.c` — `wd_refresh()` (IWDG), `reset_cause()` (RCC flags).
- `hal_sensors.c` — DHT temp/humidity (PB5), ADC light+battery (PA1/PA0), and IR object detection (PB10, latched). All real.
- `store_sd.c` — FatFS-over-SPI Log module: day-file logging + 7-day rotation, events file, config persistence (`CONFIG.BIN`), range queries, and `ls`/`cat`.
- `transport_uart.c` — `transport_send` over USART2 (transport-independent seam).
- `DHT.c`/`DHT.h` — copied from `DHT_Ex`, **patched** with an iteration guard (see §6).
- `lnc_app.c` — the super-loop + queues + the console/protocol modes + RX.

**Central Computer / RX:** `command.c` parses all management commands (8 limits, SET_RTC, GET_TIME) + the two range-retrieval instructions (unit-tested). RX is **live** — interrupt-driven, dispatched to `command_handle` in protocol mode (see §5). A host-side `lnc/tools/central.py` drives the binary protocol.

---

## 5. Current state (what works / what's off)

✅ **ALL NINE LNC modules verified on real hardware:**
- **Monitor** — 4 real channels: temp+humidity (DHT11 PB5), light (LDR PA1), battery (pot PA0).
- **Object Detection** — IR receiver on PB10 (point a remote at it); latched detect/clear → events.
- **Event** — RGB LED (PC7=R/PC8=G/PC6=B, active-high) + buzzer (TIM3 PB4) + events file + Central notify.
- **Log** — SD/FatFS day-files + 7-day rotation + events file.
- **Communication** — TLV, keep-alive (6 s), events, RX commands, range retrieval; console + binary modes.
- **Configuration** — limits + SET commands, persisted to `CONFIG.BIN` (survives reboot).
- **Init** — config load-or-default, reset cause, startup event.
- **Keep-Alive** — timestamp+measurement+mode every 6 s (protocol mode).
- **Watchdog** — IWDG refresh + reset-cause reporting.
- Part 2 (C++ Fleet): **complete** (see `fleet/`).

⚙️ **Runtime mode toggle (`s_protocol` in `lnc_app.c`, default = console):**

- **Console mode (default at boot):** human-readable text + ASCII commands over USART2, readable in `screen`. Status lines `T=… H=… L=… B=… mode=…` every 2 s; `>> EVENT: …` lines. Commands: `help now rtc tn tw ls cat get getev led ir proto`.
- **Protocol mode:** the binary TLV machine protocol (keep-alive 6 s, event/data frames, binary command RX). Enter it with the `proto` command (or run `lnc/tools/central.py`, which sends `proto` then drives it). **RESET returns to console.** Monitor samples 2 s in console, 5 s (spec) in protocol.
- RX is **interrupt-driven** (was NOT polled — polling caused a UART overrun wedge; see §6).

**Config:** first-boot defaults live in `Core/Inc/config_defaults.h` (demo-tuned: temp ≤28 NORMAL, light/battery ADC thresholds). After first boot, limits load from `CONFIG.BIN` on the SD card, so **whatever you last set persists** (that's why a stale `tn 10 22` can make it boot in WARNING — just `tn 10 32` to reset).

✅ **Central Computer (spec §3): COMPLETE** — built in C++ in `central/`, reusing the LNC's `tlv.c`/`bytes.c`/`protocol.h` directly. All four modules: Communication (transport-independent, self-syncing listener), Management Command, Log, Data Collection & Analysis (DB + 7-day retention + reports). Unit-tested (`cd central && make test`); menu-driven program (`make central && ./central`) auto-detects the port, flips the LNC to protocol mode, drives commands, and stores/reports keep-alive+event+data.

✅ **Ground Station (spec §4): COMPLETE** — built in C++ in `groundstation/`, a client of the Central Computer over **Ethernet (TCP)**, per §1.2. It requests logged measurement data and events for a time range and displays them. Reuses the LNC codec + the shared `central/ground_protocol.{h,cpp}` (TLV tags `0x40–0x44`) so the wire format is transport-independent.
- **Central gains a Ground-facing server:** `central/GroundServer` + `central --serve [port=5555] [dbdir=.]`. It loads the database (from the CSV the Central saves), listens on `127.0.0.1`, and streams matching records back per request.
- **Run the demo (two terminals):**
  ```
  cd central && ./central --serve 5555 <dir-with-measurements.csv+events.csv>
  cd groundstation && make groundstation && ./groundstation 127.0.0.1 5555
  ```
  Menu: `1` retrieve log data (from to), `2` retrieve events (from to), `3` exit.
- **Tested:** `central/test/test_ground_protocol.cpp` covers codec round-trips **and** a real TCP loopback against `GroundServer` (server on a thread, client requests a range, asserts the streamed records). `cd central && make test`.

⏳ **Open / optional:** (a) FreeRTOS variant of the firmware (on a `freertos` branch, all logic modules reusable); (b) event wire format doesn't yet carry the object-detected / INIT-watchdog flags (~2-line fix, makes Central `object=` count non-zero); (c) demo/docs polish for submission.

---

## 6. Hard-won gotchas (READ THESE before debugging serial/hardware)

1. **macOS resets a serial port to 9600 baud on open.** Set the baud on the *already-open* fd (via `termios`/pyserial), NOT with `stty` before opening. This wasted a lot of time — the board was fine, the reader was wrong. Prefer the **`/dev/cu.usbmodem*`** device over `/dev/tty.usbmodem*` (the `tty.*` one blocks on open waiting for carrier).
2. **`STM32_Programmer_CLI -coreReg` / `-r32` HALT the running CPU** (and may leave it halted → board goes silent). Only probe when you intend to, and follow with `-rst -run` to resume. Don't attribute a "dead board" to firmware until you've ruled out a leftover halt.
3. **The DHT read could hang the whole board.** `DHT.c`'s wait loops timed out using the TIM5 counter; if TIM5 hiccuped, they spun forever and locked the super-loop before the watchdog refresh. **Fix applied:** added `DHT_LOOP_GUARD` iteration caps in `DHT_DelayUs`/`DHT_WaitForLevel`. Keep any blocking bit-bang driver hang-proof this way.
4. **Only one program can read the serial port at a time** — quit `screen`/python before starting another reader.
5. The decoder (`lnc/tools/decode_serial.py`) is **self-syncing** (validates frame bodies, slides on mismatch) — needed because a naive framer desyncs on sparse streams.

---

## 7. How to build / flash / view (macOS, from a terminal)

**Host unit tests (pure core, in the repo):**
```bash
cd /Users/tehilamenasheof/Claude/SUBMARINE_MONITORING_SYSTEM/lnc && make test
```

**Headless build of the STM32 project** (works while CubeIDE is open, using a temp workspace):
```bash
/Applications/STM32CubeIDE.app/Contents/MacOS/STM32CubeIDE --launcher.suppressErrors -nosplash \
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild \
  -data /tmp/cubews -build Submarine
```
(Output ELF: `…/Submarine/Debug/Submarine.elf`)

**Flash + run** (ST-Link; note the long plugin path — adjust the version dir if it changes):
```bash
PROG="/Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.macos64_2.2.200.202503041107/tools/bin/STM32_Programmer_CLI"
"$PROG" -c port=SWD mode=UR -w /Users/tehilamenasheof/STM32CubeIDE/workspace_1.19.0/Submarine/Debug/Submarine.elf -rst -run
```

**View output:**
- Console mode (readable): `screen /dev/tty.usbmodem144203 115200`  (quit: Ctrl-A, K, y)
- Protocol mode (binary TLV): `python3 /Users/tehilamenasheof/Claude/SUBMARINE_MONITORING_SYSTEM/lnc/tools/decode_serial.py /dev/cu.usbmodem144203`
- The device name (`usbmodem144203`) may change — check `ls /dev/cu.usbmodem*`.

The developer can also just use **STM32CubeIDE's GUI** to build/flash (Run/Debug) — the headless + programmer CLI above is what an AI assistant can drive itself.

---

## 8. Plans & design docs (in the repo)

- `docs/superpowers/plans/2026-09-04-lnc-firmware.md` — full TDD plan for the firmware (course-oriented, super-loop, no RTOS). Tasks 1–11 (pure core) are **done**; 12–15 (hardware) partially done.
- `docs/superpowers/plans/2026-09-04-fleet-management-cpp.md` — full TDD plan for Part 2 (C++ fleet, GoogleTest). **Not started.**
- `architecture.html` — system/module/mode/TLV design (open in browser).
- `class-diagram.html` — C++ class model for Part 2.
- `lnc/CUBEMX_SETUP.md` — the CubeMX peripheral checklist mapped to the example projects.
- `lnc/HARDWARE.md` — hardware definition contract / decisions checklist.

---

## 9. TLV protocol summary (`protocol.h`)

Frame = `[tag:1][length:1][value:length]`; complex values nest child TLVs. Tag numbers are a project choice and must match the (future) Central Computer.

- Commands (Central→LNC): `CMD_SET_TEMP_NORMAL/WARNING` (0x20/0x21, int16 lo+hi), `CMD_SET_HUM/LIGHT/BATT_NORMAL/WARNING` (0x22–0x27, uint16 lower bound), `CMD_SET_RTC` (0x28, uint32 epoch), `CMD_GET_TIME` (0x29), `CMD_GET_DATA_RANGE`/`CMD_GET_EVENTS_RANGE` (0x2A/0x2B, uint32 from+to).
- Reports (LNC→Central): `RPT_KEEPALIVE` (0x01), `RPT_EVENT` (0x02), `RPT_DATA` (0x03), `RSP_TIME` (0x04). Child tags: `TAG_TIMESTAMP` 0x10, `TAG_MEASUREMENT` 0x11, `TAG_MODE` 0x12, `TAG_EVENT_SRC` 0x13.
- Keep-alive value nests: timestamp + measurement(temp i16, hum u16, light u16, batt u16) + mode.

---

## 10. Suggested next steps

1. **Light + battery on ADC** — enable ADC1 in CubeMX (two channels), decide the pins, then read both in `hal_sensors.c` (replace the stubs). Makes all four Monitor channels real.
2. **SD / FATFS logging** — port the `SDCard`/`FlashMemoryTask` SPI+FatFS driver so `store_log` (7-day rotation via `logplan`/`logline`) is real. Unlocks the data/event retrieval commands.
3. **RX + command execution** — re-enable receiving as interrupt/DMA-driven, wire `command_handle`'s results (config persist, RTC set, range queries) — currently stubbed/disabled.
4. **Object detection (sonar)** — add the sonar read for `objectdet_logic`.
5. **Config persistence in Flash** — currently config resets to defaults each boot; implement `store_config` in internal Flash.
6. ~~Part 2 (C++ fleet)~~ — **DONE** (see `fleet/`).

Every core module is already unit-tested (`make test` = 15 suites green), so lean on those when changing logic.
