#ifndef LIMITS_H
#define LIMITS_H
#include "lnc_types.h"
lnc_mode_t limits_classify_range(int16_t v, int16_t nlo, int16_t nhi, int16_t wlo, int16_t whi);
lnc_mode_t limits_classify_lower(uint16_t v, uint16_t nlo, uint16_t wlo);
lnc_mode_t limits_evaluate(const measurement_t* m, const lnc_config_t* c);
#endif
