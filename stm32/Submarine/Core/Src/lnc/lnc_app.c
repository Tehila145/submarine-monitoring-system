#include "lnc_app.h"
#include "board.h"          /* HAL_GetTick, handles (hspi1) */
#include "main.h"           /* SD_CS_Pin / SD_CS_GPIO_Port */
#include "config.h"
#include "lnc_limits.h"
#include "monitor_logic.h"
#include "objectdet_logic.h"
#include "event_decide.h"
#include "keepalive.h"
#include "records.h"
#include "command.h"
#include "commq.h"
#include "init_logic.h"
#include "hal_sensors.h"
#include "hal_indicators.h"
#include "hal_time.h"
#include "hal_watchdog.h"
#include "transport.h"
#include "store.h"          /* SD-card logging (Log module) */
#include "logline.h"        /* CSV line format for log/events */
#include "cmsis_os2.h"      /* FreeRTOS via CMSIS-OS v2 */
#include "FreeRTOS.h"
#include "task.h"           /* taskENTER_CRITICAL for the watchdog check */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* --- MODE (runtime) ----------------------------------------------------------
 * Console mode (default): human-readable text + ASCII commands over USART2, so
 * you can drive it from `screen`. Protocol mode: the binary TLV machine protocol
 * (keep-alive/event/data frames + binary command RX) for a Central Computer.
 * Toggle to protocol with the `proto` command; RESET returns to console. */
static volatile bool s_protocol = false;

static const char* mode_name(lnc_mode_t m) {
    return m == MODE_ERROR ? "ERROR" : (m == MODE_WARNING ? "WARNING" : "NORMAL");
}

/* ---------------- FreeRTOS objects ---------------------------------------- */
#define FRAME_MAX 64
#define EVQ_N     8
#define TXQ_N     6

typedef struct { uint8_t buf[FRAME_MAX]; uint8_t len; } frame_t;

static osMessageQueueId_t EventQueue;      /* lnc_event_t producers -> EventTask */
static osMessageQueueId_t TxQ[3];          /* indexed by PRIO_KEEPALIVE/EVENT/DATA */
static osMutexId_t        UartMutex;       /* USART2 TX: console_print + transport_send */
static osMutexId_t        StoreMutex;      /* SD card (all store_* calls)               */
static osMutexId_t        StateMutex;      /* g_cfg / g_latest shared state             */

static inline void LOCK(osMutexId_t m)   { if (m) osMutexAcquire(m, osWaitForever); }
static inline void UNLOCK(osMutexId_t m) { if (m) osMutexRelease(m); }

/* Watchdog liveness: the hot loop (Comm + Object) must check in between refreshes. */
#define ALIVE_COMM (1u << 0)
#define ALIVE_OBJ  (1u << 1)
static volatile uint32_t g_alive;

static lnc_config_t  g_cfg;
static measurement_t g_latest;
static reset_cause_t g_reset_cause;
static bool          s_logging = false;

/* Register-level TX (no HAL lock), serialized by UartMutex so concurrent tasks
 * never interleave bytes on USART2. */
static void console_print(const char* s) {
    LOCK(UartMutex);
    while (*s) {
        while (!(USART2->ISR & USART_ISR_TXE)) { }
        USART2->TDR = (uint8_t)(*s++);
    }
    UNLOCK(UartMutex);
}

/* Persist the current config to the SD card (CONFIG.BIN) so limit changes survive
 * a reboot (spec §2.6). Snapshot g_cfg under StateMutex, write under StoreMutex —
 * never both at once, so no lock nesting. */
static void config_persist(void) {
    uint8_t buf[sizeof(lnc_config_t)]; uint32_t n;
    LOCK(StateMutex); n = config_serialize(&g_cfg, buf, sizeof(buf)); UNLOCK(StateMutex);
    if (n) { LOCK(StoreMutex); store_config_save(buf, n); UNLOCK(StoreMutex); }
}

