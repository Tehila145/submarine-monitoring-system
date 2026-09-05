#ifndef BOARD_H
#define BOARD_H
/* HARDWARE CONTRACT — fill every item from your CubeMX .ioc once the STM32
 * project exists. Platform code references ONLY these names. Do not hardcode
 * handles/pins elsewhere. Anything left as a TODO here blocks the on-target
 * tasks (13-15) until resolved. See HARDWARE.md for the full checklist. */

#include "main.h"   /* CubeMX-generated: HAL types + *_Pin / *_GPIO_Port macros */

/* --- Sensors --------------------------------------------------------------
 * TODO(board): which parts and buses? Declare the CubeMX handles you use and
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
