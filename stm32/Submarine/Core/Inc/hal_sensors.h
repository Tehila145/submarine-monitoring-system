#ifndef HAL_SENSORS_H
#define HAL_SENSORS_H
#include <stdint.h>
#include <stdbool.h>
int16_t  sensors_read_temperature(void);
uint16_t sensors_read_humidity(void);
uint16_t sensors_read_light(void);
uint16_t sensors_read_battery(void);   /* ADC / potentiometer */
bool     sensors_object_present(void); /* IR object detection (latched) */
void     sensors_ir_poll(void);        /* sample IR fast; call every loop */
uint8_t  sensors_ir_raw(void);         /* raw IR pin level (0/1) for diagnostics */
#endif