/* ---------------- event + TX queues (FreeRTOS) ----------------------------- */
static void ev_post(const lnc_event_t* e) {
    if (EventQueue) osMessageQueuePut(EventQueue, e, 0, 0);   /* drop if full */
}
static void tx_enq(int prio, const uint8_t* f, uint8_t len) {
    if (len > FRAME_MAX || !TxQ[prio]) return;
    frame_t fr; memcpy(fr.buf, f, len); fr.len = len;
    osMessageQueuePut(TxQ[prio], &fr, 0, 0);                  /* drop if full */
}
static bool tx_has(int p) { return osMessageQueueGetCount(TxQ[p]) > 0; }
static void tx_drain(void) {
    for (;;) {
        commq_state_t s = { tx_has(PRIO_KEEPALIVE), tx_has(PRIO_EVENT), tx_has(PRIO_DATA) };
        int p = commq_next(&s);
        if (p < 0) break;
        frame_t fr;
        if (osMessageQueueGet(TxQ[p], &fr, NULL, 0) == osOK) {
            LOCK(UartMutex); transport_send(fr.buf, fr.len); UNLOCK(UartMutex);
        }
    }
}

/* ---------------- interrupt-driven RX + ASCII command console -------------- */
/* One byte at a time via HAL_UART_Receive_IT into a ring buffer; the callback
 * re-arms and an error callback recovers from overrun. Single-producer (ISR) /
 * single-consumer (CommTask), so the ring is lock-free. */
#define RXQ_N 128
static volatile uint8_t  rxq[RXQ_N];
static volatile uint16_t rx_head, rx_tail;
static uint8_t rx_byte;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        uint16_t n = (uint16_t)((rx_head + 1) % RXQ_N);
        if (n != rx_tail) { rxq[rx_head] = rx_byte; rx_head = n; }  /* drop if full */
        HAL_UART_Receive_IT(huart, &rx_byte, 1);                    /* re-arm */
    }
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        __HAL_UART_CLEAR_OREFLAG(huart);                            /* clear overrun */
        HAL_UART_Receive_IT(huart, &rx_byte, 1);                    /* re-arm */
    }
}
static void rx_start(void) {
    rx_head = rx_tail = 0;
    HAL_UART_Receive_IT(BOARD_CENTRAL_UART, &rx_byte, 1);
}

/* Execute one typed command line (bring-up console). Maps to the same underlying
 * operations as the TLV management commands. */
