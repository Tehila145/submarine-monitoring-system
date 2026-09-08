#ifndef LNC_APP_H
#define LNC_APP_H
/* LNC application — FreeRTOS (CMSIS-OS v2) integration of the tested core modules.
 *
 * Wiring in main.c:
 *   USER CODE 2 (before osKernelInitialize):  lnc_app_init();
 *   StartDefaultTask USER CODE 5:              lnc_app_start();
 *                                              for(;;){ lnc_app_watchdog_step(); osDelay(250); }
 *
 * lnc_app_init()          — pre-scheduler hardware prep (peripherals already MX_*_Init'd).
 * lnc_app_start()         — create queues/mutexes and the worker tasks (scheduler running).
 * lnc_app_watchdog_step() — one supervised IWDG refresh; run periodically from defaultTask. */
void lnc_app_init(void);
void lnc_app_start(void);
void lnc_app_watchdog_step(void);
#endif
