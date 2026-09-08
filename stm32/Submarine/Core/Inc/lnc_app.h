#ifndef LNC_APP_H
#define LNC_APP_H
/* LNC application: super-loop integration of the tested core modules.
 * Call lnc_app_init() once after the CubeMX MX_*_Init() calls, then
 * lnc_app_poll() every iteration of the main while(1) loop. */
void lnc_app_init(void);
void lnc_app_poll(void);
#endif
