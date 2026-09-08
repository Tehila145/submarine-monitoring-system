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
extern ADC_HandleTypeDef  hadc1;    /* battery + light (analog) */

#define BOARD_CENTRAL_UART   (&huart2)

/* --- RGB LED (GPIOC), active-high, mapping verified on the actual shield:
   PC7 = Red, PC8 = Green, PC6 = Blue. Configured in code by led_init(). */
#define BOARD_LED_R_PORT   GPIOC
#define BOARD_LED_R_PIN    GPIO_PIN_7
#define BOARD_LED_G_PORT   GPIOC
#define BOARD_LED_G_PIN    GPIO_PIN_8
#define BOARD_LED_B_PORT   GPIOC
#define BOARD_LED_B_PIN    GPIO_PIN_6
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

/* --- ADC1: battery = potentiometer on PA0 (IN5); light = LDR on PA1 (IN6) --- */
#define BOARD_BATTERY_CHANNEL  ADC_CHANNEL_5
#define BOARD_LIGHT_CHANNEL    ADC_CHANNEL_6

/* --- Object detection: IR sensor digital OUT on D6 = PB10 --- */
#define BOARD_IR_PORT   GPIOB
#define BOARD_IR_PIN    GPIO_PIN_10
/* Most IR obstacle sensors pull OUT LOW when an object is detected (active-low);
   confirmed/flipped via the `ir` console command + BOARD_IR_ACTIVE_LOW. */
#define BOARD_IR_ACTIVE_LOW  1

#endif
