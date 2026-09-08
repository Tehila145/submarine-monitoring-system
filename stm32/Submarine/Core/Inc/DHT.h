/*
 * DHT.h
 *
 *  Created on: Jun 20, 2026
 *      Author: tehilamenasheof
 */

#ifndef INC_DHT_H_
#define INC_DHT_H_

#include "main.h"
#include <stdint.h>

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    TIM_HandleTypeDef *timer;
} DHT_HandleTypeDef;

typedef enum
{
    DHT_OK = 0,
    DHT_ERROR_TIMEOUT,
    DHT_ERROR_CHECKSUM
} DHT_StatusTypeDef;

void DHT_Init(DHT_HandleTypeDef *dht,
              GPIO_TypeDef *port,
              uint16_t pin,
              TIM_HandleTypeDef *timer);

DHT_StatusTypeDef DHT_Read(DHT_HandleTypeDef *dht,
                           uint8_t *temperature,
                           uint8_t *humidity);


#endif /* INC_DHT_H_ */
