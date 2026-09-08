#include "hal_watchdog.h"
#include "board.h"   /* hiwdg, HAL, RCC */

void wd_refresh(void) { HAL_IWDG_Refresh(&hiwdg); }

reset_cause_t reset_cause(void) {
    reset_cause_t c = __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) ? RESET_WATCHDOG : RESET_NORMAL;
    __HAL_RCC_CLEAR_RESET_FLAGS();
    return c;
}
