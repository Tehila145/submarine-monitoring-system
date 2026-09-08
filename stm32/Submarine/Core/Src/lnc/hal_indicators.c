#include "hal_indicators.h"
#include "board.h"   /* -> main.h (HAL types, pin defines, handles) */

static volatile bool s_button;

void led_init(void) {
    /* The RGB LED (PC6/PC7/PC8) is not configured by CubeMX, so set it up here. */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin   = BOARD_LED_R_PIN | BOARD_LED_G_PIN | BOARD_LED_B_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &g);   /* all three are on GPIOC */
    HAL_GPIO_WritePin(GPIOC, g.Pin, GPIO_PIN_RESET);   /* start off */
}

void led_set(led_color_t c) {
    /* Green=G, Yellow=R+G, Red=R. Blue LED unused by the three LNC modes. */
    uint8_t r = (c == LED_RED)   || (c == LED_YELLOW);
    uint8_t g = (c == LED_GREEN) || (c == LED_YELLOW);
    HAL_GPIO_WritePin(BOARD_LED_R_PORT, BOARD_LED_R_PIN, r ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BOARD_LED_G_PORT, BOARD_LED_G_PIN, g ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BOARD_LED_B_PORT, BOARD_LED_B_PIN, GPIO_PIN_RESET);
}

void alarm_set(bool on) {
    if (on) {
        __HAL_TIM_SET_COMPARE(BOARD_BUZZER_TIM, BOARD_BUZZER_CHANNEL, 500); /* 50% duty */
        HAL_TIM_PWM_Start(BOARD_BUZZER_TIM, BOARD_BUZZER_CHANNEL);
    } else {
        HAL_TIM_PWM_Stop(BOARD_BUZZER_TIM, BOARD_BUZZER_CHANNEL);
    }
}

bool button_pressed_clear(void) { bool p = s_button; s_button = false; return p; }
void indicators_button_isr(void) { s_button = true; }

/* Button EXTI -> flag. If CubeMX generated its own HAL_GPIO_EXTI_Callback
 * elsewhere, remove this one to avoid a duplicate definition. */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == BOARD_BUTTON_PIN) indicators_button_isr();
}
