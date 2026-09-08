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

## Build & run

Everything host-side uses a plain compiler + `make` (no external test framework).

**LNC logic unit tests** (pure C core, runs anywhere):
```bash
cd lnc && make test
```

**Fleet Management System:**
```bash
cd fleet && make test        # run the tests
cd fleet && make fleet && ./fleet   # run the interactive menu
```

**Central Computer** — drive the live board (auto-detects the serial port):
```bash
cd central && make central && ./central
```

**Ground Station ⇄ Central demo** (two terminals):
```bash
# Terminal 1 — Central serves its database over TCP (Ethernet link):
cd central && make central && ./central --serve 5555 sample_db

# Terminal 2 — Ground Station connects and queries a time range:
cd groundstation && make groundstation && ./groundstation 127.0.0.1 5555
```
`sample_db/` is bundled demo data. For real data, run `./central` against the
board, save the database with menu option 10, then point `--serve` at that folder.

**Firmware** — open `stm32/Submarine/` in STM32CubeIDE, build, and flash to a
Nucleo-L476RG. See `HANDOFF.md` for the pin map and the serial console commands.

## Protocol

TLV tags are defined once in `lnc/src/core/protocol.h` and reused verbatim by the
Central (compiled as C++). The Ground Station ⇄ Central link adds its own tags in
`central/include/central/ground_protocol.h`, keeping the transport a detail of the
communication layer.
