#include "hal_sensors.h"
#include "board.h"
#include "DHT.h"

/* Temperature + humidity: real DHT11 on PB5 + TIM5 (mirrors the DHT_Ex project).
 * Battery + light: real ADC1 single-channel reads (PA0/IN5 pot, PA1/IN6 LDR).
 * Object detection (sonar) is still stubbed until its hardware is chosen. */

/* ---- DHT (temperature + humidity) ---- */
static DHT_HandleTypeDef s_dht;
static bool     s_dht_init = false;
static uint8_t  s_temp = 20;   /* sensible in-range seed so a failed first */
static uint8_t  s_hum  = 50;   /* read before the sensor settles stays Normal */
static uint32_t s_dht_last = 0;

static void dht_refresh(void) {
    if (!s_dht_init) {
        HAL_TIM_Base_Start(BOARD_DHT_TIM);
        DHT_Init(&s_dht, BOARD_DHT_PORT, BOARD_DHT_PIN, BOARD_DHT_TIM);
        s_dht_init = true;
    }
    uint32_t now = HAL_GetTick();
    if (s_dht_last == 0 || (now - s_dht_last) >= 2000u) {   /* DHT11 min interval */
        uint8_t t, h;
        if (DHT_Read(&s_dht, &t, &h) == DHT_OK) { s_temp = t; s_hum = h; }
        s_dht_last = now;                                   /* keep last good on failure */
    }
}

int16_t  sensors_read_temperature(void) { dht_refresh(); return (int16_t)s_temp; }
uint16_t sensors_read_humidity(void)    { dht_refresh(); return (uint16_t)s_hum; }

/* ---- ADC (battery + light) ---- */
static bool s_adc_init = false;

static uint16_t adc_read(uint32_t channel) {
    if (!s_adc_init) {
        HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);  /* one-time */
        s_adc_init = true;
    }
    ADC_ChannelConfTypeDef c = {0};
    c.Channel      = channel;
    c.Rank         = ADC_REGULAR_RANK_1;
    c.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;   /* long: pot/LDR are high-impedance */
    c.SingleDiff   = ADC_SINGLE_ENDED;
    c.OffsetNumber = ADC_OFFSET_NONE;
    c.Offset       = 0;
    if (HAL_ADC_ConfigChannel(&hadc1, &c) != HAL_OK) return 0;
    if (HAL_ADC_Start(&hadc1) != HAL_OK)             return 0;
    uint16_t v = 0;
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
        v = (uint16_t)HAL_ADC_GetValue(&hadc1);    /* 0..4095 (12-bit) */
    HAL_ADC_Stop(&hadc1);
    return v;
}

uint16_t sensors_read_battery(void) { return adc_read(BOARD_BATTERY_CHANNEL); }
uint16_t sensors_read_light(void)   { return adc_read(BOARD_LIGHT_CHANNEL); }

/* ---- Object detection: IR sensor digital OUT on PB10 ---- */
static bool s_ir_init = false;
static void ir_ensure_init(void) {
    if (s_ir_init) return;
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin  = BOARD_IR_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;          /* safe if OUT is open-collector */
    HAL_GPIO_Init(BOARD_IR_PORT, &g);
    s_ir_init = true;
}

uint8_t sensors_ir_raw(void) {     /* instantaneous pin level, for the `ir` diagnostic */
    ir_ensure_init();
    return (HAL_GPIO_ReadPin(BOARD_IR_PORT, BOARD_IR_PIN) == GPIO_PIN_SET) ? 1 : 0;
}

/* The IR RECEIVER (remote type) outputs pulsed activity while a modulated IR
 * signal (a remote) is present. Sample it fast (sensors_ir_poll, called every
 * loop) and latch: "object present" = any activity within the last IR_HOLD_MS,
 * so a held remote button reads as a steady detection. */
#define IR_HOLD_MS 400u
static uint32_t s_ir_last = 0;

void sensors_ir_poll(void) {
    ir_ensure_init();
    uint8_t level = (HAL_GPIO_ReadPin(BOARD_IR_PORT, BOARD_IR_PIN) == GPIO_PIN_SET) ? 1 : 0;
#if BOARD_IR_ACTIVE_LOW
    bool active = (level == 0);
#else
    bool active = (level == 1);
#endif
    if (active) s_ir_last = HAL_GetTick();
}

bool sensors_object_present(void) {
    return s_ir_last != 0 && (HAL_GetTick() - s_ir_last) < IR_HOLD_MS;
}
