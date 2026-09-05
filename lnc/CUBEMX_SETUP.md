# Submarine LNC — CubeMX setup checklist

Board: **Nucleo-L476RG** (STM32L476RGT3). Every setting below mirrors one of your
existing workspace example projects, so it's a configuration you already know works.
Do all of this in `Submarine.ioc`, then **Project → Generate Code**.

> **Clock first:** set the system clock to **80 MHz** (MSI+PLL, the L476 default max).
> All the timer prescalers below assume an 80 MHz timer clock (as in your examples).

## Conflict-free pin map

| Function | Pin(s) | Peripheral / mode | Mirror project |
|---|---|---|---|
| Central link | PA2, PA3 | USART2 Asynchronous, 115200 8N1 | (already in Submarine) |
| SD card | PA5 SCK, PA6 MISO, PA7 MOSI | SPI1 Full-Duplex Master | `SDCard` |
| SD chip-select | PB6 | GPIO_Output, label `SD_CS` | `SDCard` |
| Temp+humidity (DHT) | PB5 | GPIO (data line) + TIM5 timing | `DHT_Ex` |
| Buzzer / alarm | PB4 | TIM3_CH1 PWM | `BuzzPlay` |
| RGB LED — Red | PC0 | GPIO_Output, label `LED_R` | (new; PA5 taken by SPI) |
| RGB LED — Green | PC1 | GPIO_Output, label `LED_G` | |
| RGB LED — Blue | PC2 | GPIO_Output, label `LED_B` | |
| Button (stop alarm) | PC13 | GPIO_EXTI13 (on-board B1) | `RTC` / `Buttons` |
| RTC clock | PC14, PC15 | LSE oscillator | `RTC` |
| Battery (potentiometer) | *TBD* | ADC1 channel | *not decided yet* |
| Light sensor | *TBD* | ADC1 channel or I2C | *not decided yet* |
| Sonar (object) | *TBD* | TIM input-capture | *not decided yet* |

> "Yellow" = Red + Green LEDs both on.

## Peripheral-by-peripheral

- [ ] **USART2** — already enabled. Confirm Asynchronous, 115200 8N1. In **NVIC**, enable the **USART2 global interrupt** (needed for non-blocking RX). Mirror: it's the ST-Link virtual COM port.
- [ ] **SPI1 + FATFS (SD logs & config)** — copy `SDCard` exactly: SPI1 = Full-Duplex Master (PA5/PA6/PA7); PB6 = GPIO_Output `SD_CS`; add the **FATFS** middleware in *User-defined* mode. Then copy the SD `user_diskio.c` glue from your `SDCard`/`FlashMemoryTask` project.
- [ ] **TIM5 (DHT timing)** — copy `DHT_Ex`: TIM5, Prescaler **79**, Period **65535** (→ 1 MHz / 1 µs tick). DHT data on **PB5**.
- [ ] **TIM3_CH1 (buzzer PWM)** — copy `BuzzPlay`: TIM3, Prescaler **79**, Period **999**, PWM Generation CH1 on **PB4**.
- [ ] **RGB LED** — add PC0/PC1/PC2 as GPIO_Output, labels `LED_R` / `LED_G` / `LED_B`.
- [ ] **Button** — PC13 as GPIO_EXTI13; enable **EXTI line[15:10] interrupt** in NVIC. Mirror: `RTC` project's `USER_BUTTON`.
- [ ] **RTC** — copy `RTC`: enable RTC, clock source **LSE** (PC14/PC15), Calendar mode, Format = BIN. (If your board has no 32.768 kHz crystal, use **LSI** instead.)
- [ ] **IWDG (watchdog)** — copy `Watchdog`: Prescaler **32**, Reload **4095** (≈ 4 s). We'll refresh it every 500 ms in the super-loop.
- [ ] **Do NOT enable FreeRTOS** — the LNC uses a bare-metal super-loop (the `RTOS_Ex` project is not our model here).

## Deferred (battery / light / sonar)

When you decide these, enable **ADC1** with one channel each for battery and light
(e.g. PA0 = ADC1_IN5, PA1 = ADC1_IN6 — both free), and a timer input-capture for the
sonar echo. Tell me the parts/pins and I'll write those reads; until then they're
stubbed in `hal_sensors.c`.

## After generating code

Tell me and I'll: (1) drop the 14 tested core files into the project, (2) write the
platform layer (`hal_*`, `store_*`, `transport_uart`, `comm`) against these exact
handles — `huart2`, `hspi1`, `htim5`, `htim3`, `hrtc`, `hiwdg` — mirroring the example
drivers, and (3) write the super-loop `main` integration.
