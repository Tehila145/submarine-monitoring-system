# FreeRTOS migration (branch `freertos`)

The bare-metal super-loop stays on `master`. This branch moves the LNC firmware
onto **FreeRTOS (CMSIS-OS v2)**. All the tested logic modules are reused
**unchanged** — only `lnc_app.c` (the wiring) is rewritten. `main.c`, `freertos.c`,
`FreeRTOSConfig.h`, and the kernel come from CubeMX.

## Why this is low-risk
Every module under `Core/Src/lnc/` (monitor_logic, objectdet_logic, event_decide,
keepalive, records, command, commq, config, init_logic, logline, store, tlv,
bytes …) is pure logic with no OS dependency. The super-loop already used an
event queue and TX priority queues — FreeRTOS just replaces those hand-rolled
rings with kernel objects, and replaces `sched_due()` polling with real periodic
tasks.

---

## Part A — CubeMX (you, in STM32CubeIDE)

> Do this on the `freertos` branch (`git switch freertos`) with the project open.
> Keep the changes minimal — we create the tasks/queues in code, not in CubeMX.

1. **Change the HAL timebase off SysTick** (required — FreeRTOS owns SysTick):
   `Pinout & Configuration → System Core → SYS → Timebase Source = TIM6`
   (any free basic timer; TIM6/TIM7 are ideal and unused here).
2. **Enable FreeRTOS:**
   `Middleware and Software Packs → FREERTOS → Interface = CMSIS_V2`.
3. **Config Parameters tab:**
   - `Memory Management scheme = heap_4`
   - `TOTAL_HEAP_SIZE ≈ 15360` (15 KB) — we allocate a handful of small tasks/queues.
   - `USE_MUTEXES = Enabled`, `USE_RECURSIVE_MUTEXES = Enabled` (FatFS wants it),
     `USE_COUNTING_SEMAPHORES = Enabled`.
   - `CHECK_FOR_STACK_OVERFLOW = Option2`, `USE_MALLOC_FAILED_HOOK = Enabled`
     (cheap safety during bring-up).
4. **Leave the auto-created `defaultTask`** as-is. We won't use CubeMX's Tasks &
   Queues panel — all our tasks/queues/mutexes are created from code so they live
   in version control, not the `.ioc`.
5. **FATFS:** it should already be set up. With FreeRTOS enabled, CubeMX wires
   FatFS's reentrancy to a FreeRTOS mutex automatically — no action needed, but
   confirm `FS_REENTRANT` didn't throw a warning.
6. **Generate Code** (gear icon). Commit the regenerated files:
   ```
   git add -A && git commit -m "Enable FreeRTOS (CMSIS-OS v2) via CubeMX"
   ```
7. Tell me it's done. If you can, paste the generated `Core/Src/freertos.c`
   (just the `MX_FREERTOS_Init` + `StartDefaultTask` skeleton) so I match the
   exact generated names.

### Expected generated changes
- `main.c`: adds `osKernelInitialize()`, `MX_FREERTOS_Init()`, `osKernelStart()`;
  the `while(1)` becomes unreachable (that's fine).
- New `Core/Src/freertos.c`, `Core/Inc/FreeRTOSConfig.h`.
- New `Middlewares/Third_Party/FreeRTOS/…`.
- TIM6 init added; SysTick handler no longer calls `HAL_IncTick` (FreeRTOS/TIM6 do).

---

## Part B — the task rewrite (me, after you regenerate)

I rewrite `Core/Src/lnc/lnc_app.c` to create these from `MX_FREERTOS_Init()`'s
USER CODE section. The module *step* functions (`monitor_task`, `objectdet_task`,
`keepalive_task`, `handle_events`, `exec_command`, `run_query`,
`process_binary_rx`, `process_console_rx`, the Log helpers, `console_exec`) are
reused verbatim.

### Tasks
| Task | Priority | Cadence | Job |
|------|----------|---------|-----|
| `InitTask` | RealTime | once, then becomes Watchdog | HW init that needs the scheduler (mount SD, load/persist config, post startup event), then loops as the watchdog |
| `CommTask` | AboveNormal | event-driven + 5 ms poll | drain RX ring → console/binary dispatch; drain TX priority queues → `transport_send` |
| `EventTask` | AboveNormal | blocks on `EventQueue` | `event_decide` → LED/alarm, log event, enqueue TX (proto) or print (console) |
| `ObjectTask` | Normal | 200 ms | `sensors_ir_poll` + `objectdet_logic_step` → post event; also the button check |
| `MonitorTask` | Normal | 5 s (`vTaskDelayUntil`) | read sensors, evaluate mode, log measurement, post transition event |
| `KeepAliveTask` | Normal | 6 s | build keep-alive → TX (protocol mode only) |

`InitTask` folds into the watchdog: after init it refreshes the IWDG every
~250 ms **only if** each supervised task has checked in since the last refresh
(a per-task "alive" bitmask) — so a hung task actually triggers the watchdog
reset, which is the point of the module.

### Kernel objects (replacing the hand-rolled rings)
- `EventQueue` = `osMessageQueueNew(8, sizeof(lnc_event_t), …)` — producers:
  Monitor, Object, Init, and config-changed in `exec_command`.
- `TxKA`, `TxEV`, `TxData` = three `osMessageQueue`s of `frame_t`; `CommTask`
  picks the next with the existing `commq_next()` using queue counts, preserving
  the spec's keep-alive > event > data priority.
- **RX ring stays as-is** — it's single-producer (ISR) / single-consumer
  (CommTask), so it's already lock-free; the ISR just needs nothing extra.

### Mutexes (shared hardware / state)
- `StoreMutex` — every `store_*` call (SD is touched by Monitor, Event, Comm).
- `UartMutex` — around `console_print()` and `transport_send()` (one owner of USART2 TX at a time).
- `StateMutex` — around `g_cfg` and `g_latest` read/modify (Monitor writes, KeepAlive/Comm read).

### Verification after the rewrite
- Headless CubeIDE build (see `HANDOFF.md`) must succeed.
- Flash + the same serial checks as bare-metal: console banner, `T=…` status,
  keep-alives in `proto` mode, events fire LED/alarm, SD logging, watchdog still
  resets on a deliberate hang.
- The host-side logic tests (`lnc/`, `central/`) are unaffected — same modules.

---

## Rollback
`master` is the working bare-metal build. `git switch master` returns to it at
any time; nothing on this branch touches it.
