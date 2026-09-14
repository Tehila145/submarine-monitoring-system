# LNC End-Unit Firmware (STM32 Nucleo, C) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the Local Node Controller firmware for the Submarine Monitoring System — nine cooperating modules that sample the submarine's sensors, drive LED/alarm on mode and object changes, persist configuration and rotate logs, keep the watchdog fed, and exchange TLV messages with the Central Computer (including every management command and data/event retrieval) over a transport-independent link.

**Architecture:** A **non-blocking, timer-driven super-loop** (no RTOS). All decision logic lives in small, spec-aligned `.c/.h` modules split into a **pure part** (no hardware includes — host-testable) and a thin **on-target part** that reads inputs via **direct STM32 HAL** and calls the pure part. The Communication module depends on a **transport interface** (a header contract implemented by exactly one transport `.c`) so UART can be swapped for Ethernet with no change elsewhere — this is a compile-time seam, not a runtime object. Pure logic is covered by lightweight host tests (plain C + `assert`, one `Makefile`, no external test framework). Hardware behaviour is verified on the board with an explicit smoke checklist.

**Tech Stack:** C11, STM32 HAL (CubeMX-generated project), bare-metal super-loop. Host tests: plain C compiled with the system `cc`, driven by a `Makefile`. On-target build: STM32CubeIDE.

**Course context:** Embedded systems course, Google Reichman Tech School. This is a course project, not a production system — the methodology is deliberately lean (no RTOS, no mocking framework); functional coverage of the specification is complete.

**Spec:** `SUBMARINE_MONITORING_SYSTEM/final project.pdf` (§2 LNC End Unit, Figures 2–4) and the derived design doc `SUBMARINE_MONITORING_SYSTEM/architecture.html` (§1–10).

## Global Constraints

- **Language:** C11. The **pure** core sources (`src/core/*`) MUST NOT include any STM32 HAL header — they compile and test on the host with `cc -std=c11 -Wall -Wextra`. On-target sources (`src/platform/*`, `src/app/*`) use HAL directly.
- **Execution model:** one non-blocking super-loop. No blocking waits, no `HAL_Delay` in the loop path, no RTOS. Periodic work is dispatched by comparing a free-running millisecond counter against each job's due time.
- **Timing (from spec):** Monitor samples every **5 s**; Keep-Alive sends every **6 s**; Watchdog refresh on a schedule shorter than the IWDG timeout. These are the only fixed periods the spec sets.
- **Modes (spec §2.10):** exactly three — `MODE_NORMAL`, `MODE_WARNING`, `MODE_ERROR`. Overall mode is the worst of the four channels. Temperature is a **range** (Normal & Warning each have low+high bounds); humidity, light, and battery are **lower boundaries** only.
- **Transport independence (spec §2.5 NOTE):** the Communication module reaches the Central Computer only through the transport-interface header. No module names UART or Ethernet directly. The interface may be realised as a small set of functions selected at build time; a runtime vtable is **not** required.
- **TLV:** every wire message is Tag(1 byte) · Length(1 byte, 0–255) · Value; complex payloads nest child TLVs. **Numeric tag values are a project design decision** recorded in `protocol.h` and MUST match the Central Computer's protocol.
- **No invented hardware or magic numbers:** this plan does not fix a board model, pin, peripheral part, storage medium, CubeMX handle, or numeric configuration default. All such values are declared as named symbols in `board.h` / `config_defaults.h` for the team to fill from their own CubeMX `.ioc` and sensor choices (Task 12). The spec requires that defaults *exist* (§2.6) but does not specify their values.
- **No dynamic allocation in the core** — fixed-size, caller-provided buffers only.
- **Every pure task is red→green→commit against `make test` before moving on.** Attribution: end each commit message with `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.

**Testing boundary:** Tasks 1–11 are pure logic with host tests. Task 12 defines the hardware contract. Tasks 13–15 are on-target: direct-HAL platform code, the transport + command execution, and the super-loop wiring — each verified by an explicit on-target checklist, because hardware and timing behaviour cannot be meaningfully unit-tested off-target.

---

## File Structure

```
lnc/
  Makefile                         # host test build/run (pure core only)
  src/
    core/                          # PURE — no HAL includes, host-testable
      lnc_types.h                  # enums + config/measurement/event structs
      protocol.h                   # TLV tag numbers (project-defined; match Central)
      bytes.h   bytes.c            # little-endian pack/unpack helpers
      tlv.h     tlv.c              # TLV encode/decode
      limits.h  limits.c           # mode classification (Monitor logic)
      monitor_logic.h  .c          # measurement -> mode-change event (pure part of Monitor)
      objectdet_logic.h .c         # sonar edge -> object event (pure part of Object Detection)
      event_decide.h  .c           # event -> LED/alarm/ops actions (pure part of Event)
      logplan.h logplan.c          # 7-day rotation decision (pure part of Log)
      commq.h   commq.c            # TX priority selection (pure part of Communication)
      config.h  config.c           # defaults + (de)serialize + apply SET-command (pure part of Config)
      keepalive.h keepalive.c      # build keep-alive TLV
      records.h records.c          # build DATA_REPORT / EVENT_RECORD TLV frames
      command.h command.c          # parse + dispatch ALL management commands (pure)
      init_logic.h .c              # build startup event from reset cause + first-boot
    platform/                      # ON-TARGET — direct STM32 HAL
      board.h                      # HARDWARE CONTRACT: handles/pins/channels (team fills)
      config_defaults.h            # default limit values (team chooses; spec fixes none)
      hal_sensors.c                # sensor reads (temp/humidity/light/battery)
      hal_indicators.c             # RGB LED + buzzer + button
      hal_time.c                   # RTC get/set
      hal_watchdog.c               # IWDG refresh + reset cause
      store_config.c               # config persistence in internal Flash
      store_log.c                  # day-named log + events files
      transport.h                  # transport-independent interface (contract)
      transport_uart.c             # UART implementation of transport.h
      comm.c                       # RX framing + command execution + TX priority drain
    app/
      sched.h  sched.c             # non-blocking "is this job due?" helper
      main.c                       # HW init, init sequence, the super-loop
  test/
    test_util.h                    # tiny EXPECT() harness
    test_bytes.c  test_tlv.c  test_limits.c  test_monitor_logic.c
    test_objectdet_logic.c  test_event_decide.c  test_logplan.c
    test_commq.c  test_config.c  test_keepalive.c  test_records.c  test_command.c
```

Each `.c/.h` maps to one specification concern. The `_logic` suffix marks the pure half of a module whose other half touches hardware; the two halves share the module's data types from `lnc_types.h`.

---

### Task 1: Host test harness + core types + protocol tags

Stands up the plain-C test build and the shared types/tags every later task uses.

**Files:**
- Create: `lnc/Makefile`, `lnc/test/test_util.h`, `lnc/src/core/lnc_types.h`, `lnc/src/core/protocol.h`
- Test: `lnc/test/test_types.c`

**Interfaces:**
- Produces `lnc_types.h`:
  - `typedef enum { MODE_NORMAL=0, MODE_WARNING=1, MODE_ERROR=2 } lnc_mode_t;`
  - `typedef enum { LED_GREEN=0, LED_YELLOW=1, LED_RED=2 } led_color_t;`
  - `typedef enum { SRC_MONITOR=0, SRC_OBJECT, SRC_CONFIG, SRC_INIT } evt_src_t;`
  - `typedef enum { RESET_NORMAL=0, RESET_WATCHDOG=1 } reset_cause_t;`
  - `lnc_config_t`, `measurement_t`, `lnc_event_t` (fields below).
- Produces `protocol.h`: the project's TLV tag numbers.

- [ ] **Step 1: Write the failing test**

`lnc/test/test_util.h`:
```c
#ifndef TEST_UTIL_H
#define TEST_UTIL_H
#include <stdio.h>
static int g_fail = 0;
#define EXPECT(cond) do { if (!(cond)) { \
    printf("  FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); g_fail++; } } while (0)
#define REPORT() do { \
    if (g_fail) { printf("%d CHECK(S) FAILED\n", g_fail); return 1; } \
    printf("OK\n"); return 0; } while (0)
#endif
```

`lnc/test/test_types.c`:
```c
#include "test_util.h"
#include "lnc_types.h"

int main(void) {
    EXPECT(MODE_ERROR > MODE_WARNING);
    EXPECT(MODE_WARNING > MODE_NORMAL);
    measurement_t m = { .ts = 100, .temperature = -3, .humidity = 40,
                        .light = 500, .battery = 3300, .mode = MODE_NORMAL };
    EXPECT(m.ts == 100u);
    EXPECT(m.temperature == -3);
    EXPECT(m.battery == 3300);
    REPORT();
}
```

- [ ] **Step 2: Write the Makefile**

`lnc/Makefile`:
```make
CC      ?= cc
CFLAGS  := -std=c11 -Wall -Wextra -Isrc/core -Itest
CORE    := $(wildcard src/core/*.c)
TESTS   := $(wildcard test/test_*.c)

test:
	@set -e; for t in $(TESTS); do \
	  echo "== $$t =="; \
	  $(CC) $(CFLAGS) $$t $(CORE) -o /tmp/lnc_test; \
	  /tmp/lnc_test; \
	done; \
	echo "ALL HOST TESTS PASSED"

.PHONY: test
```
> Each test file has its own `main()` and is linked against all pure core sources. As core files are added they are picked up automatically by the wildcard.

- [ ] **Step 3: Run test to verify it fails**

Run: `cd lnc && make test`
Expected: FAIL — `lnc_types.h: No such file or directory`.

- [ ] **Step 4: Write minimal implementation**

`lnc/src/core/lnc_types.h`:
```c
#ifndef LNC_TYPES_H
#define LNC_TYPES_H
#include <stdint.h>
#include <stdbool.h>

typedef enum { MODE_NORMAL = 0, MODE_WARNING = 1, MODE_ERROR = 2 } lnc_mode_t;
typedef enum { LED_GREEN = 0, LED_YELLOW = 1, LED_RED = 2 } led_color_t;
typedef enum { SRC_MONITOR = 0, SRC_OBJECT, SRC_CONFIG, SRC_INIT } evt_src_t;
typedef enum { RESET_NORMAL = 0, RESET_WATCHDOG = 1 } reset_cause_t;

typedef struct {
    uint32_t magic;                        /* sentinel: valid vs first boot */
    int16_t  temp_norm_lo, temp_norm_hi;   /* temperature: a RANGE */
    int16_t  temp_warn_lo, temp_warn_hi;
    uint16_t hum_norm_lo,  hum_warn_lo;    /* others: LOWER boundaries */
    uint16_t light_norm_lo, light_warn_lo;
    uint16_t batt_norm_lo, batt_warn_lo;
} lnc_config_t;

typedef struct {
    uint32_t   ts;
    int16_t    temperature;
    uint16_t   humidity;
    uint16_t   light;
    uint16_t   battery;
    lnc_mode_t mode;
} measurement_t;

typedef struct {
    evt_src_t src;
    uint32_t  ts;
    union {
        struct { lnc_mode_t from, to; measurement_t m; } transition; /* SRC_MONITOR */
        struct { bool detected; } object;                            /* SRC_OBJECT  */
        struct { bool after_wd_reset; } startup;                     /* SRC_INIT    */
    } data;
} lnc_event_t;

#endif
```

`lnc/src/core/protocol.h`:
```c
#ifndef PROTOCOL_H
#define PROTOCOL_H
/* Project-defined TLV tag numbers. These MUST be identical in the Central
 * Computer's protocol definition. Values are a design choice, not fixed by
 * the specification; keep them stable once the Central side is written. */

