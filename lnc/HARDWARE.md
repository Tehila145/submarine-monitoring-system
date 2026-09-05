# Hardware Definition — required before on-target tasks

Fill these in from your Nucleo board and CubeMX `.ioc`. Each blank item blocks
the on-target task noted. Nothing here is guessed by the plan — the values are
yours to choose from your actual hardware.

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

## Time / safety (Task 13)
- [ ] RTC source (LSE/LSI) and epoch<->calendar conversion: __________
- [ ] IWDG timeout and chosen refresh period: __________

## Storage (Task 13)
- [ ] Config: reserved internal-Flash sector base + size: __________
- [ ] Logs: SD present? (SPI/SDIO + FatFS)  OR  external-flash scheme: __________

## Link (Task 14)
- [ ] Central UART instance + baud + pins: __________
- [ ] (Later) Ethernet PHY, if used: __________

---

## Status of the codebase (as built so far)

The entire hardware-independent core is implemented and host-tested (14 suites
passing via `make test`):

| Spec module | Pure core (done, tested) | On-target wrapper (this file gates) |
|---|---|---|
| Monitor        | `limits`, `monitor_logic`    | sensor reads @5s (Task 13/15) |
| Object Detect  | `objectdet_logic`            | sonar read (Task 13/15) |
| Event          | `event_decide`               | `led_set`/`alarm_set` + events file (Task 13/15) |
| Log            | `logplan`, `logline`         | `store_log_*` files (Task 13) |
| Communication  | `tlv`, `commq`, `command`, `records` | transport + drain (Task 14) |
| Configuration  | `config`                     | Flash persist (Task 13) |
| Init           | `init_logic`                 | sequence + time sync (Task 15) |
| Keep-Alive     | `keepalive`                  | 6 s send (Task 15) |
| Watchdog       | (reset cause in `init_logic`)| IWDG refresh (Task 13/15) |

Next step: create the CubeMX project (generating `main.h` with the handles/pins
named in `board.h`), fill this file and `src/platform/config_defaults.h`, then
implement Tasks 13–15 against `board.h`.