static void console_exec(char *line) {
    char *cmd = strtok(line, " \t");
    if (!cmd) return;
    if (!strcmp(cmd, "help")) {
        console_print("cmds: help | now | rtc <epoch> | tn <lo> <hi> | tw <lo> <hi>"
                      " | ls | cat <FILE> | get <from> <to> | getev <from> <to>"
                      " | led <c> | ir | proto\r\n");
    } else if (!strcmp(cmd, "ls")) {
        static char b[320];
        LOCK(StoreMutex); store_ls(b, sizeof(b)); UNLOCK(StoreMutex);
        console_print(b[0] ? b : "(no files / SD off)\r\n");
    } else if (!strcmp(cmd, "cat")) {
        char *a = strtok(NULL, " \t");
        if (!a) { console_print("usage: cat <FILE>  (e.g. cat EVENTS.TXT)\r\n"); }
        else {
            static char b[512];
            LOCK(StoreMutex); int n = store_cat(a, b, sizeof(b)); UNLOCK(StoreMutex);
            if (n < 0) console_print("cat: not found / SD off\r\n");
            else { console_print(b); if (n == 0) console_print("(empty)\r\n"); }
        }
    } else if (!strcmp(cmd, "get") || !strcmp(cmd, "getev")) {
        /* Range retrieval (spec §2.5): records whose timestamp is in [from,to]. */
        bool ev = !strcmp(cmd, "getev");
        char *a = strtok(NULL, " \t"), *b2 = strtok(NULL, " \t");
        uint32_t from = a  ? (uint32_t)strtoul(a,  NULL, 10) : 0u;
        uint32_t to   = b2 ? (uint32_t)strtoul(b2, NULL, 10) : 0xFFFFFFFFu;
        static char buf[768];
        LOCK(StoreMutex);
        int n = ev ? store_query_events(from, to, buf, sizeof(buf))
                   : store_query_data(from, to, buf, sizeof(buf));
        UNLOCK(StoreMutex);
        char h[48]; snprintf(h, sizeof(h), "-- %s: %d record(s) --\r\n",
                             ev ? "events" : "data", n);
        console_print(h);
        if (n > 0) console_print(buf);
    } else if (!strcmp(cmd, "now")) {
        char b[40]; snprintf(b, sizeof(b), "time=%lu\r\n", (unsigned long)rtc_now());
        console_print(b);
    } else if (!strcmp(cmd, "rtc")) {
        char *a = strtok(NULL, " \t");
        if (a) { rtc_set((uint32_t)strtoul(a, NULL, 10)); console_print("OK: rtc set\r\n"); }
        else console_print("usage: rtc <epoch>\r\n");
    } else if (!strcmp(cmd, "tn")) {
        char *lo = strtok(NULL, " \t"), *hi = strtok(NULL, " \t");
        if (lo && hi) { LOCK(StateMutex); g_cfg.temp_norm_lo = (int16_t)atoi(lo);
                        g_cfg.temp_norm_hi = (int16_t)atoi(hi); UNLOCK(StateMutex);
                        config_persist();
                        console_print("OK: temp normal set (saved)\r\n"); }
        else console_print("usage: tn <lo> <hi>\r\n");
    } else if (!strcmp(cmd, "tw")) {
        char *lo = strtok(NULL, " \t"), *hi = strtok(NULL, " \t");
        if (lo && hi) { LOCK(StateMutex); g_cfg.temp_warn_lo = (int16_t)atoi(lo);
                        g_cfg.temp_warn_hi = (int16_t)atoi(hi); UNLOCK(StateMutex);
                        config_persist();
                        console_print("OK: temp warning set (saved)\r\n"); }
        else console_print("usage: tw <lo> <hi>\r\n");
    } else if (!strcmp(cmd, "led")) {
        /* Diagnostic: drive the RGB pins directly (HIGH = on). */
        char *a = strtok(NULL, " \t");
        uint8_t r = 0, g = 0, b = 0;
        if (!a) { console_print("usage: led r|g|b|y|all|off\r\n"); }
        else {
            if      (!strcmp(a, "r"))   r = 1;
            else if (!strcmp(a, "g"))   g = 1;
            else if (!strcmp(a, "b"))   b = 1;
            else if (!strcmp(a, "y"))   { r = 1; g = 1; }
            else if (!strcmp(a, "all")) { r = 1; g = 1; b = 1; }
            HAL_GPIO_WritePin(BOARD_LED_R_PORT, BOARD_LED_R_PIN, r ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(BOARD_LED_G_PORT, BOARD_LED_G_PIN, g ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(BOARD_LED_B_PORT, BOARD_LED_B_PIN, b ? GPIO_PIN_SET : GPIO_PIN_RESET);
            console_print("led set (R/G/B pins driven HIGH=on)\r\n");
        }
    } else if (!strcmp(cmd, "ir")) {
        char b[48];
        snprintf(b, sizeof(b), "IR raw=%u  object=%s\r\n",
                 (unsigned)sensors_ir_raw(), sensors_object_present() ? "YES" : "no");
        console_print(b);
    } else if (!strcmp(cmd, "proto")) {
        console_print("Switching to BINARY PROTOCOL mode. Drive it with the host "
                      "'Central' tool; press RESET to return to console.\r\n");
        s_protocol = true;                 /* CommTask switches paths next iteration */
    } else {
        console_print("? unknown - type 'help'\r\n");
    }
}

/* Drain the RX ring, assemble lines, execute on CR/LF. Echoes typed characters. */
static void process_console_rx(void) {
    static char lb[64];
    static uint8_t ln;
    while (rx_tail != rx_head) {
        uint8_t c = rxq[rx_tail];
        rx_tail = (uint16_t)((rx_tail + 1) % RXQ_N);
        if (c == '\r' || c == '\n') {
            console_print("\r\n");
            if (ln > 0) { lb[ln] = '\0'; console_exec(lb); ln = 0; }
        } else if (c == '\b' || c == 0x7f) {          /* backspace / delete */
            if (ln > 0) { ln--; console_print("\b \b"); }
        } else if (c >= 0x20 && c < 0x7f && ln < sizeof(lb) - 1) {
            lb[ln++] = (char)c;
            char e[2] = { (char)c, 0 };
            console_print(e);                          /* echo */
        }
    }
}

/* ---------------- module steps -------------------------------------------- */
static void log_measurement(const measurement_t* m);   /* Log module (defined below) */
static void log_event(const lnc_event_t* e);

static void monitor_once(void) {
    measurement_t m;
    m.ts = rtc_now();
    m.temperature = sensors_read_temperature();
    m.humidity    = sensors_read_humidity();
    m.light       = sensors_read_light();
    m.battery     = sensors_read_battery();
    LOCK(StateMutex);
    m.mode   = limits_evaluate(&m, &g_cfg);
    g_latest = m;
    UNLOCK(StateMutex);
    log_measurement(&m);            /* Log module: append to today's day file */
    lnc_event_t e;
    if (monitor_logic_step(&m, &e)) ev_post(&e);
}

static void objectdet_once(void) {
    lnc_event_t e;
    if (objectdet_logic_step(sensors_object_present(), rtc_now(), &e)) ev_post(&e);
}

static void keepalive_once(void) {
    measurement_t snap;
    LOCK(StateMutex); snap = g_latest; UNLOCK(StateMutex);
    uint8_t f[FRAME_MAX];
    int n = keepalive_build(f, sizeof(f), rtc_now(), &snap);
    if (n > 0) tx_enq(PRIO_KEEPALIVE, f, (uint8_t)n);
}

/* Act on one event (spec §2.3): LED/alarm, log, and forward. */
static void handle_one_event(const lnc_event_t* e) {
    event_action_t a = event_decide(e);
    if (a.set_led)   led_set(a.led);
    if (a.set_alarm) alarm_set(a.alarm_on);
    log_event(e);   /* Log module: append to the events file (both modes) */
    if (s_protocol) {
        uint8_t f[FRAME_MAX];
        int n = record_event(f, sizeof(f), e);
        if (n > 0) tx_enq(PRIO_EVENT, f, (uint8_t)n);
    } else {
        char line[64];
        if (e->src == SRC_MONITOR)
            snprintf(line, sizeof(line), ">> EVENT: mode %s -> %s\r\n",
                     mode_name(e->data.transition.from), mode_name(e->data.transition.to));
        else if (e->src == SRC_OBJECT)
            snprintf(line, sizeof(line), ">> EVENT: object %s\r\n",
                     e->data.object.detected ? "DETECTED" : "cleared");
        else if (e->src == SRC_INIT)
            snprintf(line, sizeof(line), ">> EVENT: startup%s\r\n",
                     e->data.startup.after_wd_reset ? " (after watchdog reset)" : "");
        else
            snprintf(line, sizeof(line), ">> EVENT: config changed\r\n");
        console_print(line);
    }
}

static void run_query(const command_result_t* r);

/* Execute the side effects of a parsed command (protocol mode). */
static void exec_command(const command_result_t* r) {
    if (r->config_changed) {
        config_persist();            /* §2.6: persist config on change */
        lnc_event_t ce = {0}; ce.src = SRC_CONFIG; ce.ts = rtc_now();
        ev_post(&ce);
    }
    if (r->set_rtc)   rtc_set(r->rtc_epoch);
    if (r->reply_len) tx_enq(PRIO_DATA, r->reply, r->reply_len);
    if (r->query != QUERY_NONE) run_query(r);
}

/* Stream stored records in [from,to] back as TLV frames (GET_*_RANGE). */
static void run_query(const command_result_t* r) {
    static char blob[768];
    LOCK(StoreMutex);
    if (r->query == QUERY_EVENTS)
        store_query_events(r->range_from, r->range_to, blob, sizeof(blob));
    else
        store_query_data(r->range_from, r->range_to, blob, sizeof(blob));
    UNLOCK(StoreMutex);
    char* p = blob;
    while (*p) {
        char lbuf[80];
        char* nl = strchr(p, '\n');
        size_t L = nl ? (size_t)(nl - p) : strlen(p);
        if (L >= sizeof(lbuf)) L = sizeof(lbuf) - 1;
        memcpy(lbuf, p, L); lbuf[L] = '\0';
        uint8_t f[FRAME_MAX]; int n = 0;
        if (r->query == QUERY_EVENTS) {
            lnc_event_t e;
            if (logline_parse_event(lbuf, &e)) n = record_event(f, sizeof(f), &e);
        } else {
            measurement_t m;
            if (logline_parse_measurement(lbuf, &m)) n = record_measurement(f, sizeof(f), &m);
        }
        if (n > 0) tx_enq(PRIO_DATA, f, (uint8_t)n);
        if (!nl) break;
        p = nl + 1;
    }
}

/* Binary RX (protocol mode): assemble TLV frames from the interrupt ring. */
static void process_binary_rx(void) {
    static uint8_t fr[FRAME_MAX];
    static uint8_t have = 0, need = 0;
    while (rx_tail != rx_head) {
        uint8_t c = rxq[rx_tail];
        rx_tail = (uint16_t)((rx_tail + 1) % RXQ_N);
        if (have < FRAME_MAX) fr[have++] = c;
        if (have == 2) {
            need = (uint8_t)(2u + fr[1]);
            if (need > FRAME_MAX) { have = 0; need = 0; continue; }  /* desync guard */
        }
        if (have >= 2 && have == need) {
            command_result_t r;
            LOCK(StateMutex);
            r = command_handle(fr, have, &g_cfg, rtc_now());
            UNLOCK(StateMutex);
            exec_command(&r);
            have = 0; need = 0;
        }
    }
}

static void button_once(void) {
    if (button_pressed_clear()) alarm_set(false);   /* button stops the alarm */
}

/* ---------------- Log module (SD card) ------------------------------------ */
static void log_measurement(const measurement_t* m) {
    if (!s_logging) return;
    char day[9];  rtc_datestr8(day);            /* "YYYYMMDD" */
    char fname[16]; snprintf(fname, sizeof(fname), "%s.TXT", day);
    char line[64];
    if (logline_format_measurement(line, sizeof(line), m) > 0) {
        LOCK(StoreMutex); store_log_write(fname, line); UNLOCK(StoreMutex);  /* rotate(7d)+append */
    }
}

static void log_event(const lnc_event_t* e) {
    if (!s_logging) return;
    char line[64];
    if (logline_format_event(line, sizeof(line), e) > 0) {
        LOCK(StoreMutex); store_events_append(line); UNLOCK(StoreMutex);
    }
}

/* ---------------- tasks ---------------------------------------------------- */
/* Communication: RX dispatch + TX priority drain. Highest cadence; part of the
 * watchdog liveness set. */
static void CommTaskFn(void *arg) {
    (void)arg;
    for (;;) {
        if (s_protocol) process_binary_rx();
        else            process_console_rx();
        tx_drain();
        g_alive |= ALIVE_COMM;
        osDelay(5);
    }
}

/* Event: blocks on the event queue and acts on each event (LED/alarm/log/forward). */
static void EventTaskFn(void *arg) {
    (void)arg;
    lnc_event_t e;
    for (;;) {
        if (osMessageQueueGet(EventQueue, &e, NULL, osWaitForever) == osOK)
            handle_one_event(&e);
    }
}

/* Object detection: fast IR sampling (5 Hz object decision) + button; liveness set. */
static void ObjectTaskFn(void *arg) {
    (void)arg;
    uint32_t wake = osKernelGetTickCount();
    uint8_t  sub = 0;
    for (;;) {
        sensors_ir_poll();                 /* sample the IR receiver (latches) */
        button_once();
        if (++sub >= 4) { sub = 0; objectdet_once(); }   /* 4 * 50 ms = 200 ms */
        g_alive |= ALIVE_OBJ;
        wake += 50; osDelayUntil(wake);
    }
}

/* Monitor: also performs the one-time boot init that needs the scheduler
 * (mount SD, load/persist config, startup event, banner), then samples @5 s. */
static void MonitorTaskFn(void *arg) {
    (void)arg;

    /* --- one-time boot (runs in this big-stack task, scheduler active) --- */
    LOCK(StoreMutex); s_logging = store_init(); UNLOCK(StoreMutex);   /* mount SD */
    bool cfg_loaded = false;
    if (s_logging) {                        /* §2.6: load persisted config, else save defaults */
        uint8_t buf[sizeof(lnc_config_t)]; uint32_t n = 0;
        bool ok;
        LOCK(StoreMutex); ok = store_config_load(buf, sizeof(buf), &n); UNLOCK(StoreMutex);
        if (ok) { LOCK(StateMutex); ok = config_deserialize(&g_cfg, buf, n); UNLOCK(StateMutex); }
        if (ok) cfg_loaded = true;
        else    config_persist();           /* first boot: write defaults to CONFIG.BIN */
    }
    lnc_event_t se;
    init_logic_startup_event(g_reset_cause, rtc_now(), &se);
    ev_post(&se);
    console_print("\r\n=== LNC console (FreeRTOS) ===  (type 'help')\r\n");
    console_print(s_logging ? "SD: mounted - logging enabled\r\n"
                            : "SD: mount failed - logging disabled\r\n");
    console_print(cfg_loaded ? "Config: loaded from CONFIG.BIN\r\n"
                             : "Config: defaults\r\n");

    /* --- periodic sampling (spec §2.1: every 5 s) --- */
    uint32_t wake = osKernelGetTickCount();
    for (;;) {
        monitor_once();
        if (!s_protocol) {
            measurement_t snap;
            LOCK(StateMutex); snap = g_latest; UNLOCK(StateMutex);
            char line[80];
            snprintf(line, sizeof(line),
                     "T=%dC  H=%u%%  L=%u  B=%u  mode=%s\r\n",
                     (int)snap.temperature, (unsigned)snap.humidity,
                     (unsigned)snap.light, (unsigned)snap.battery, mode_name(snap.mode));
            console_print(line);
        }
        wake += 5000; osDelayUntil(wake);
    }
}

/* Keep-alive: every 6 s in protocol mode (spec §2.8). */
static void KeepAliveTaskFn(void *arg) {
    (void)arg;
    uint32_t wake = osKernelGetTickCount();
    for (;;) {
        if (s_protocol) keepalive_once();
        wake += 6000; osDelayUntil(wake);
    }
}

/* Re-apply the SD-over-SPI settings that CubeMX's generated init doesn't carry
 * (its .ioc leaves SPI1 at 4-bit / MISO no-pull / CS low). Kept here — in our own
 * code — so they survive any future CubeMX regeneration rather than being silently
 * reverted in the generated files. Required by user_diskio_spi.c. */
static void sd_spi_fixups(void) {
    /* MISO (PA6) needs a pull-up so it idles high when the card isn't driving. */
    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_6;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);
    /* SD needs 8-bit SPI frames (CubeMX default here is 4-bit). */
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    HAL_SPI_Init(&hspi1);
    /* Deselect the card (CS idle high) before the FatFS driver runs. */
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
}

/* ---------------- lifecycle ------------------------------------------------ */
/* Pre-scheduler prep: peripherals are already MX_*_Init'd. No SD, no RTOS objects
 * (mutexes are NULL here, so LOCK/UNLOCK are no-ops and the single-threaded reads
 * below are safe). */
void lnc_app_init(void) {
    g_reset_cause = reset_cause();
    config_load_defaults(&g_cfg);
    monitor_logic_reset();
    objectdet_logic_reset();
    sd_spi_fixups();                /* SD needs 8-bit SPI / MISO pull-up / CS high */
    transport_init();
    rx_start();                     /* arm interrupt-driven command RX */
    led_init();                     /* configure RGB pins */
    led_set(LED_GREEN);
    /* prime g_latest so the first keep-alive has data (no event posting yet) */
    measurement_t m;
    m.ts = rtc_now();
    m.temperature = sensors_read_temperature();
    m.humidity    = sensors_read_humidity();
    m.light       = sensors_read_light();
    m.battery     = sensors_read_battery();
    m.mode        = limits_evaluate(&m, &g_cfg);
    g_latest = m;
}

/* Create the RTOS objects and worker tasks. Called from the default task once the
 * scheduler is running. */
void lnc_app_start(void) {
    UartMutex  = osMutexNew(NULL);
    StoreMutex = osMutexNew(NULL);
    StateMutex = osMutexNew(NULL);
    EventQueue = osMessageQueueNew(EVQ_N, sizeof(lnc_event_t), NULL);
    TxQ[PRIO_KEEPALIVE] = osMessageQueueNew(TXQ_N, sizeof(frame_t), NULL);
    TxQ[PRIO_EVENT]     = osMessageQueueNew(TXQ_N, sizeof(frame_t), NULL);
    TxQ[PRIO_DATA]      = osMessageQueueNew(TXQ_N, sizeof(frame_t), NULL);

    static const osThreadAttr_t comm_attr =
        { .name = "Comm",      .stack_size = 2048, .priority = osPriorityAboveNormal };
    static const osThreadAttr_t event_attr =
        { .name = "Event",     .stack_size = 1280, .priority = osPriorityAboveNormal };
    static const osThreadAttr_t obj_attr =
        { .name = "Object",    .stack_size = 768,  .priority = osPriorityNormal };
    static const osThreadAttr_t mon_attr =
        { .name = "Monitor",   .stack_size = 2048, .priority = osPriorityBelowNormal };
    static const osThreadAttr_t ka_attr =
        { .name = "KeepAlive", .stack_size = 768,  .priority = osPriorityBelowNormal };

    osThreadNew(CommTaskFn,      NULL, &comm_attr);
    osThreadNew(EventTaskFn,     NULL, &event_attr);
    osThreadNew(ObjectTaskFn,    NULL, &obj_attr);
    osThreadNew(MonitorTaskFn,   NULL, &mon_attr);
    osThreadNew(KeepAliveTaskFn, NULL, &ka_attr);
}

/* Watchdog module (spec §2.9): refresh the IWDG only if the hot loop checked in
 * since the last refresh, so a hung Comm/Object task actually triggers a reset.
 * Call periodically (e.g. every 250 ms) from the default task. */
void lnc_app_watchdog_step(void) {
    const uint32_t need = ALIVE_COMM | ALIVE_OBJ;
    bool ok;
    taskENTER_CRITICAL();
    ok = ((g_alive & need) == need);
    if (ok) g_alive = 0;
    taskEXIT_CRITICAL();
    if (ok) wd_refresh();
}