/* Commands: Central -> LNC */
#define CMD_SET_TEMP_NORMAL    0x20  /* value: int16 lo, int16 hi */
#define CMD_SET_TEMP_WARNING   0x21  /* value: int16 lo, int16 hi */
#define CMD_SET_HUM_NORMAL     0x22  /* value: uint16 lower bound  */
#define CMD_SET_HUM_WARNING    0x23  /* value: uint16 lower bound  */
#define CMD_SET_LIGHT_NORMAL   0x24  /* value: uint16 lower bound  */
#define CMD_SET_LIGHT_WARNING  0x25  /* value: uint16 lower bound  */
#define CMD_SET_BATT_NORMAL    0x26  /* value: uint16 lower bound  */
#define CMD_SET_BATT_WARNING   0x27  /* value: uint16 lower bound  */
#define CMD_SET_RTC            0x28  /* value: uint32 epoch        */
#define CMD_GET_TIME           0x29  /* value: none                */
#define CMD_GET_DATA_RANGE     0x2A  /* value: uint32 from, uint32 to */
#define CMD_GET_EVENTS_RANGE   0x2B  /* value: uint32 from, uint32 to */

/* Reports/replies: LNC -> Central */
#define RPT_KEEPALIVE          0x01  /* nested: timestamp, measurement, mode */
#define RPT_EVENT              0x02  /* nested: an event record              */
#define RPT_DATA               0x03  /* nested: a measurement record         */
#define RSP_TIME               0x04  /* value: uint32 epoch                  */

/* Child tags used inside nested values */
#define TAG_TIMESTAMP          0x10
#define TAG_MEASUREMENT        0x11
#define TAG_MODE               0x12
#define TAG_EVENT_SRC          0x13

#endif
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cd lnc && make test`
Expected: PASS (`OK`, then `ALL HOST TESTS PASSED`).

- [ ] **Step 6: Commit**

```bash
git add lnc/Makefile lnc/test/test_util.h lnc/test/test_types.c lnc/src/core/lnc_types.h lnc/src/core/protocol.h
git commit -m "feat: host test harness, core types, and TLV tag definitions"
```

---

### Task 2: Little-endian byte helpers

**Files:**
- Create: `lnc/src/core/bytes.h`, `lnc/src/core/bytes.c`
- Test: `lnc/test/test_bytes.c`

**Interfaces:**
- Produces:
  - `void bytes_put_u16(uint8_t* p, uint16_t v);` / `uint16_t bytes_get_u16(const uint8_t* p);`
  - `void bytes_put_i16(uint8_t* p, int16_t v);` / `int16_t bytes_get_i16(const uint8_t* p);`
  - `void bytes_put_u32(uint8_t* p, uint32_t v);` / `uint32_t bytes_get_u32(const uint8_t* p);`
  - All little-endian.

- [ ] **Step 1: Write the failing test**

`lnc/test/test_bytes.c`:
```c
#include "test_util.h"
#include "bytes.h"

