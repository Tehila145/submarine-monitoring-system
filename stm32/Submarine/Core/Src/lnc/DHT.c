/*
 * DHT.c
 *
 *  Created on: Jun 20, 2026
 *      Author: tehilamenasheof
 */


#include "DHT.h"

#define DHT_TIMEOUT_US 120

static void DHT_SetPinOutput(DHT_HandleTypeDef *dht)
{
    GPIO_InitTypeDef gpioStruct = {0};

    gpioStruct.Pin = dht->pin;
    gpioStruct.Mode = GPIO_MODE_OUTPUT_PP;
    gpioStruct.Pull = GPIO_PULLUP;
    gpioStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(dht->port, &gpioStruct);
}

static void DHT_SetPinInput(DHT_HandleTypeDef *dht)
{
    GPIO_InitTypeDef gpioStruct = {0};

    gpioStruct.Pin = dht->pin;
    gpioStruct.Mode = GPIO_MODE_INPUT;
    gpioStruct.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(dht->port, &gpioStruct);
}

/* Iteration backstop: even if the TIM counter stalls, these loops must never
 * spin forever (a hang here would lock the whole super-loop and never feed the
 * watchdog). ~500k iterations is far longer than any real DHT pulse yet still
 * a small fraction of the IWDG timeout. */
#define DHT_LOOP_GUARD 500000u

static void DHT_DelayUs(DHT_HandleTypeDef *dht, uint16_t us)
{
    uint32_t guard = 0;
    __HAL_TIM_SET_COUNTER(dht->timer, 0);
    while (__HAL_TIM_GET_COUNTER(dht->timer) < us)
    {
        if (++guard > DHT_LOOP_GUARD) return;
    }
}

static uint8_t DHT_WaitForLevel(DHT_HandleTypeDef *dht, GPIO_PinState level)
{
    uint32_t guard = 0;
    __HAL_TIM_SET_COUNTER(dht->timer, 0);

    while (HAL_GPIO_ReadPin(dht->port, dht->pin) != level)
    {
        if (__HAL_TIM_GET_COUNTER(dht->timer) > DHT_TIMEOUT_US)
        {
            return 0;
        }
        if (++guard > DHT_LOOP_GUARD)
        {
            return 0;
        }
    }

    return 1;
}

void DHT_Init(DHT_HandleTypeDef *dht,
              GPIO_TypeDef *port,
              uint16_t pin,
              TIM_HandleTypeDef *timer)
{
    dht->port = port;
    dht->pin = pin;
    dht->timer = timer;

    DHT_SetPinOutput(dht);
    HAL_GPIO_WritePin(dht->port, dht->pin, GPIO_PIN_SET);
}

DHT_StatusTypeDef DHT_Read(DHT_HandleTypeDef *dht,
                           uint8_t *temperature,
                           uint8_t *humidity)
{
    uint8_t data[5] = {0};

    DHT_SetPinOutput(dht);

    HAL_GPIO_WritePin(dht->port, dht->pin, GPIO_PIN_RESET);
    HAL_Delay(18);

    HAL_GPIO_WritePin(dht->port, dht->pin, GPIO_PIN_SET);
    DHT_DelayUs(dht, 40);

    DHT_SetPinInput(dht);

    if (!DHT_WaitForLevel(dht, GPIO_PIN_RESET)) return DHT_ERROR_TIMEOUT;
    if (!DHT_WaitForLevel(dht, GPIO_PIN_SET))   return DHT_ERROR_TIMEOUT;
    if (!DHT_WaitForLevel(dht, GPIO_PIN_RESET)) return DHT_ERROR_TIMEOUT;

    for (int i = 0; i < 40; i++)
    {
        if (!DHT_WaitForLevel(dht, GPIO_PIN_SET)) return DHT_ERROR_TIMEOUT;

        __HAL_TIM_SET_COUNTER(dht->timer, 0);

        if (!DHT_WaitForLevel(dht, GPIO_PIN_RESET)) return DHT_ERROR_TIMEOUT;

        uint16_t pulseLength = __HAL_TIM_GET_COUNTER(dht->timer);

        data[i / 8] <<= 1;

        if (pulseLength > 40)
        {
            data[i / 8] |= 1;
        }
    }

    uint8_t checksum = data[0] + data[1] + data[2] + data[3];

    if (checksum != data[4])
    {
        return DHT_ERROR_CHECKSUM;
    }

    *humidity = data[0];
    *temperature = data[2];

    return DHT_OK;
}
