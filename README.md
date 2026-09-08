# Submarine Monitoring System

Final project — a submarine monitoring system built around a **Local Node
Controller (LNC)** end unit, a **Central Computer**, a **Ground Station**, and
an object-oriented **Fleet Management System**. All parts speak a common
**TLV (Tag-Length-Value)** message protocol, and the physical transport
(UART / Ethernet) is isolated behind a transport-independent interface.

```
 Ground Station  ──TCP/TLV (Ethernet)──▶  Central Computer  ──UART/TLV──▶  LNC End Unit  ──▶ sensors
  (§4, C++)                                 (§3, C++)                        (Part 1, C on STM32)
                                                                             ├─ temperature / humidity (DHT)
 Fleet Management System (OOP part, C++) — models a fleet of submarines,     ├─ light + battery (ADC)
 each of which *owns* a Central Computer.                                    ├─ object detection (IR)
                                                                             └─ RGB LED + buzzer + SD log
```

## Deliverables & status

| Part | What | Language | Where | Status |
|------|------|----------|-------|--------|
| Part 1 | LNC End Unit firmware — 9 modules (Monitor, Object Detection, Event, Log, Communication, Configuration, Init, Keep-Alive, Watchdog) | C | `stm32/`, `lnc/` | ✅ runs on real Nucleo-L476RG |
| §3 | Central Computer — Communication, Management Command, Log, Data Collection & Analysis | C++ | `central/` | ✅ verified against the board |
| §4 | Ground Station — requests logged data / events over a time range | C++ | `groundstation/` | ✅ verified end-to-end |
| OOP | Fleet Management System — research/combat submarines, missions, messaging, 10-option menu | C++ | `fleet/` | ✅ |

## Repository layout

| Path | Contents |
|------|----------|
| `stm32/Submarine/` | The STM32CubeIDE firmware project (the code that runs on the board). |
| `lnc/` | Host-testable core of the firmware logic + unit tests (plain C, no hardware). |
| `central/` | Central Computer (C++), incl. the Ground-facing TCP server (`--serve`). |
| `groundstation/` | Ground Station client (C++). |
| `fleet/` | Fleet Management System (C++, OOP part). |
| `architecture.html`, `class-diagram.html` | Design documents (open in a browser). |
| `HANDOFF.md` | Detailed engineering notes: pin map, gotchas, build/flash steps. |
| `final project.pdf` | The project specification. |

## Tests

Everything host-side uses a plain compiler + `make` (no external test framework).

```bash
cd lnc     && make test      # LNC logic core  (pure C, no hardware)
cd fleet   && make test      # Fleet Management (C++)
cd central && make test      # Central + Ground-protocol, incl. a TCP loopback (C++)
```

## Demo walkthrough

Four independent demos. The three host programs (`fleet`, `central`,
`groundstation`) build and run on macOS/Linux with no board attached; only the
LNC firmware demo needs the Nucleo.

### 1. Fleet Management System (OOP part)
```bash
cd fleet && make fleet && ./fleet
```
Drives a 10-option menu (`1 Add  2 Display all  3 Search  4 Assign mission
5 Update mission  6 End mission  7 Associate combat  8 Send message
9 Messages  10 Exit`). Suggested demo order (menu numbers in brackets):
- **[1] Add** a *research* submarine (serial, name, researchers, topic), then a
  *combat* submarine (serial, name, commander, crew count).
- **[2] Display all** — shows both, with `available` / `on mission`.
- **[3] Search** by serial number.
- **[4] Assign a mission** to the combat sub → its status flips to *on mission*.
- **[7] Associate** a second combat sub with the *same* mission ("operate together").
- **[8] Send a message** from one combat sub to the other in that mission.
- **[9] Messages** received by the second sub — shows content **and** the sender
  (the stored reference to the sending submarine).
- **[6] End the mission** → the sub is *available* again.

### 2. LNC firmware on the Nucleo-L476RG
Open `stm32/Submarine/` in STM32CubeIDE → **Run** (▶) to build + flash, then open
a serial console at **115200 8N1** (`screen /dev/cu.usbmodem* 115200`).

The board boots in **console mode**. What the demo should show:
- **Boot banner** → `=== LNC console (FreeRTOS) ===`, `SD: mounted`,
  `Config: loaded from CONFIG.BIN`, and a startup event (§2.7).
- **Monitor (§2.1)** — a status line every 5 s: `T=..C H=..% L=.. B=.. mode=..`.
- **Modes + indicators (§2.3, §2.10)** — warm/cool the DHT or turn the light/
  battery pots to cross a limit: the mode moves NORMAL↔WARNING↔ERROR, the **RGB
  LED** goes green / yellow / red, and the **buzzer** sounds on ERROR.
- **Alarm stop (§2.3.1)** — press **SW1 (D2/PA10)** to silence the buzzer.
- **Object detection (§2.2, §2.3.4)** — trigger the IR sensor → an `object
  DETECTED` event (LED red + buzzer); it clears when the object is gone.
- **Log module (§2.4/§2.5)** — `ls` lists day files, `cat 20260908.TXT` prints a
  day's log, `get <from> <to>` / `getev <from> <to>` retrieve records by time.
- **Config persistence (§2.6)** — `tn <lo> <hi>` / `tw <lo> <hi>` set limits;
  they are saved to `CONFIG.BIN` and survive a reset.
- **Protocol mode** — type `proto` to switch to the binary TLV machine protocol
  for the Central Computer (keep-alives every 6 s §2.8, events, data). The
  hardware watchdog (§2.9) keeps the system alive throughout.

(The firmware runs on FreeRTOS on the `freertos` branch and as a bare-metal
super-loop on `master`; behaviour and the serial interface are identical.)

### 3. Central Computer driving the board (§3)
With the board attached (it will be flipped into protocol mode automatically):
```bash
cd central && make central && ./central
```
Menu demo:
- **1 Listen** — watch `KEEP_ALIVE` lines stream in, and `EVENT` lines appear
  when you trip a mode change or the IR sensor (e.g. `EVENT src=MONITOR
  NORMAL->ERROR`, `EVENT src=OBJECT DETECTED`).
- **2 / 3** — get / set the LNC's RTC.
- **4 / 5 / 6** — management commands: set temp NORMAL / WARNING ranges, light
  NORMAL lower bound (§3.2); these persist on the board's SD card.
- **7 / 8** — retrieve stored **data** / **events** for a time range (read back
  from the board's SD log).
- **9 Report** — Data Collection & Analysis (§3.4): min/avg/max per field, mode
  counts, and event stats.
- **10 Save** — writes `measurements.csv` + `events.csv` (the on-disk database).

### 4. Ground Station ⇄ Central over Ethernet (§4)
The Ground Station is a TCP client of the Central. Two terminals:
```bash
# Terminal 1 — Central serves its database over TCP (the Ethernet link):
cd central && make central && ./central --serve 5555 sample_db

# Terminal 2 — Ground Station connects and queries a time range:
cd groundstation && make groundstation && ./groundstation 127.0.0.1 5555
```
In the Ground Station menu: **1** retrieves logged measurement data for a time
range, **2** retrieves events, **3** exits. Press Enter twice at the range
prompts to fetch everything. `sample_db/` is bundled demo data; to demo with
**real** data, capture it live with `./central` (menu **10** saves the CSVs) and
point `--serve` at that folder instead.

## Protocol

TLV tags are defined once in `lnc/src/core/protocol.h` and reused verbatim by the
Central (compiled as C++). The Ground Station ⇄ Central link adds its own tags in
`central/include/central/ground_protocol.h`, keeping the transport a detail of the
communication layer.