int main(void) {
    uint8_t b[4];
    bytes_put_u16(b, 0x1234);
    EXPECT(b[0] == 0x34 && b[1] == 0x12);
    EXPECT(bytes_get_u16(b) == 0x1234);
    bytes_put_i16(b, -2);
    EXPECT(bytes_get_i16(b) == -2);
    bytes_put_u32(b, 0x11223344u);
    EXPECT(b[0]==0x44 && b[3]==0x11);
    EXPECT(bytes_get_u32(b) == 0x11223344u);
    REPORT();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd lnc && make test`
Expected: FAIL — `bytes.h: No such file or directory`.

- [ ] **Step 3: Write minimal implementation**

`lnc/src/core/bytes.h`:
```c
#ifndef BYTES_H
#define BYTES_H
#include <stdint.h>

void     bytes_put_u16(uint8_t* p, uint16_t v);
uint16_t bytes_get_u16(const uint8_t* p);
void     bytes_put_i16(uint8_t* p, int16_t v);
int16_t  bytes_get_i16(const uint8_t* p);
void     bytes_put_u32(uint8_t* p, uint32_t v);
uint32_t bytes_get_u32(const uint8_t* p);

#endif
```

`lnc/src/core/bytes.c`:
```c
#include "bytes.h"

void bytes_put_u16(uint8_t* p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
uint16_t bytes_get_u16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1]<<8)); }
void bytes_put_i16(uint8_t* p, int16_t v) { bytes_put_u16(p, (uint16_t)v); }
int16_t bytes_get_i16(const uint8_t* p) { return (int16_t)bytes_get_u16(p); }
void bytes_put_u32(uint8_t* p, uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
uint32_t bytes_get_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd lnc && make test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add lnc/src/core/bytes.h lnc/src/core/bytes.c lnc/test/test_bytes.c
git commit -m "feat: little-endian byte pack/unpack helpers"
```

---

### Task 3: TLV codec

**Files:**
- Create: `lnc/src/core/tlv.h`, `lnc/src/core/tlv.c`
- Test: `lnc/test/test_tlv.c`

**Interfaces:**
- Produces:
  - `int tlv_write(uint8_t* buf, uint32_t cap, uint8_t tag, const uint8_t* val, uint8_t len);` — writes `[tag][len][val...]`; returns total bytes (`len+2`) or `-1` on overflow.
  - `int tlv_read(const uint8_t* buf, uint32_t len, uint8_t* tag, uint8_t* vlen, const uint8_t** val);` — parses one TLV; returns bytes consumed (`*vlen+2`) or `-1` if truncated.

- [ ] **Step 1: Write the failing test**

`lnc/test/test_tlv.c`:
```c
#include "test_util.h"
#include "tlv.h"

int main(void) {
    uint8_t buf[16];
    uint8_t payload[3] = {0xAA, 0xBB, 0xCC};
    int n = tlv_write(buf, sizeof(buf), 0x21, payload, 3);
    EXPECT(n == 5);
    EXPECT(buf[0] == 0x21 && buf[1] == 0x03);

    uint8_t tag, vlen; const uint8_t* val;
    int c = tlv_read(buf, (uint32_t)n, &tag, &vlen, &val);
    EXPECT(c == 5);
    EXPECT(tag == 0x21 && vlen == 3);
    EXPECT(val[0]==0xAA && val[2]==0xCC);

    uint8_t small[4], big[5] = {0};
    EXPECT(tlv_write(small, sizeof(small), 0x01, big, 5) == -1);   /* overflow */

    uint8_t trunc[2] = {0x21, 0x05};
    EXPECT(tlv_read(trunc, 2, &tag, &vlen, &val) == -1);           /* truncated */
    REPORT();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd lnc && make test` → FAIL (`tlv.h` not found).

- [ ] **Step 3: Write minimal implementation**

`lnc/src/core/tlv.h`:
```c
#ifndef TLV_H
#define TLV_H
#include <stdint.h>
int tlv_write(uint8_t* buf, uint32_t cap, uint8_t tag, const uint8_t* val, uint8_t len);
int tlv_read(const uint8_t* buf, uint32_t len, uint8_t* tag, uint8_t* vlen, const uint8_t** val);
#endif
```

`lnc/src/core/tlv.c`:
```c
#include "tlv.h"
#include <string.h>

int tlv_write(uint8_t* buf, uint32_t cap, uint8_t tag, const uint8_t* val, uint8_t len) {
    uint32_t total = (uint32_t)len + 2u;
    if (cap < total) return -1;
    buf[0] = tag; buf[1] = len;
    if (len) memcpy(&buf[2], val, len);
    return (int)total;
}

int tlv_read(const uint8_t* buf, uint32_t len, uint8_t* tag, uint8_t* vlen, const uint8_t** val) {
    if (len < 2u) return -1;
    uint8_t l = buf[1];
    if ((uint32_t)l + 2u > len) return -1;
    *tag = buf[0]; *vlen = l; *val = &buf[2];
    return (int)l + 2;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd lnc && make test` → PASS.

- [ ] **Step 5: Commit**

```bash
git add lnc/src/core/tlv.h lnc/src/core/tlv.c lnc/test/test_tlv.c
git commit -m "feat: TLV encode/decode with overflow and truncation guards"
```

---

### Task 4: Mode classification (limits)

**Files:**
- Create: `lnc/src/core/limits.h`, `lnc/src/core/limits.c`
- Test: `lnc/test/test_limits.c`

**Interfaces:**
- Produces:
  - `lnc_mode_t limits_classify_range(int16_t v, int16_t nlo, int16_t nhi, int16_t wlo, int16_t whi);`
  - `lnc_mode_t limits_classify_lower(uint16_t v, uint16_t nlo, uint16_t wlo);`
  - `lnc_mode_t limits_evaluate(const measurement_t* m, const lnc_config_t* c);` — worst of the four channels.

- [ ] **Step 1: Write the failing test**

`lnc/test/test_limits.c`:
```c
#include "test_util.h"
#include "limits.h"

int main(void) {
    /* range: normal 10..30, warning 0..40 */
    EXPECT(limits_classify_range(20, 10, 30, 0, 40) == MODE_NORMAL);
    EXPECT(limits_classify_range(35, 10, 30, 0, 40) == MODE_WARNING);
    EXPECT(limits_classify_range(5,  10, 30, 0, 40) == MODE_WARNING);
    EXPECT(limits_classify_range(45, 10, 30, 0, 40) == MODE_ERROR);
    EXPECT(limits_classify_range(-5, 10, 30, 0, 40) == MODE_ERROR);

    /* lower: normal >=600, warning >=400 */
    EXPECT(limits_classify_lower(700, 600, 400) == MODE_NORMAL);
    EXPECT(limits_classify_lower(500, 600, 400) == MODE_WARNING);
    EXPECT(limits_classify_lower(300, 600, 400) == MODE_ERROR);

    lnc_config_t c = {0};
    c.temp_norm_lo=10; c.temp_norm_hi=30; c.temp_warn_lo=0; c.temp_warn_hi=40;
    c.hum_norm_lo=40; c.hum_warn_lo=20; c.light_norm_lo=600; c.light_warn_lo=400;
    c.batt_norm_lo=3000; c.batt_warn_lo=2500;
    measurement_t m = { .temperature=20, .humidity=45, .light=700, .battery=3300 };
    EXPECT(limits_evaluate(&m, &c) == MODE_NORMAL);
    m.battery = 2700; EXPECT(limits_evaluate(&m, &c) == MODE_WARNING);
    m.temperature = 45; EXPECT(limits_evaluate(&m, &c) == MODE_ERROR);
    REPORT();
}
```
> These test-only limit numbers exercise the classifier; they are not the firmware's default configuration (that lives in `config_defaults.h`, Task 12, chosen by the team).

- [ ] **Step 2: Run test to verify it fails** — `cd lnc && make test` → FAIL.

- [ ] **Step 3: Write minimal implementation**

`lnc/src/core/limits.h`:
```c
#ifndef LIMITS_H
#define LIMITS_H
#include "lnc_types.h"
lnc_mode_t limits_classify_range(int16_t v, int16_t nlo, int16_t nhi, int16_t wlo, int16_t whi);
lnc_mode_t limits_classify_lower(uint16_t v, uint16_t nlo, uint16_t wlo);
lnc_mode_t limits_evaluate(const measurement_t* m, const lnc_config_t* c);
#endif
```

`lnc/src/core/limits.c`:
```c
#include "limits.h"

lnc_mode_t limits_classify_range(int16_t v, int16_t nlo, int16_t nhi, int16_t wlo, int16_t whi) {
    if (v >= nlo && v <= nhi) return MODE_NORMAL;
    if (v >= wlo && v <= whi) return MODE_WARNING;
    return MODE_ERROR;
}
lnc_mode_t limits_classify_lower(uint16_t v, uint16_t nlo, uint16_t wlo) {
    if (v >= nlo) return MODE_NORMAL;
    if (v >= wlo) return MODE_WARNING;
    return MODE_ERROR;
}
static lnc_mode_t worst(lnc_mode_t a, lnc_mode_t b) { return a > b ? a : b; }
lnc_mode_t limits_evaluate(const measurement_t* m, const lnc_config_t* c) {
    lnc_mode_t mode = limits_classify_range(m->temperature,
        c->temp_norm_lo, c->temp_norm_hi, c->temp_warn_lo, c->temp_warn_hi);
    mode = worst(mode, limits_classify_lower(m->humidity, c->hum_norm_lo, c->hum_warn_lo));
    mode = worst(mode, limits_classify_lower(m->light,   c->light_norm_lo, c->light_warn_lo));
    mode = worst(mode, limits_classify_lower(m->battery, c->batt_norm_lo, c->batt_warn_lo));
    return mode;
}
```

- [ ] **Step 4: Run test to verify it passes** — `cd lnc && make test` → PASS.

- [ ] **Step 5: Commit**

```bash
git add lnc/src/core/limits.h lnc/src/core/limits.c lnc/test/test_limits.c
git commit -m "feat: mode classification with range and lower-boundary limits"
```

---

### Task 5: Monitor logic (mode-change detection)

The pure half of the Monitor module: given the previous mode and a fresh measurement, decide whether a mode-change event fires. The on-target half (reading sensors every 5 s) is Task 13.

**Files:**
- Create: `lnc/src/core/monitor_logic.h`, `lnc/src/core/monitor_logic.c`
- Test: `lnc/test/test_monitor_logic.c`

**Interfaces:**
- Produces:
  - `void monitor_logic_reset(void);` — clears the "no previous sample yet" state.
  - `bool monitor_logic_step(const measurement_t* m_evaluated, lnc_event_t* out_event);` — `m_evaluated->mode` is already set by `limits_evaluate`; returns `true` and fills `out_event` (a `SRC_MONITOR` transition) only when the mode differs from the previous accepted sample. Updates internal previous-mode state each call.

- [ ] **Step 1: Write the failing test**

`lnc/test/test_monitor_logic.c`:
```c
#include "test_util.h"
#include "monitor_logic.h"

static measurement_t mk(lnc_mode_t mode, uint32_t ts) {
    measurement_t m = {0}; m.mode = mode; m.ts = ts; return m;
}

int main(void) {
    monitor_logic_reset();
    lnc_event_t e;
    measurement_t a = mk(MODE_NORMAL, 1000);
    EXPECT(monitor_logic_step(&a, &e) == false);   /* first sample: no change */

    measurement_t b = mk(MODE_ERROR, 1005);
    EXPECT(monitor_logic_step(&b, &e) == true);     /* normal -> error */
    EXPECT(e.src == SRC_MONITOR);
    EXPECT(e.data.transition.from == MODE_NORMAL);
    EXPECT(e.data.transition.to == MODE_ERROR);
    EXPECT(e.ts == 1005u);

    measurement_t c = mk(MODE_ERROR, 1010);
    EXPECT(monitor_logic_step(&c, &e) == false);    /* still error: no event */
    REPORT();
}
```

- [ ] **Step 2: Run test to verify it fails** — FAIL (`monitor_logic.h` not found).

- [ ] **Step 3: Write minimal implementation**

`lnc/src/core/monitor_logic.h`:
```c
#ifndef MONITOR_LOGIC_H
#define MONITOR_LOGIC_H
#include "lnc_types.h"
void monitor_logic_reset(void);
bool monitor_logic_step(const measurement_t* m_evaluated, lnc_event_t* out_event);
#endif
```

`lnc/src/core/monitor_logic.c`:
```c
#include "monitor_logic.h"

static bool s_have_prev;
static lnc_mode_t s_prev;

void monitor_logic_reset(void) { s_have_prev = false; s_prev = MODE_NORMAL; }

bool monitor_logic_step(const measurement_t* m, lnc_event_t* out) {
    bool changed = s_have_prev && (m->mode != s_prev);
    if (changed) {
        out->src = SRC_MONITOR;
        out->ts = m->ts;
        out->data.transition.from = s_prev;
        out->data.transition.to = m->mode;
        out->data.transition.m = *m;
    }
    s_prev = m->mode;
    s_have_prev = true;
    return changed;
}
```

- [ ] **Step 4: Run test to verify it passes** — PASS.

- [ ] **Step 5: Commit**

```bash
git add lnc/src/core/monitor_logic.h lnc/src/core/monitor_logic.c lnc/test/test_monitor_logic.c
git commit -m "feat: monitor mode-change detection logic"
```

---

### Task 6: Object-detection edge logic

Pure half of Object Detection: turn a present/absent reading into detected/cleared events on the edge only.

**Files:**
- Create: `lnc/src/core/objectdet_logic.h`, `lnc/src/core/objectdet_logic.c`
- Test: `lnc/test/test_objectdet_logic.c`

**Interfaces:**
- Produces:
  - `void objectdet_logic_reset(void);` — initial state = absent.
  - `bool objectdet_logic_step(bool present_now, uint32_t ts, lnc_event_t* out_event);` — returns `true` and fills a `SRC_OBJECT` event only on a change; `out_event->data.object.detected` reflects the new state.

- [ ] **Step 1: Write the failing test**

`lnc/test/test_objectdet_logic.c`:
```c
#include "test_util.h"
#include "objectdet_logic.h"

int main(void) {
    objectdet_logic_reset();
    lnc_event_t e;
    EXPECT(objectdet_logic_step(false, 1, &e) == false);  /* no change */
    EXPECT(objectdet_logic_step(true, 2, &e) == true);    /* detected */
    EXPECT(e.src == SRC_OBJECT && e.data.object.detected == true);
    EXPECT(objectdet_logic_step(true, 3, &e) == false);   /* still present */
    EXPECT(objectdet_logic_step(false, 4, &e) == true);   /* cleared */
    EXPECT(e.data.object.detected == false);
    REPORT();
}
```

- [ ] **Step 2: Run test to verify it fails** — FAIL.

- [ ] **Step 3: Write minimal implementation**

`lnc/src/core/objectdet_logic.h`:
```c
#ifndef OBJECTDET_LOGIC_H
#define OBJECTDET_LOGIC_H
#include "lnc_types.h"
void objectdet_logic_reset(void);
bool objectdet_logic_step(bool present_now, uint32_t ts, lnc_event_t* out_event);
#endif
```

`lnc/src/core/objectdet_logic.c`:
```c
#include "objectdet_logic.h"

static bool s_present;

void objectdet_logic_reset(void) { s_present = false; }

bool objectdet_logic_step(bool present_now, uint32_t ts, lnc_event_t* out) {
    if (present_now == s_present) return false;
    s_present = present_now;
    out->src = SRC_OBJECT;
    out->ts = ts;
    out->data.object.detected = present_now;
    return true;
}
```

- [ ] **Step 4: Run test to verify it passes** — PASS.

- [ ] **Step 5: Commit**

```bash
git add lnc/src/core/objectdet_logic.h lnc/src/core/objectdet_logic.c lnc/test/test_objectdet_logic.c
git commit -m "feat: object-detection edge logic"
```

---

### Task 7: Event decision logic

Pure half of the Event module (Figure 3 / §2.3.1–2.3.4): map an event to the LED/alarm/ops actions. Applying to hardware is Task 13.

**Files:**
- Create: `lnc/src/core/event_decide.h`, `lnc/src/core/event_decide.c`
- Test: `lnc/test/test_event_decide.c`

**Interfaces:**
- Produces:
  - `typedef struct { bool set_led; led_color_t led; bool set_alarm; bool alarm_on; bool resume_ops; bool suppress_ops; } event_action_t;`
  - `event_action_t event_decide(const lnc_event_t* e);`

- [ ] **Step 1: Write the failing test**

`lnc/test/test_event_decide.c`:
```c
#include "test_util.h"
#include "event_decide.h"

static lnc_event_t trans(lnc_mode_t f, lnc_mode_t t) {
    lnc_event_t e = {0}; e.src = SRC_MONITOR;
    e.data.transition.from = f; e.data.transition.to = t; return e;
}

int main(void) {
    lnc_event_t e;
    e = trans(MODE_NORMAL, MODE_WARNING);
    event_action_t a = event_decide(&e);
    EXPECT(a.set_led && a.led == LED_YELLOW && !a.set_alarm);

    e = trans(MODE_WARNING, MODE_ERROR); a = event_decide(&e);
    EXPECT(a.led == LED_RED && a.set_alarm && a.alarm_on && a.suppress_ops);

    e = trans(MODE_ERROR, MODE_WARNING); a = event_decide(&e);
    EXPECT(a.led == LED_YELLOW && a.set_alarm && !a.alarm_on && a.resume_ops);

    e = trans(MODE_WARNING, MODE_NORMAL); a = event_decide(&e);
    EXPECT(a.led == LED_GREEN && !a.set_alarm);

    e = trans(MODE_ERROR, MODE_NORMAL); a = event_decide(&e);
    EXPECT(a.led == LED_GREEN && a.set_alarm && !a.alarm_on && a.resume_ops);

    lnc_event_t od = {0}; od.src = SRC_OBJECT; od.data.object.detected = true;
    a = event_decide(&od);
    EXPECT(a.led == LED_RED && a.alarm_on);
    od.data.object.detected = false; a = event_decide(&od);
    EXPECT(a.led == LED_GREEN && a.set_alarm && !a.alarm_on);

    lnc_event_t cfg = {0}; cfg.src = SRC_CONFIG; a = event_decide(&cfg);
    EXPECT(!a.set_led && !a.set_alarm);   /* record-only */
    REPORT();
}
```

- [ ] **Step 2: Run test to verify it fails** — FAIL.

- [ ] **Step 3: Write minimal implementation**

`lnc/src/core/event_decide.h`:
```c
#ifndef EVENT_DECIDE_H
#define EVENT_DECIDE_H
#include "lnc_types.h"
typedef struct {
    bool set_led; led_color_t led;
    bool set_alarm; bool alarm_on;
    bool resume_ops; bool suppress_ops;
} event_action_t;
event_action_t event_decide(const lnc_event_t* e);
#endif
```

`lnc/src/core/event_decide.c`:
```c
#include "event_decide.h"

event_action_t event_decide(const lnc_event_t* e) {
    event_action_t a = {0};
    if (e->src == SRC_MONITOR) {
        lnc_mode_t from = e->data.transition.from;
        lnc_mode_t to   = e->data.transition.to;
        if (to == MODE_ERROR) {
            a.set_led = true; a.led = LED_RED;
            a.set_alarm = true; a.alarm_on = true; a.suppress_ops = true;
        } else if (to == MODE_WARNING) {
            a.set_led = true; a.led = LED_YELLOW;
            if (from == MODE_ERROR) { a.set_alarm = true; a.alarm_on = false; a.resume_ops = true; }
        } else {
            a.set_led = true; a.led = LED_GREEN;
            if (from == MODE_ERROR) { a.set_alarm = true; a.alarm_on = false; a.resume_ops = true; }
        }
    } else if (e->src == SRC_OBJECT) {
        if (e->data.object.detected) {
            a.set_led = true; a.led = LED_RED; a.set_alarm = true; a.alarm_on = true;
        } else {
            a.set_led = true; a.led = LED_GREEN; a.set_alarm = true; a.alarm_on = false;
        }
    }
    return a;   /* SRC_CONFIG / SRC_INIT: record-only */
}
```

- [ ] **Step 4: Run test to verify it passes** — PASS.

- [ ] **Step 5: Commit**

```bash
git add lnc/src/core/event_decide.h lnc/src/core/event_decide.c lnc/test/test_event_decide.c
git commit -m "feat: event decision logic for LED/alarm/ops"
```

---

### Task 8: Log rotation planning + Comm TX priority

Two small pure decisions grouped: the 7-day retention plan (Log) and the send-order selection (Communication). File writes and the real queues are on-target (Tasks 13, 15).

**Files:**
- Create: `lnc/src/core/logplan.h/.c`, `lnc/src/core/commq.h/.c`
- Test: `lnc/test/test_logplan.c`, `lnc/test/test_commq.c`

**Interfaces:**
- `logplan.h`:
  - `#define LNC_MAX_LOG_DAYS 7`
  - `bool logplan_evict(const char names[][16], int count, const char* today, char out_evict[16]);` — names sorted ascending; returns `true` with `out_evict` set to the oldest file when `today` is new and `count >= LNC_MAX_LOG_DAYS`; else `false`.
- `commq.h`:
  - `typedef enum { PRIO_KEEPALIVE=0, PRIO_EVENT=1, PRIO_DATA=2 } tx_prio_t;`
  - `typedef struct { bool has_keepalive, has_event, has_data; } commq_state_t;`
  - `int commq_next(const commq_state_t* s);` — highest-priority non-empty class, or `-1`.

- [ ] **Step 1: Write the failing tests**

`lnc/test/test_logplan.c`:
```c
#include "test_util.h"
#include "logplan.h"
#include <string.h>

int main(void) {
    char names[8][16]; char evict[16];
    /* today already present -> no eviction */
    strcpy(names[0], "2026-09-01"); strcpy(names[1], "2026-09-04");
    EXPECT(logplan_evict(names, 2, "2026-09-04", evict) == false);

    /* 7 existing, new day -> evict oldest (index 0) */
    const char* d[7] = {"2026-08-28","2026-08-29","2026-08-30","2026-08-31",
                        "2026-09-01","2026-09-02","2026-09-03"};
    for (int i=0;i<7;i++) strcpy(names[i], d[i]);
    EXPECT(logplan_evict(names, 7, "2026-09-04", evict) == true);
    EXPECT(strcmp(evict, "2026-08-28") == 0);
    REPORT();
}
```

`lnc/test/test_commq.c`:
```c
#include "test_util.h"
#include "commq.h"

int main(void) {
    commq_state_t s = { true, true, true };
    EXPECT(commq_next(&s) == PRIO_KEEPALIVE);
    s = (commq_state_t){ false, true, true };
    EXPECT(commq_next(&s) == PRIO_EVENT);
    s = (commq_state_t){ false, false, true };
    EXPECT(commq_next(&s) == PRIO_DATA);
    s = (commq_state_t){ false, false, false };
    EXPECT(commq_next(&s) == -1);
    REPORT();
}
```

- [ ] **Step 2: Run tests to verify they fail** — FAIL.

- [ ] **Step 3: Write minimal implementations**

`lnc/src/core/logplan.h`:
```c
#ifndef LOGPLAN_H
#define LOGPLAN_H
#include <stdbool.h>
#define LNC_MAX_LOG_DAYS 7
bool logplan_evict(const char names[][16], int count, const char* today, char out_evict[16]);
#endif
```

`lnc/src/core/logplan.c`:
```c
#include "logplan.h"
#include <string.h>

bool logplan_evict(const char names[][16], int count, const char* today, char out_evict[16]) {
    for (int i = 0; i < count; ++i)
        if (strcmp(names[i], today) == 0) return false;   /* today already open */
    if (count < LNC_MAX_LOG_DAYS) return false;
    strncpy(out_evict, names[0], 16);                     /* sorted asc -> oldest */
    out_evict[15] = '\0';
    return true;
}
```

`lnc/src/core/commq.h`:
```c
#ifndef COMMQ_H
#define COMMQ_H
#include <stdbool.h>
typedef enum { PRIO_KEEPALIVE=0, PRIO_EVENT=1, PRIO_DATA=2 } tx_prio_t;
typedef struct { bool has_keepalive, has_event, has_data; } commq_state_t;
int commq_next(const commq_state_t* s);
#endif
```

`lnc/src/core/commq.c`:
```c
#include "commq.h"
int commq_next(const commq_state_t* s) {
    if (s->has_keepalive) return PRIO_KEEPALIVE;
    if (s->has_event)     return PRIO_EVENT;
    if (s->has_data)      return PRIO_DATA;
    return -1;
}
```

- [ ] **Step 4: Run tests to verify they pass** — PASS.

- [ ] **Step 5: Commit**

```bash
git add lnc/src/core/logplan.h lnc/src/core/logplan.c lnc/src/core/commq.h lnc/src/core/commq.c lnc/test/test_logplan.c lnc/test/test_commq.c
git commit -m "feat: log-rotation planning and comm TX priority selection"
```

---

### Task 9: Config data logic — defaults, (de)serialize, apply SET-command

Pure half of Configuration: the in-memory config, its byte layout for Flash, and applying a single management SET-command to the config struct. Flash persistence itself is on-target (Task 13).

**Files:**
- Create: `lnc/src/core/config.h`, `lnc/src/core/config.c`
- Test: `lnc/test/test_config.c`

**Interfaces:**
- `config.h`:
  - `#define LNC_CONFIG_MAGIC 0x4C4E4331u`
  - `void config_load_defaults(lnc_config_t* c);` — sets `magic` and copies the team-chosen defaults from `config_defaults.h` (Task 12). **Note:** the pure build compiles with a test-only `config_defaults.h` shim on the host; the real values are supplied on target.
  - `uint32_t config_serialize(const lnc_config_t* c, uint8_t* buf, uint32_t cap);` — returns bytes written (0 on overflow).
  - `bool config_deserialize(lnc_config_t* c, const uint8_t* buf, uint32_t len);` — false if too short or wrong magic.
  - `bool config_apply_set(lnc_config_t* c, uint8_t cmd_tag, const uint8_t* val, uint8_t vlen);` — applies one `CMD_SET_*` limit command to `*c`; returns false on unknown tag or wrong value length. **This is the data-level implementation of the eight SET management commands.**

- [ ] **Step 1: Write the failing test**

`lnc/test/test_config.c`:
```c
#include "test_util.h"
#include "config.h"
#include "protocol.h"
#include "bytes.h"

int main(void) {
    lnc_config_t c; config_load_defaults(&c);
    EXPECT(c.magic == LNC_CONFIG_MAGIC);

    /* serialize/deserialize roundtrip */
    c.temp_norm_lo = 11; c.batt_warn_lo = 2222;
    uint8_t buf[128];
    uint32_t n = config_serialize(&c, buf, sizeof(buf));
    EXPECT(n > 0);
    lnc_config_t back;
    EXPECT(config_deserialize(&back, buf, n));
    EXPECT(back.temp_norm_lo == 11 && back.batt_warn_lo == 2222);

    /* bad magic rejected */
    uint8_t zero[128] = {0};
    EXPECT(config_deserialize(&back, zero, sizeof(zero)) == false);

    /* apply each SET command tag */
    uint8_t v4[4]; bytes_put_i16(v4, 5); bytes_put_i16(v4+2, 35);
    EXPECT(config_apply_set(&c, CMD_SET_TEMP_NORMAL, v4, 4));
    EXPECT(c.temp_norm_lo == 5 && c.temp_norm_hi == 35);
    bytes_put_i16(v4, -10); bytes_put_i16(v4+2, 50);
    EXPECT(config_apply_set(&c, CMD_SET_TEMP_WARNING, v4, 4));
    EXPECT(c.temp_warn_lo == -10 && c.temp_warn_hi == 50);

    uint8_t v2[2];
    bytes_put_u16(v2, 55); EXPECT(config_apply_set(&c, CMD_SET_HUM_NORMAL, v2, 2));  EXPECT(c.hum_norm_lo==55);
    bytes_put_u16(v2, 25); EXPECT(config_apply_set(&c, CMD_SET_HUM_WARNING, v2, 2)); EXPECT(c.hum_warn_lo==25);
    bytes_put_u16(v2, 650);EXPECT(config_apply_set(&c, CMD_SET_LIGHT_NORMAL, v2, 2));EXPECT(c.light_norm_lo==650);
    bytes_put_u16(v2, 450);EXPECT(config_apply_set(&c, CMD_SET_LIGHT_WARNING, v2,2));EXPECT(c.light_warn_lo==450);
    bytes_put_u16(v2, 3100);EXPECT(config_apply_set(&c, CMD_SET_BATT_NORMAL, v2,2)); EXPECT(c.batt_norm_lo==3100);
    bytes_put_u16(v2, 2600);EXPECT(config_apply_set(&c, CMD_SET_BATT_WARNING, v2,2));EXPECT(c.batt_warn_lo==2600);

    /* wrong length and unknown tag rejected */
    EXPECT(config_apply_set(&c, CMD_SET_HUM_NORMAL, v2, 1) == false);
    EXPECT(config_apply_set(&c, 0x99, v2, 2) == false);
    REPORT();
}
```

- [ ] **Step 2: Provide the host-side defaults shim**

Create `lnc/test/config_defaults.h` (host build only; the real one is Task 12). The `Makefile` already has `-Itest` so this is found first on the host:
```c
#ifndef CONFIG_DEFAULTS_H
#define CONFIG_DEFAULTS_H
/* Host-test placeholder values. The on-target config_defaults.h (Task 12)
 * carries the team's real chosen defaults; the spec does not fix them. */
#define DEF_TEMP_NORM_LO   10
#define DEF_TEMP_NORM_HI   30
#define DEF_TEMP_WARN_LO   0
#define DEF_TEMP_WARN_HI   40
#define DEF_HUM_NORM_LO    40
#define DEF_HUM_WARN_LO    20
#define DEF_LIGHT_NORM_LO  600
#define DEF_LIGHT_WARN_LO  400
#define DEF_BATT_NORM_LO   3000
#define DEF_BATT_WARN_LO   2500
#endif
```

- [ ] **Step 3: Run test to verify it fails** — FAIL (`config.h` not found).

- [ ] **Step 4: Write minimal implementation**

`lnc/src/core/config.h`:
```c
#ifndef CONFIG_H
#define CONFIG_H
#include "lnc_types.h"
#define LNC_CONFIG_MAGIC 0x4C4E4331u   /* "LNC1" */
void     config_load_defaults(lnc_config_t* c);
uint32_t config_serialize(const lnc_config_t* c, uint8_t* buf, uint32_t cap);
bool     config_deserialize(lnc_config_t* c, const uint8_t* buf, uint32_t len);
bool     config_apply_set(lnc_config_t* c, uint8_t cmd_tag, const uint8_t* val, uint8_t vlen);
#endif
```

`lnc/src/core/config.c`:
```c
#include "config.h"
#include "protocol.h"
#include "bytes.h"
#include "config_defaults.h"
#include <string.h>

void config_load_defaults(lnc_config_t* c) {
    c->magic = LNC_CONFIG_MAGIC;
    c->temp_norm_lo = DEF_TEMP_NORM_LO;  c->temp_norm_hi = DEF_TEMP_NORM_HI;
    c->temp_warn_lo = DEF_TEMP_WARN_LO;  c->temp_warn_hi = DEF_TEMP_WARN_HI;
    c->hum_norm_lo  = DEF_HUM_NORM_LO;   c->hum_warn_lo  = DEF_HUM_WARN_LO;
    c->light_norm_lo = DEF_LIGHT_NORM_LO; c->light_warn_lo = DEF_LIGHT_WARN_LO;
    c->batt_norm_lo = DEF_BATT_NORM_LO;  c->batt_warn_lo = DEF_BATT_WARN_LO;
}

uint32_t config_serialize(const lnc_config_t* c, uint8_t* buf, uint32_t cap) {
    if (cap < sizeof(lnc_config_t)) return 0;
    memcpy(buf, c, sizeof(lnc_config_t));
    return (uint32_t)sizeof(lnc_config_t);
}

bool config_deserialize(lnc_config_t* c, const uint8_t* buf, uint32_t len) {
    if (len < sizeof(lnc_config_t)) return false;
    lnc_config_t tmp; memcpy(&tmp, buf, sizeof(lnc_config_t));
    if (tmp.magic != LNC_CONFIG_MAGIC) return false;
    *c = tmp; return true;
}

bool config_apply_set(lnc_config_t* c, uint8_t tag, const uint8_t* v, uint8_t vlen) {
    switch (tag) {
        case CMD_SET_TEMP_NORMAL:
            if (vlen != 4) return false;
            c->temp_norm_lo = bytes_get_i16(v); c->temp_norm_hi = bytes_get_i16(v+2); return true;
        case CMD_SET_TEMP_WARNING:
            if (vlen != 4) return false;
            c->temp_warn_lo = bytes_get_i16(v); c->temp_warn_hi = bytes_get_i16(v+2); return true;
        case CMD_SET_HUM_NORMAL:    if (vlen!=2) return false; c->hum_norm_lo   = bytes_get_u16(v); return true;
        case CMD_SET_HUM_WARNING:   if (vlen!=2) return false; c->hum_warn_lo   = bytes_get_u16(v); return true;
        case CMD_SET_LIGHT_NORMAL:  if (vlen!=2) return false; c->light_norm_lo = bytes_get_u16(v); return true;
        case CMD_SET_LIGHT_WARNING: if (vlen!=2) return false; c->light_warn_lo = bytes_get_u16(v); return true;
        case CMD_SET_BATT_NORMAL:   if (vlen!=2) return false; c->batt_norm_lo  = bytes_get_u16(v); return true;
        case CMD_SET_BATT_WARNING:  if (vlen!=2) return false; c->batt_warn_lo  = bytes_get_u16(v); return true;
        default: return false;
    }
}
```

- [ ] **Step 5: Run test to verify it passes** — PASS.

- [ ] **Step 6: Commit**

```bash
git add lnc/src/core/config.h lnc/src/core/config.c lnc/test/config_defaults.h lnc/test/test_config.c
git commit -m "feat: config defaults, serialization, and SET-command application"
```

---

### Task 10: Keep-alive + record builders

Build the outbound frames: keep-alive (§2.8), a measurement data report, and an event record. These feed both Keep-Alive and the retrieval replies.

**Files:**
- Create: `lnc/src/core/keepalive.h/.c`, `lnc/src/core/records.h/.c`
- Test: `lnc/test/test_keepalive.c`, `lnc/test/test_records.c`

**Interfaces:**
- `keepalive.h`: `int keepalive_build(uint8_t* buf, uint32_t cap, uint32_t ts, const measurement_t* m);` — `RPT_KEEPALIVE` TLV nesting timestamp, measurement, and mode.
- `records.h`:
  - `int record_measurement(uint8_t* buf, uint32_t cap, const measurement_t* m);` — `RPT_DATA` TLV nesting timestamp + measurement + mode.
  - `int record_event(uint8_t* buf, uint32_t cap, const lnc_event_t* e);` — `RPT_EVENT` TLV nesting timestamp + event-source byte (+ mode transition when `SRC_MONITOR`).

- [ ] **Step 1: Write the failing tests**

`lnc/test/test_keepalive.c`:
```c
#include "test_util.h"
#include "keepalive.h"
#include "protocol.h"
#include "tlv.h"

int main(void) {
    measurement_t m = { .ts=0x11223344, .temperature=21, .humidity=40,
                        .light=700, .battery=3300, .mode=MODE_WARNING };
    uint8_t buf[64];
    int n = keepalive_build(buf, sizeof(buf), m.ts, &m);
    EXPECT(n > 0 && buf[0] == RPT_KEEPALIVE);

    uint8_t tag, vlen; const uint8_t* val;
    EXPECT(tlv_read(buf, (uint32_t)n, &tag, &vlen, &val) == n);
    /* first child = timestamp */
    uint8_t ct, cl; const uint8_t* cv;
    EXPECT(tlv_read(val, vlen, &ct, &cl, &cv) > 0);
    EXPECT(ct == TAG_TIMESTAMP && cl == 4);
    REPORT();
}
```

`lnc/test/test_records.c`:
```c
#include "test_util.h"
#include "records.h"
#include "protocol.h"
#include "tlv.h"

int main(void) {
    measurement_t m = { .ts=1000, .temperature=10, .humidity=50, .light=600,
                        .battery=3200, .mode=MODE_NORMAL };
    uint8_t buf[64];
    int n = record_measurement(buf, sizeof(buf), &m);
    EXPECT(n > 0 && buf[0] == RPT_DATA);

    lnc_event_t e = {0}; e.src = SRC_MONITOR; e.ts = 2000;
    e.data.transition.from = MODE_NORMAL; e.data.transition.to = MODE_ERROR;
    n = record_event(buf, sizeof(buf), &e);
    EXPECT(n > 0 && buf[0] == RPT_EVENT);
    uint8_t tag, vlen; const uint8_t* val;
    EXPECT(tlv_read(buf, (uint32_t)n, &tag, &vlen, &val) == n);
    REPORT();
}
```

- [ ] **Step 2: Run tests to verify they fail** — FAIL.

- [ ] **Step 3: Write minimal implementations**

`lnc/src/core/keepalive.h`:
```c
#ifndef KEEPALIVE_H
#define KEEPALIVE_H
#include "lnc_types.h"
int keepalive_build(uint8_t* buf, uint32_t cap, uint32_t ts, const measurement_t* m);
#endif
```

`lnc/src/core/keepalive.c`:
```c
#include "keepalive.h"
#include "protocol.h"
#include "tlv.h"
#include "bytes.h"

/* Shared helper: pack timestamp+measurement+mode child TLVs into `inner`. */
int lnc_pack_snapshot(uint8_t* inner, uint32_t cap, uint32_t ts, const measurement_t* m);

int keepalive_build(uint8_t* buf, uint32_t cap, uint32_t ts, const measurement_t* m) {
    uint8_t inner[48];
    int off = lnc_pack_snapshot(inner, sizeof(inner), ts, m);
    if (off < 0 || off > 255) return -1;
    return tlv_write(buf, cap, RPT_KEEPALIVE, inner, (uint8_t)off);
}
```

`lnc/src/core/records.h`:
```c
#ifndef RECORDS_H
#define RECORDS_H
#include "lnc_types.h"
int record_measurement(uint8_t* buf, uint32_t cap, const measurement_t* m);
int record_event(uint8_t* buf, uint32_t cap, const lnc_event_t* e);
/* shared with keepalive.c */
int lnc_pack_snapshot(uint8_t* inner, uint32_t cap, uint32_t ts, const measurement_t* m);
#endif
```

`lnc/src/core/records.c`:
```c
#include "records.h"
#include "protocol.h"
#include "tlv.h"
#include "bytes.h"

int lnc_pack_snapshot(uint8_t* inner, uint32_t cap, uint32_t ts, const measurement_t* m) {
    int off = 0, n;
    uint8_t tsb[4]; bytes_put_u32(tsb, ts);
    n = tlv_write(inner+off, cap-(uint32_t)off, TAG_TIMESTAMP, tsb, 4); if (n<0) return -1; off+=n;
    uint8_t meas[8];
    bytes_put_i16(meas+0, m->temperature); bytes_put_u16(meas+2, m->humidity);
    bytes_put_u16(meas+4, m->light);       bytes_put_u16(meas+6, m->battery);
    n = tlv_write(inner+off, cap-(uint32_t)off, TAG_MEASUREMENT, meas, 8); if (n<0) return -1; off+=n;
    uint8_t mode = (uint8_t)m->mode;
    n = tlv_write(inner+off, cap-(uint32_t)off, TAG_MODE, &mode, 1); if (n<0) return -1; off+=n;
    return off;
}

int record_measurement(uint8_t* buf, uint32_t cap, const measurement_t* m) {
    uint8_t inner[48];
    int off = lnc_pack_snapshot(inner, sizeof(inner), m->ts, m);
    if (off < 0 || off > 255) return -1;
    return tlv_write(buf, cap, RPT_DATA, inner, (uint8_t)off);
}

int record_event(uint8_t* buf, uint32_t cap, const lnc_event_t* e) {
    uint8_t inner[48]; int off = 0, n;
    uint8_t tsb[4]; bytes_put_u32(tsb, e->ts);
    n = tlv_write(inner+off, sizeof(inner)-(uint32_t)off, TAG_TIMESTAMP, tsb, 4); if (n<0) return -1; off+=n;
    uint8_t src = (uint8_t)e->src;
    n = tlv_write(inner+off, sizeof(inner)-(uint32_t)off, TAG_EVENT_SRC, &src, 1); if (n<0) return -1; off+=n;
    if (e->src == SRC_MONITOR) {
        uint8_t mm[2] = { (uint8_t)e->data.transition.from, (uint8_t)e->data.transition.to };
        n = tlv_write(inner+off, sizeof(inner)-(uint32_t)off, TAG_MODE, mm, 2); if (n<0) return -1; off+=n;
    }
    if (off > 255) return -1;
    return tlv_write(buf, cap, RPT_EVENT, inner, (uint8_t)off);
}
```

- [ ] **Step 4: Run tests to verify they pass** — PASS.

- [ ] **Step 5: Commit**

```bash
git add lnc/src/core/keepalive.h lnc/src/core/keepalive.c lnc/src/core/records.h lnc/src/core/records.c lnc/test/test_keepalive.c lnc/test/test_records.c
git commit -m "feat: keep-alive and data/event record TLV builders"
```

---

### Task 11: Command parser/dispatcher (ALL management commands) + init logic

The pure heart of the Communication RX path. `command_handle` recognises every management command, applies config SETs in place, and reports the side effects the on-target Comm task must perform (RTC set, time reply, range query). Also the pure Init startup-event builder.

**Files:**
- Create: `lnc/src/core/command.h/.c`, `lnc/src/core/init_logic.h/.c`
- Test: `lnc/test/test_command.c`

**Interfaces:**
- `command.h`:
  - `typedef enum { QUERY_NONE=0, QUERY_DATA, QUERY_EVENTS } query_kind_t;`
  - `typedef struct { bool handled; bool nack; bool config_changed; bool set_rtc; uint32_t rtc_epoch; query_kind_t query; uint32_t range_from, range_to; uint8_t reply[16]; uint8_t reply_len; } command_result_t;`
  - `command_result_t command_handle(const uint8_t* frame, uint32_t len, lnc_config_t* cfg, uint32_t now_epoch);` — parses one command TLV. Config SETs update `*cfg` and set `config_changed`. `CMD_SET_RTC` sets `set_rtc`+`rtc_epoch`. `CMD_GET_TIME` fills `reply` with an `RSP_TIME` TLV of `now_epoch`. `CMD_GET_DATA_RANGE`/`CMD_GET_EVENTS_RANGE` set `query` + range. Unknown tag → `handled=false`; malformed → `nack=true`.
- `init_logic.h`:
  - `void init_logic_startup_event(reset_cause_t cause, uint32_t ts, lnc_event_t* out);` — fills a `SRC_INIT` event with `after_wd_reset = (cause == RESET_WATCHDOG)`.

- [ ] **Step 1: Write the failing test**

`lnc/test/test_command.c`:
```c
#include "test_util.h"
#include "command.h"
#include "init_logic.h"
#include "protocol.h"
#include "tlv.h"
#include "bytes.h"

/* build a command frame [tag][len][val] into f, return length */
static uint32_t frame(uint8_t* f, uint8_t tag, const uint8_t* v, uint8_t n) {
    return (uint32_t)tlv_write(f, 32, tag, v, n);
}

int main(void) {
    lnc_config_t cfg; config_load_defaults(&cfg);
    uint8_t f[32];

    /* a config SET flows through command_handle */
    uint8_t v2[2]; bytes_put_u16(v2, 77);
    uint32_t n = frame(f, CMD_SET_HUM_NORMAL, v2, 2);
    command_result_t r = command_handle(f, n, &cfg, 5000);
    EXPECT(r.handled && r.config_changed && cfg.hum_norm_lo == 77);

    /* SET_RTC reports the epoch for the caller to apply */
    uint8_t v4[4]; bytes_put_u32(v4, 1700000000u);
    n = frame(f, CMD_SET_RTC, v4, 4);
    r = command_handle(f, n, &cfg, 5000);
    EXPECT(r.handled && r.set_rtc && r.rtc_epoch == 1700000000u);

    /* GET_TIME builds an RSP_TIME reply carrying now_epoch */
    n = frame(f, CMD_GET_TIME, NULL, 0);
    r = command_handle(f, n, &cfg, 12345);
    EXPECT(r.handled && r.reply_len > 0 && r.reply[0] == RSP_TIME);
    uint8_t tag, vlen; const uint8_t* val;
    EXPECT(tlv_read(r.reply, r.reply_len, &tag, &vlen, &val) > 0);
    EXPECT(vlen == 4 && bytes_get_u32(val) == 12345u);

    /* GET_DATA_RANGE / GET_EVENTS_RANGE parse the window */
    uint8_t v8[8]; bytes_put_u32(v8, 100); bytes_put_u32(v8+4, 200);
    n = frame(f, CMD_GET_DATA_RANGE, v8, 8);
    r = command_handle(f, n, &cfg, 0);
    EXPECT(r.query == QUERY_DATA && r.range_from == 100 && r.range_to == 200);
    n = frame(f, CMD_GET_EVENTS_RANGE, v8, 8);
    r = command_handle(f, n, &cfg, 0);
    EXPECT(r.query == QUERY_EVENTS);

    /* unknown tag / malformed length */
    n = frame(f, 0x99, v2, 2);
    r = command_handle(f, n, &cfg, 0);
    EXPECT(!r.handled);
    n = frame(f, CMD_SET_HUM_NORMAL, v2, 1);   /* wrong len */
    r = command_handle(f, n, &cfg, 0);
    EXPECT(r.handled && r.nack);

    /* init startup event */
    lnc_event_t e;
    init_logic_startup_event(RESET_WATCHDOG, 42, &e);
    EXPECT(e.src == SRC_INIT && e.ts == 42u && e.data.startup.after_wd_reset);
    init_logic_startup_event(RESET_NORMAL, 43, &e);
    EXPECT(!e.data.startup.after_wd_reset);
    REPORT();
}
```

- [ ] **Step 2: Run test to verify it fails** — FAIL (`command.h` not found).

- [ ] **Step 3: Write minimal implementations**

`lnc/src/core/command.h`:
```c
#ifndef COMMAND_H
#define COMMAND_H
#include "lnc_types.h"
#include "config.h"

typedef enum { QUERY_NONE=0, QUERY_DATA, QUERY_EVENTS } query_kind_t;

typedef struct {
    bool handled;          /* tag recognized                       */
    bool nack;             /* recognized but value malformed       */
    bool config_changed;   /* a SET_* was applied to *cfg          */
    bool set_rtc;          /* caller must rtc_set(rtc_epoch)        */
    uint32_t rtc_epoch;
    query_kind_t query;    /* caller runs a range query            */
    uint32_t range_from, range_to;
    uint8_t reply[16];     /* e.g. RSP_TIME frame to send back      */
    uint8_t reply_len;
} command_result_t;

command_result_t command_handle(const uint8_t* frame, uint32_t len,
                                lnc_config_t* cfg, uint32_t now_epoch);
#endif
```

`lnc/src/core/command.c`:
```c
#include "command.h"
#include "protocol.h"
#include "tlv.h"
#include "bytes.h"
#include <string.h>

command_result_t command_handle(const uint8_t* frame, uint32_t len,
                                lnc_config_t* cfg, uint32_t now_epoch) {
    command_result_t r; memset(&r, 0, sizeof(r));
    uint8_t tag, vlen; const uint8_t* val;
    if (tlv_read(frame, len, &tag, &vlen, &val) < 0) { return r; }  /* handled=false */

    switch (tag) {
        case CMD_SET_TEMP_NORMAL: case CMD_SET_TEMP_WARNING:
        case CMD_SET_HUM_NORMAL:  case CMD_SET_HUM_WARNING:
        case CMD_SET_LIGHT_NORMAL:case CMD_SET_LIGHT_WARNING:
        case CMD_SET_BATT_NORMAL: case CMD_SET_BATT_WARNING:
            r.handled = true;
            if (config_apply_set(cfg, tag, val, vlen)) r.config_changed = true;
            else r.nack = true;
            return r;
        case CMD_SET_RTC:
            r.handled = true;
            if (vlen != 4) { r.nack = true; return r; }
            r.set_rtc = true; r.rtc_epoch = bytes_get_u32(val);
            return r;
        case CMD_GET_TIME: {
            r.handled = true;
            uint8_t tb[4]; bytes_put_u32(tb, now_epoch);
            int n = tlv_write(r.reply, sizeof(r.reply), RSP_TIME, tb, 4);
            if (n < 0) { r.nack = true; return r; }
            r.reply_len = (uint8_t)n;
            return r;
        }
        case CMD_GET_DATA_RANGE:
        case CMD_GET_EVENTS_RANGE:
            r.handled = true;
            if (vlen != 8) { r.nack = true; return r; }
            r.query = (tag == CMD_GET_DATA_RANGE) ? QUERY_DATA : QUERY_EVENTS;
            r.range_from = bytes_get_u32(val);
            r.range_to   = bytes_get_u32(val + 4);
            return r;
        default:
            return r;   /* handled=false */
    }
}
```

`lnc/src/core/init_logic.h`:
```c
#ifndef INIT_LOGIC_H
#define INIT_LOGIC_H
#include "lnc_types.h"
void init_logic_startup_event(reset_cause_t cause, uint32_t ts, lnc_event_t* out);
#endif
```

`lnc/src/core/init_logic.c`:
```c
#include "init_logic.h"
#include <string.h>
void init_logic_startup_event(reset_cause_t cause, uint32_t ts, lnc_event_t* out) {
    memset(out, 0, sizeof(*out));
    out->src = SRC_INIT;
    out->ts = ts;
    out->data.startup.after_wd_reset = (cause == RESET_WATCHDOG);
}
```

- [ ] **Step 4: Run test to verify it passes** — PASS.

- [ ] **Step 5: Commit**

```bash
git add lnc/src/core/command.h lnc/src/core/command.c lnc/src/core/init_logic.h lnc/src/core/init_logic.c lnc/test/test_command.c
git commit -m "feat: management-command parser/dispatcher and init startup logic"
```

---

### Task 12: Hardware Definition contract (`board.h` + `config_defaults.h`)

**No STM32 project exists yet.** Before any on-target code, capture exactly what the hardware layer needs so later tasks reference named symbols instead of invented values. This task produces two fill-in headers and a decisions checklist — it does not invent pins, parts, or numbers.

**Files:**
- Create: `lnc/src/platform/board.h`, `lnc/src/platform/config_defaults.h`
- Create: `lnc/HARDWARE.md` (the decisions checklist)

**Interfaces:**
- Produces `board.h` — declared symbols the platform `.c` files (Tasks 13–15) will use, each to be filled from the team's CubeMX `.ioc`:

- [ ] **Step 1: Create the hardware contract**

`lnc/src/platform/board.h`:
```c
#ifndef BOARD_H
#define BOARD_H
/* HARDWARE CONTRACT — fill every item from your CubeMX .ioc once the STM32
 * project exists. Platform code references ONLY these names. Do not hardcode
 * handles/pins elsewhere. Anything left as a TODO here blocks the on-target
 * tasks (13-15) until resolved. */

#include "main.h"   /* CubeMX-generated: HAL types + *_Pin / *_GPIO_Port macros */

/* --- Sensors --------------------------------------------------------------
 * TODO(board): which parts and buses?  Declare the CubeMX handles you use and
 * document the read sequence for each sensor in HARDWARE.md. Examples of what
 * MUST be provided (names are yours to choose to match CubeMX):
 *   extern ADC_HandleTypeDef  hadc_battery;   // potentiometer channel
 *   #define BOARD_BATTERY_ADC_CHANNEL  ADC_CHANNEL_x
 *   extern I2C_HandleTypeDef  hi2c_env;        // temp/humidity (if I2C)
 *   // + light sensor interface, + sonar timer/pins
 */

/* --- RGB LED (3 lines) ----------------------------------------------------
 * TODO(board): LED_R_Pin/Port, LED_G_Pin/Port, LED_B_Pin/Port from CubeMX. */

/* --- Alarm / buzzer -------------------------------------------------------
 * TODO(board): ALARM_Pin/Port (GPIO) or a TIM PWM channel handle. */

/* --- Button (stops the alarm) --------------------------------------------
 * TODO(board): BUTTON_Pin + the EXTI line; note active-high/low. */

/* --- RTC ------------------------------------------------------------------
 * TODO(board): extern RTC_HandleTypeDef hrtc; and the epoch<->calendar policy. */

/* --- Watchdog -------------------------------------------------------------
 * TODO(board): extern IWDG_HandleTypeDef hiwdg; and the configured timeout. */

/* --- Config storage (internal Flash) -------------------------------------
 * TODO(board): reserved sector/bank base+size for the config blob. */

/* --- Log storage ----------------------------------------------------------
 * TODO(board): CONFIRM the medium. Is an SD card present (SPI/SDIO + FatFS)?
 * If not, define the external-flash/ring-buffer scheme for day-named logs. */

/* --- Central link (transport) --------------------------------------------
 * TODO(board): extern UART_HandleTypeDef huart_central; (+ Ethernet later). */

#endif
```

`lnc/src/platform/config_defaults.h` (real on-target defaults — the team chooses values appropriate to their sensors; the spec fixes none):
```c
#ifndef CONFIG_DEFAULTS_H
#define CONFIG_DEFAULTS_H
/* TODO(team): choose sensible first-boot defaults for YOUR sensors and units.
 * The specification (§2.6) requires defaults to exist but does not fix values.
 * Keep the macro names; set the numbers. */
#define DEF_TEMP_NORM_LO   /* TODO */ 0
#define DEF_TEMP_NORM_HI   /* TODO */ 0
#define DEF_TEMP_WARN_LO   /* TODO */ 0
#define DEF_TEMP_WARN_HI   /* TODO */ 0
#define DEF_HUM_NORM_LO    /* TODO */ 0
#define DEF_HUM_WARN_LO    /* TODO */ 0
#define DEF_LIGHT_NORM_LO  /* TODO */ 0
#define DEF_LIGHT_WARN_LO  /* TODO */ 0
#define DEF_BATT_NORM_LO   /* TODO */ 0
#define DEF_BATT_WARN_LO   /* TODO */ 0
#endif
```

`lnc/HARDWARE.md` — checklist of what must be decided before Tasks 13–15 (fill in as you build the CubeMX project):
```markdown
# Hardware Definition — required before on-target tasks

Fill these in from your Nucleo board and CubeMX .ioc. Each blank item blocks
the on-target task noted.

## Board
- [ ] Exact Nucleo/STM32 part number: __________
- [ ] Clock config / SysTick tick rate (for the ms scheduler): __________

## Sensors (Task 13)
- [ ] Temperature + humidity: part __________  bus __________  handle __________
- [ ] Light: part __________  interface (ADC/I2C) __________
- [ ] Battery (potentiometer): ADC handle __________  channel __________  scaling to config units __________
- [ ] Sonar (object detection): part __________  trigger/echo pins __________  timer __________

## Indicators (Task 13)
- [ ] RGB LED pins (R/G/B) and how "yellow" is produced: __________
- [ ] Buzzer pin or PWM channel: __________
- [ ] Button pin + EXTI line + active level: __________

## Time / safety (Tasks 13)
- [ ] RTC source (LSE/LSI) and epoch<->calendar conversion: __________
- [ ] IWDG timeout and chosen refresh period: __________

## Storage (Task 13)
- [ ] Config: reserved internal-Flash sector base + size: __________
- [ ] Logs: SD present? (SPI/SDIO + FatFS)  OR  external-flash scheme: __________

## Link (Task 14)
- [ ] Central UART instance + baud + pins: __________
- [ ] (Later) Ethernet PHY, if used: __________
```

- [ ] **Step 2: Verify the host build is unaffected**

Run: `cd lnc && make test`
Expected: PASS — `board.h` / on-target `config_defaults.h` are not in the pure build path (the host build uses `test/config_defaults.h`).

- [ ] **Step 3: Commit**

```bash
git add lnc/src/platform/board.h lnc/src/platform/config_defaults.h lnc/HARDWARE.md
git commit -m "docs: hardware definition contract and default-value placeholders"
```

> **Gate:** Tasks 13–15 require `HARDWARE.md` fully filled and a CubeMX project generating `main.h` with the declared handles/pins. If any item is still blank, resolve it with the team/course hardware before implementing that peripheral — do not guess.

---

### Task 13: On-target platform modules (direct HAL)

Implement each hardware module directly against STM32 HAL, using the `board.h` symbols. Not host-tested; verified in Task 15's smoke checklist. Build inside the CubeMX-generated STM32CubeIDE project with `src/core` and `src/platform` added to the include/source paths.

**Files:**
- Create: `hal_sensors.c`, `hal_indicators.c`, `hal_time.c`, `hal_watchdog.c`, `store_config.c`, `store_log.c` under `lnc/src/platform/`.

**Interfaces (module headers, consumed by the app in Task 15):**
- `int16_t sensors_read_temperature(void); uint16_t sensors_read_humidity(void); uint16_t sensors_read_light(void); uint16_t sensors_read_battery(void);`
- `void led_set(led_color_t c); void alarm_set(bool on); bool button_pressed_clear(void);` — `button_pressed_clear` returns and clears a flag set by the button EXTI ISR.
- `uint32_t rtc_now(void); void rtc_set(uint32_t epoch);`
- `void wd_refresh(void); reset_cause_t reset_cause(void);`
- `bool store_config_load(uint8_t* buf, uint32_t cap, uint32_t* out_len); bool store_config_save(const uint8_t* buf, uint32_t len);`
- `int store_log_list(char names[][16], int max); bool store_log_append(const char* name, const char* line); bool store_log_delete(const char* name); bool store_events_append(const char* line);`
- `int store_log_read_range(uint32_t from, uint32_t to, measurement_t* out, int max); int store_events_read_range(uint32_t from, uint32_t to, lnc_event_t* out, int max);` — used by the retrieval commands.

**Header files:** each platform module gets a matching `.h` in `lnc/src/platform/` declaring the functions above — `hal_sensors.h`, `hal_indicators.h`, `hal_time.h`, `hal_watchdog.h`. Storage functions are grouped into `store.h` (config + log + events + the range readers). One more small header, `event_bus.h`, declares `void event_post(const lnc_event_t* e);` — the Event module's inbound queue used by Monitor, Object Detection, Configuration (via Comm), and Init. Task 14's `comm.c` includes `store.h`, `event_bus.h`, `hal_time.h`, and `config.h`; these are the concrete file names for those includes.

**Log line format (writer/reader contract).** `store_log_append` and `store_log_read_range` MUST agree on one text format so a written line parses back into a `measurement_t`. Use fixed CSV, one record per line:

```
<ts>,<temperature>,<humidity>,<light>,<battery>,<mode>\n
```

where `ts` is decimal epoch seconds, `temperature` is a signed decimal, the rest are unsigned decimals, and `mode` is `0|1|2`. The events file uses:

```
<ts>,<src>,<from>,<to>,<detected>,<after_wd_reset>\n
```

with unused fields written as `0`. `store_log_read_range` / `store_events_read_range` parse these back and return only records whose `ts` is within `[from, to]`. Implement the format/parse as a tiny pure helper `logline.c/.h` (`logline_format_measurement`, `logline_parse_measurement`, `logline_format_event`, `logline_parse_event`) placed in `src/core` and covered by a `test/test_logline.c` roundtrip test, so the writer and reader cannot drift — add it to the host `Makefile` build the same way as the other pure modules, then commit it before the platform storage code uses it.

- [ ] **Step 1: Implement the indicator + button module (example)**

`lnc/src/platform/hal_indicators.c`:
```c
#include "lnc_types.h"
#include "board.h"
/* Uses LED_x_Pin / ALARM_Pin / BUTTON_Pin declared in board.h (from CubeMX). */

static volatile bool s_button;

void led_set(led_color_t c) {
    /* Requires: RGB LED pins from board.h. "Yellow" per HARDWARE.md wiring. */
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, (c==LED_RED)   ? GPIO_PIN_SET:GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, (c==LED_GREEN) ? GPIO_PIN_SET:GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, (c==LED_YELLOW)? GPIO_PIN_SET:GPIO_PIN_RESET);
}
void alarm_set(bool on) {
    HAL_GPIO_WritePin(ALARM_GPIO_Port, ALARM_Pin, on ? GPIO_PIN_SET:GPIO_PIN_RESET);
}
bool button_pressed_clear(void) { bool p = s_button; s_button = false; return p; }

/* Called from HAL_GPIO_EXTI_Callback in the app for BUTTON_Pin. */
void button_isr(void) { s_button = true; }
```

- [ ] **Step 2: Implement the remaining modules against `board.h`**

Implement `hal_sensors.c` (ADC read for battery/light, sensor bus reads for temp/humidity per HARDWARE.md; sonar present/absent read used by object detection), `hal_time.c` (RTC calendar↔epoch), `hal_watchdog.c` (`HAL_IWDG_Refresh`; `reset_cause` from `__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)` then clear flags), `store_config.c` (erase+write one reserved Flash sector; load reads it back), `store_log.c` (day-named files + events file per the storage decision in HARDWARE.md, plus the range readers). Each function is a thin wrapper; keep logic in the core modules.

> Where HARDWARE.md is still blank for a peripheral, stop and resolve it — do not substitute a guess.

- [ ] **Step 3: Build for target**

Build the STM32CubeIDE project with `src/core` + `src/platform` included.
Expected: compiles and links against the core library.

- [ ] **Step 4: Commit**

```bash
git add lnc/src/platform/hal_sensors.c lnc/src/platform/hal_indicators.c lnc/src/platform/hal_time.c lnc/src/platform/hal_watchdog.c lnc/src/platform/store_config.c lnc/src/platform/store_log.c
git commit -m "feat: STM32 platform modules (sensors, indicators, RTC, watchdog, storage)"
```

---

### Task 14: Transport interface + Communication service

The transport-independent seam and the Comm module that frames RX, executes command results, and drains the TX priority queue. Every management command is executed here explicitly.

**Files:**
- Create: `lnc/src/platform/transport.h`, `lnc/src/platform/transport_uart.c`, `lnc/src/platform/comm.c` (+ `comm.h`)

**Interfaces:**
- `transport.h` (the required transport-independent contract — compile-time seam, not a runtime vtable):
```c
#ifndef TRANSPORT_H
#define TRANSPORT_H
#include <stdint.h>
int transport_init(void);
int transport_send(const uint8_t* buf, uint32_t len);
/* non-blocking: returns 1 and fills *out_len if a full frame is available, else 0 */
int transport_poll(uint8_t* buf, uint32_t cap, uint32_t* out_len);
#endif
```
  Exactly one `.c` (here `transport_uart.c`) defines these three functions. Swapping to Ethernet means adding `transport_eth.c` and linking it instead — no other module changes. (Spec §2.5 NOTE satisfied without a vtable.)
- `comm.h`:
  - `void comm_init(lnc_config_t* cfg);`
  - `void comm_service(void);` — called every super-loop iteration: polls the transport; on a full frame, runs `command_handle` and **executes its result** (persist config on change + emit a `SRC_CONFIG` event; `rtc_set` on `set_rtc`; send `reply` if present; run the range query and stream `record_measurement`/`record_event` frames); then drains the TX queue in `commq` priority order.
  - `void comm_enqueue(tx_prio_t prio, const uint8_t* frame, uint8_t len);` — used by Keep-Alive/Event/Data producers.

- [ ] **Step 1: Implement the UART transport**

`lnc/src/platform/transport_uart.c` — implement the three `transport.h` functions over `huart_central` (from `board.h`) using non-blocking HAL (interrupt or DMA RX into a ring buffer; `transport_poll` extracts one complete TLV frame). Keep framing minimal: read `[tag][len]`, then `len` bytes.

- [ ] **Step 2: Implement the Comm service with explicit per-command execution**

`lnc/src/platform/comm.c` (core of the RX execution — each management command handled, none deferred):
```c
#include "comm.h"
#include "transport.h"
#include "command.h"
#include "records.h"
#include "commq.h"
#include "config.h"
#include "hal_time.h"     /* rtc_now, rtc_set */
#include "store.h"        /* store_config_save, store_log_read_range, store_events_read_range */
#include "event_bus.h"    /* event_post: hands a lnc_event_t to the Event module */

static lnc_config_t* s_cfg;

void comm_init(lnc_config_t* cfg) { s_cfg = cfg; transport_init(); }

static void run_query(const command_result_t* r) {
    uint8_t buf[64];
    if (r->query == QUERY_DATA) {
        measurement_t recs[64];
        int k = store_log_read_range(r->range_from, r->range_to, recs, 64);
        for (int i = 0; i < k; ++i) {
            int n = record_measurement(buf, sizeof(buf), &recs[i]);
            if (n > 0) comm_enqueue(PRIO_DATA, buf, (uint8_t)n);
        }
    } else if (r->query == QUERY_EVENTS) {
        lnc_event_t evs[64];
        int k = store_events_read_range(r->range_from, r->range_to, evs, 64);
        for (int i = 0; i < k; ++i) {
            int n = record_event(buf, sizeof(buf), &evs[i]);
            if (n > 0) comm_enqueue(PRIO_DATA, buf, (uint8_t)n);
        }
    }
}

void comm_service(void) {
    uint8_t frame[64]; uint32_t flen;
    if (transport_poll(frame, sizeof(frame), &flen)) {
        command_result_t r = command_handle(frame, flen, s_cfg, rtc_now());
        if (r.config_changed) {
            uint8_t blob[sizeof(lnc_config_t)];
            uint32_t bn = config_serialize(s_cfg, blob, sizeof(blob));
            store_config_save(blob, bn);
            lnc_event_t ce = { .src = SRC_CONFIG, .ts = rtc_now() };
            event_post(&ce);                 /* Event module writes it to events file + notifies */
        }
        if (r.set_rtc) rtc_set(r.rtc_epoch);
        if (r.reply_len) comm_enqueue(PRIO_DATA, r.reply, r.reply_len);
        if (r.query != QUERY_NONE) run_query(&r);
    }
    /* drain TX in priority order (implementation of commq over three ring buffers) */
    comm_drain_tx();
}
```
> `comm_enqueue`/`comm_drain_tx` maintain three fixed ring buffers and use `commq_next` to choose which to send via `transport_send`. `event_bus`/`store` headers group the platform functions from Task 13.

- [ ] **Step 3: Build for target** — compiles and links.

- [ ] **Step 4: Commit**

```bash
git add lnc/src/platform/transport.h lnc/src/platform/transport_uart.c lnc/src/platform/comm.c lnc/src/platform/comm.h
git commit -m "feat: transport-independent link and command-executing Comm service"
```

---

### Task 15: Super-loop wiring, init sequence, and on-target verification

Assemble everything into the non-blocking super-loop and verify every module and every management command on the board.

**Files:**
- Create: `lnc/src/app/sched.h`, `lnc/src/app/sched.c`, `lnc/src/app/main.c`
- Create/extend: `lnc/src/platform/monitor.c`, `objectdet.c`, `event.c`, `log.c`, `keepalive_send.c`, `watchdog.c` (thin on-target wrappers that call the core logic and the platform HAL).

**Interfaces:**
- `sched.h`: `bool sched_due(uint32_t* last_ms, uint32_t period_ms, uint32_t now_ms);` — returns true and advances `*last_ms` when due.

- [ ] **Step 1: Implement the scheduler helper**

`lnc/src/app/sched.c`:
```c
#include "sched.h"
bool sched_due(uint32_t* last, uint32_t period, uint32_t now) {
    if ((uint32_t)(now - *last) >= period) { *last = now; return true; }
    return false;
}
```
> `sched.c` is pure and MAY be added to the host `Makefile` with a `test_sched.c` (optional). It uses only `uint32_t` wrap-around arithmetic.

- [ ] **Step 2: Wire the init sequence + super-loop**

`lnc/src/app/main.c`:
```c
/* CubeMX generates HAL_Init, SystemClock_Config, MX_*_Init, HAL_GetTick(). */
#include "lnc_types.h"
#include "config.h"
#include "init_logic.h"
#include "monitor_logic.h"     /* via monitor.c wrapper */
#include "sched.h"
#include "comm.h"
#include "hal_watchdog.h"      /* wd_refresh, reset_cause */
#include "hal_time.h"          /* rtc_now */

static lnc_config_t g_cfg;

/* wrappers from platform: */
void monitor_task(void);       /* sample -> evaluate -> log + mode-change event */
void objectdet_task(void);     /* read sonar -> object event */
void event_task(void);         /* drain event queue -> decide -> apply + log + notify */
void keepalive_task(void);     /* build + enqueue keep-alive */
int  time_sync_request(void);  /* Init: ask Central for time via Comm (send request) */
bool time_is_synced(void);     /* true once the time-sync reply has been applied */

#define TIME_SYNC_TIMEOUT_MS 3000  /* team may tune; bounds boot if Central is absent */

int main(void) {
    /* HAL_Init(); SystemClock_Config(); MX_*_Init();  (CubeMX) */

    /* --- Init module: config load-or-default, reset cause, startup event --- */
    reset_cause_t cause = reset_cause();
    bool first_boot;
    {   uint8_t blob[sizeof(lnc_config_t)]; uint32_t bn = 0;
        if (store_config_load(blob, sizeof(blob), &bn) && config_deserialize(&g_cfg, blob, bn))
            first_boot = false;
        else { config_load_defaults(&g_cfg);
               bn = config_serialize(&g_cfg, blob, sizeof(blob));
               store_config_save(blob, bn); first_boot = true; }
    }
    comm_init(&g_cfg);
    monitor_logic_reset();

    /* §2.7: request time sync, then post the startup event ONCE SYNC COMPLETES.
     * time_sync_request() sends the request; the reply arrives asynchronously via
     * comm_service(), so pump the loop until time_is_synced() (bounded by a timeout
     * so a missing Central cannot hang boot). Only then is the startup event stamped
     * with a valid RTC time and posted to the Event module. */
    time_sync_request();
    { uint32_t t0 = HAL_GetTick();
      while (!time_is_synced() && (uint32_t)(HAL_GetTick() - t0) < TIME_SYNC_TIMEOUT_MS)
          comm_service();
      lnc_event_t se; init_logic_startup_event(cause, rtc_now(), &se); event_post(&se); }
    (void)first_boot;

    uint32_t t_mon = 0, t_ka = 0, t_wd = 0;
    for (;;) {
        uint32_t now = HAL_GetTick();          /* ms */
        comm_service();                        /* RX commands + TX drain, non-blocking */
        objectdet_task();                      /* poll sonar edge */
        event_task();                          /* handle queued events */
        if (sched_due(&t_mon, 5000, now)) monitor_task();
        if (sched_due(&t_ka,  6000, now)) keepalive_task();
        if (sched_due(&t_wd,   500, now)) wd_refresh();
    }
}
```
> The six `*_task()` wrappers are thin: `monitor_task` reads the four sensors, calls `limits_evaluate`, appends via the Log module (`logplan_evict` + `store_log_*`), and posts a `SRC_MONITOR` event on change via `monitor_logic_step`; `event_task` pops the event queue, calls `event_decide` + `led_set`/`alarm_set`, writes the events file, and enqueues a `record_event` to Comm; `keepalive_task` calls `keepalive_build` and `comm_enqueue(PRIO_KEEPALIVE, …)`; `objectdet_task` reads the sonar and calls `objectdet_logic_step`. The button EXTI stops the alarm (`alarm_set(false)` when `button_pressed_clear()`).

- [ ] **Step 3: On-target smoke verification**

Flash the board and record pass/fail for each — this is the acceptance test for the whole firmware:

| # | Check | Expected |
|---|---|---|
| 1 | Boot in-range | LED green; keep-alive frame on the Central UART every ~6 s |
| 2 | Drive one channel into Warning | LED yellow within ~5 s; event frame sent |
| 3 | Drive into Error | LED red + buzzer; `suppress non-essential ops`; event frame sent |
| 4 | Press button during Error | buzzer stops; LED stays red |
| 5 | Return all channels to range | LED green; buzzer off; event frame sent |
| 6 | Present object to sonar / remove | LED red+buzzer on detect; green on clear; event frames both edges |
| 7 | Log files | a day-named file grows; after 7 days the oldest is deleted (simulate by pre-seeding 7 files) |
| 8 | Watchdog | temporarily skip `wd_refresh` → board resets after IWDG timeout; next boot's startup event reports `after_wd_reset` |
| 9 | `CMD_SET_*` (all 8) | send each limit command → subsequent behaviour reflects new thresholds; config survives a power cycle (Flash persist) |
| 10 | `CMD_SET_RTC` then `CMD_GET_TIME` | `RSP_TIME` reply reflects the set time |
| 11 | `CMD_GET_DATA_RANGE` | LNC streams `RPT_DATA` frames within the window |
| 12 | `CMD_GET_EVENTS_RANGE` | LNC streams `RPT_EVENT` frames within the window |
| 13 | Priority | under load, keep-alive is not starved behind a large data-range reply |

- [ ] **Step 4: Commit**

```bash
git add lnc/src/app/ lnc/src/platform/monitor.c lnc/src/platform/objectdet.c lnc/src/platform/event.c lnc/src/platform/log.c lnc/src/platform/keepalive_send.c lnc/src/platform/watchdog.c
git commit -m "feat: super-loop wiring, init sequence, and on-target verification"
```

---

## Self-Review

**Spec coverage** (§2 LNC modules + management commands):
- 2.1 Monitor (5 s sample, compare, log, event on change) → Tasks 4, 5 (logic) + 13/15 (sampling) ✓
- 2.2 Object Detection (detect/clear → event) → Task 6 (logic) + 13/15 ✓
- 2.3 Event (per-source actions, LED/alarm, events file, notify Central) → Task 7 (logic) + 14/15 (apply + file + notify) ✓
- 2.4 Log (date files, 7-day retention) → Task 8 (plan) + 13 (`store_log_*`) + 15 (wiring) ✓
- 2.5 Communication (TLV, transport-independent, priority TX, RX commands) → Tasks 3, 8 (priority), 11 (parse), 14 (transport + execute) ✓
- 2.6 Configuration (limits, Flash persist, load-or-default, SET commands) → Task 9 (data + SET) + 13 (`store_config_*`) + 15 (init load-or-default) ✓
- 2.7 Init (config load, time sync, startup event incl. WD flag, start activities) → Task 11 (startup event) + 15 (sequence incl. `time_sync_request`) ✓
- 2.8 Keep-Alive (6 s, ts + measurement + mode) → Task 10 (build) + 15 (6 s send) ✓
- 2.9 Watchdog (refresh on schedule) → Task 11 (reset cause) + 13 (`wd_refresh`) + 15 (500 ms refresh) ✓
- 2.10 Operating modes (Normal/Warning/Error transitions) → Tasks 4, 7 ✓
- **Management commands (§2.5)** — each has explicit implementation + verification:
  - 8 × `SET_*` limits → `config_apply_set` (Task 9, per-tag host test) + executed in `comm_service` (Task 14) + smoke #9 ✓
  - `SET_RTC` → `command_handle` (Task 11 test) + `rtc_set` in `comm_service` (Task 14) + smoke #10 ✓
  - `GET_TIME` → `RSP_TIME` reply built in `command_handle` (Task 11 test) + sent in `comm_service` + smoke #10 ✓
  - `GET_DATA_RANGE` → parsed (Task 11 test) + `run_query` streams `RPT_DATA` (Task 14) + smoke #11 ✓
  - `GET_EVENTS_RANGE` → parsed (Task 11 test) + `run_query` streams `RPT_EVENT` (Task 14) + smoke #12 ✓

**Methodology simplification vs. the prior version:** RTOS → non-blocking super-loop (Task 15); Ceedling/Unity/CMock → plain-C `assert` harness + one `Makefile` (Task 1); full HAL mock architecture → pure `_logic` split with hardware verified on-target; runtime transport vtable → compile-time `transport.h` seam (Task 14). All functional requirements retained.

**No invented hardware/defaults:** board model, pins, parts, storage medium, CubeMX handles, and numeric config defaults are all deferred to the fill-in `board.h` / `config_defaults.h` / `HARDWARE.md` contract (Task 12), which Tasks 13–15 are gated on. The only concrete numbers in code are the spec's own periods (5 s / 6 s) and project-chosen TLV tag values (documented as a design decision that must match the Central Computer).

**Placeholder scan:** pure Tasks 1–11 contain complete, compilable code and tests. Tasks 12–15 contain concrete code plus clearly-marked `TODO(board)` items that are *deliberate* contract blanks (unknown until the STM32 project exists), not hidden logic gaps — each is gated and listed in `HARDWARE.md`.

**Type consistency:** `lnc_event_t` union fields, `event_action_t`, `command_result_t`, `commq_state_t`, and the `transport_*`/`store_*`/`rtc_*` signatures are used identically across the tasks that produce and consume them.
