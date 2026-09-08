#ifndef CONFIG_H
#define CONFIG_H
#include "lnc_types.h"
#define LNC_CONFIG_MAGIC 0x4C4E4331u   /* "LNC1" */
void     config_load_defaults(lnc_config_t* c);
uint32_t config_serialize(const lnc_config_t* c, uint8_t* buf, uint32_t cap);
bool     config_deserialize(lnc_config_t* c, const uint8_t* buf, uint32_t len);
bool     config_apply_set(lnc_config_t* c, uint8_t cmd_tag, const uint8_t* val, uint8_t vlen);
#endif
