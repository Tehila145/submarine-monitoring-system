#ifndef BOARD_H
#define BOARD_H
/* HARDWARE CONTRACT for the Submarine LNC on Nucleo-L476RG.
 * Pin/peripheral choices mirror the workspace example projects (see
 * CUBEMX_SETUP.md). Platform code references ONLY these names. Fill the three
 * TODO(board) items (battery/light/sonar) once decided. */

#include "main.h"   /* CubeMX-generated HAL types + peripheral defines */

/* Peripheral handles created by CubeMX (declared in main.c). */
extern UART_HandleTypeDef huart2;   /* Central link (ST-Link VCP) */
extern SPI_HandleTypeDef  hspi1;    /* SD card (FatFS) */
extern TIM_HandleTypeDef  htim5;    /* DHT microsecond timing */
extern TIM_HandleTypeDef  htim3;    /* buzzer PWM (channel 1) */
extern RTC_HandleTypeDef  hrtc;     /* calendar / epoch */
extern IWDG_HandleTypeDef hiwdg;    /* independent watchdog */

#define BOARD_CENTRAL_UART   (&huart2)

/* --- RGB LED (PC0/PC1/PC2) --- */
#define BOARD_LED_R_PORT   GPIOC
#define BOARD_LED_R_PIN    GPIO_PIN_0
#define BOARD_LED_G_PORT   GPIOC
#define BOARD_LED_G_PIN    GPIO_PIN_1
#define BOARD_LED_B_PORT   GPIOC
#define BOARD_LED_B_PIN    GPIO_PIN_2
/* "Yellow" = Red + Green on. */

/* --- Buzzer: TIM3 channel 1 (PB4) --- */
#define BOARD_BUZZER_TIM      (&htim3)
#define BOARD_BUZZER_CHANNEL  TIM_CHANNEL_1

/* --- Button: on-board B1 (PC13, EXTI13) --- */
#define BOARD_BUTTON_PIN   GPIO_PIN_13   /* handled in HAL_GPIO_EXTI_Callback */

/* --- DHT temp/humidity: data on PB5, timing via TIM5 --- */
#define BOARD_DHT_PORT     GPIOB
#define BOARD_DHT_PIN      GPIO_PIN_5
#define BOARD_DHT_TIM      (&htim5)

/* --- SD card chip-select: PB6 --- */
#define BOARD_SD_CS_PORT   GPIOB
#define BOARD_SD_CS_PIN    GPIO_PIN_6

/* --- TODO(board): battery (potentiometer) — ADC1 channel; pin e.g. PA0. --- */
/* --- TODO(board): light sensor — ADC1 channel or I2C; pin TBD.          --- */
/* --- TODO(board): sonar (object) — TIM input-capture; trigger/echo TBD. --- */

#endif
