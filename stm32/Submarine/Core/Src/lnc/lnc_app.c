#include "lnc_app.h"
#include "board.h"          /* HAL_GetTick, handles */
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
#include "sched.h"
#include "hal_sensors.h"
#include "hal_indicators.h"
#include "hal_time.h"
#include "hal_watchdog.h"
#include "transport.h"
#include "store.h"          /* SD-card logging (Log module) */
#include "logline.h"        /* CSV line format for log/events */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* --- MODE (runtime) ----------------------------------------------------------
 * Console mode (default): human-readable text + ASCII commands over USART2, so
 * you can drive it from `screen`. Protocol mode: the binary TLV machine protocol
 * (keep-alive/event/data frames + binary command RX) for a Central Computer.
 * Toggle to protocol with the `proto` command; RESET returns to console. */
static bool s_protocol = false;

static const char* mode_name(lnc_mode_t m) {
    return m == MODE_ERROR ? "ERROR" : (m == MODE_WARNING ? "WARNING" : "NORMAL");
}
/* Register-level TX (no HAL lock) so console output never deadlocks against the
 * RX interrupt re-arming HAL_UART_Receive_IT. */
static void console_print(const char* s) {
    while (*s) {
        while (!(USART2->ISR & USART_ISR_TXE)) { }
        USART2->TDR = (uint8_t)(*s++);
    }
}

#define FRAME_MAX 64
#define EVQ_N     8
#define TXQ_N     6

static lnc_config_t g_cfg;
static measurement_t g_latest;

/* Persist the current config to the SD card (CONFIG.BIN) using the tested
 * serializer, so limit changes survive a reboot (spec §2.6). */
static void config_persist(void) {
    uint8_t buf[sizeof(lnc_config_t)];
    uint32_t n = config_serialize(&g_cfg, buf, sizeof(buf));
    if (n) store_config_save(buf, n);
}

/* ---------------- event queue (producers -> Event handling) ---------------- */
static lnc_event_t evq[EVQ_N];
static uint8_t ev_head, ev_tail;
static void ev_post(const lnc_event_t* e) {
    uint8_t n = (uint8_t)((ev_head + 1) % EVQ_N);
    if (n != ev_tail) { evq[ev_head] = *e; ev_head = n; }   /* drop if full */
}
static bool ev_pop(lnc_event_t* e) {
    if (ev_head == ev_tail) return false;
    *e = evq[ev_tail]; ev_tail = (uint8_t)((ev_tail + 1) % EVQ_N); return true;
}

/* ---------------- TX priority queues (keepalive > event > data) ------------ */
typedef struct { uint8_t buf[FRAME_MAX]; uint8_t len; } frame_t;
static frame_t txq[3][TXQ_N];
static uint8_t tx_head[3], tx_tail[3];
static void tx_enq(int prio, const uint8_t* f, uint8_t len) {
    uint8_t n = (uint8_t)((tx_head[prio] + 1) % TXQ_N);
    if (n != tx_tail[prio] && len <= FRAME_MAX) {
        memcpy(txq[prio][tx_head[prio]].buf, f, len);
        txq[prio][tx_head[prio]].len = len;
        tx_head[prio] = n;
    }
}
static bool tx_has(int p) { return tx_head[p] != tx_tail[p]; }
static void tx_drain(void) {
    for (;;) {
        commq_state_t s = { tx_has(PRIO_KEEPALIVE), tx_has(PRIO_EVENT), tx_has(PRIO_DATA) };
        int p = commq_next(&s);
        if (p < 0) break;
        frame_t* fr = &txq[p][tx_tail[p]];
        transport_send(fr->buf, fr->len);
        tx_tail[p] = (uint8_t)((tx_tail[p] + 1) % TXQ_N);
    }
}

/* ---------------- interrupt-driven RX + ASCII command console -------------- */
/* One byte at a time via HAL_UART_Receive_IT into a ring buffer; the callback
 * re-arms and an error callback recovers from overrun. The existing
 * USART2_IRQHandler (in stm32l4xx_it.c) already drives HAL_UART_IRQHandler. */
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
        store_ls(b, sizeof(b));
        console_print(b[0] ? b : "(no files / SD off)\r\n");
    } else if (!strcmp(cmd, "cat")) {
        char *a = strtok(NULL, " \t");
        if (!a) { console_print("usage: cat <FILE>  (e.g. cat EVENTS.TXT)\r\n"); }
        else {
            static char b[512];
            int n = store_cat(a, b, sizeof(b));
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
        int n = ev ? store_query_events(from, to, buf, sizeof(buf))
                   : store_query_data(from, to, buf, sizeof(buf));
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
        if (lo && hi) { g_cfg.temp_norm_lo = (int16_t)atoi(lo);
                        g_cfg.temp_norm_hi = (int16_t)atoi(hi);
                        config_persist();
                        console_print("OK: temp normal set (saved)\r\n"); }
        else console_print("usage: tn <lo> <hi>\r\n");
    } else if (!strcmp(cmd, "tw")) {
        char *lo = strtok(NULL, " \t"), *hi = strtok(NULL, " \t");
        if (lo && hi) { g_cfg.temp_warn_lo = (int16_t)atoi(lo);
                        g_cfg.temp_warn_hi = (int16_t)atoi(hi);
                        config_persist();
                        console_print("OK: temp warning set (saved)\r\n"); }
        else console_print("usage: tw <lo> <hi>\r\n");
    } else if (!strcmp(cmd, "led")) {
        /* Diagnostic: drive the RGB pins directly (HIGH = on for common-cathode). */
        char *a = strtok(NULL, " \t");
        uint8_t r = 0, g = 0, b = 0;
        if (!a) { console_print("usage: led r|g|b|y|all|off\r\n"); }
        else {
            if      (!strcmp(a, "r"))   r = 1;
            else if (!strcmp(a, "g"))   g = 1;
            else if (!strcmp(a, "b"))   b = 1;
            else if (!strcmp(a, "y"))   { r = 1; g = 1; }
            else if (!strcmp(a, "all")) { r = 1; g = 1; b = 1; }
            /* "off" => all 0 */
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
        s_protocol = true;                 /* poll() switches paths next iteration */
    } else {
        console_print("? unknown - type 'help'\r\n");
    }
}

/* Drain the RX ring, assemble lines, execute on CR/LF. Echoes typed characters
 * back so the user can see their input in `screen`. Called every loop. */
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

static void monitor_task(void) {
    measurement_t m;
    m.ts = rtc_now();
    m.temperature = sensors_read_temperature();
    m.humidity    = sensors_read_humidity();
    m.light       = sensors_read_light();
    m.battery     = sensors_read_battery();
    m.mode        = limits_evaluate(&m, &g_cfg);
    g_latest = m;
    log_measurement(&m);            /* Log module: append to today's day file */
    lnc_event_t e;
    if (monitor_logic_step(&m, &e)) ev_post(&e);
}

static void objectdet_task(void) {
    lnc_event_t e;
    if (objectdet_logic_step(sensors_object_present(), rtc_now(), &e)) ev_post(&e);
}

static void keepalive_task(void) {
    uint8_t f[FRAME_MAX];
    int n = keepalive_build(f, sizeof(f), rtc_now(), &g_latest);
    if (n > 0) tx_enq(PRIO_KEEPALIVE, f, (uint8_t)n);
}

static void handle_events(void) {
    lnc_event_t e;
    while (ev_pop(&e)) {
        event_action_t a = event_decide(&e);
        if (a.set_led)   led_set(a.led);
        if (a.set_alarm) alarm_set(a.alarm_on);
        log_event(&e);   /* Log module: append to the events file (both modes) */
        if (s_protocol) {
            uint8_t f[FRAME_MAX];
            int n = record_event(f, sizeof(f), &e);
            if (n > 0) tx_enq(PRIO_EVENT, f, (uint8_t)n);
        } else {
            char line[64];
            if (e.src == SRC_MONITOR)
                snprintf(line, sizeof(line), ">> EVENT: mode %s -> %s\r\n",
                         mode_name(e.data.transition.from), mode_name(e.data.transition.to));
            else if (e.src == SRC_OBJECT)
                snprintf(line, sizeof(line), ">> EVENT: object %s\r\n",
                         e.data.object.detected ? "DETECTED" : "cleared");
            else if (e.src == SRC_INIT)
                snprintf(line, sizeof(line), ">> EVENT: startup%s\r\n",
                         e.data.startup.after_wd_reset ? " (after watchdog reset)" : "");
            else
                snprintf(line, sizeof(line), ">> EVENT: config changed\r\n");
            console_print(line);
        }
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

/* Stream stored records in [from,to] back as TLV frames (GET_*_RANGE): read the
 * CSV lines the Log module wrote, parse them to structs, encode each as an
 * RPT_DATA / RPT_EVENT frame. Capped by the query buffer. */
static void run_query(const command_result_t* r) {
    static char blob[768];
    if (r->query == QUERY_EVENTS)
        store_query_events(r->range_from, r->range_to, blob, sizeof(blob));
    else
        store_query_data(r->range_from, r->range_to, blob, sizeof(blob));
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

/* Binary RX (protocol mode): assemble TLV frames from the interrupt ring and
 * dispatch each through command_handle + exec_command. */
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
            command_result_t r = command_handle(fr, have, &g_cfg, rtc_now());
            exec_command(&r);
            have = 0; need = 0;
        }
    }
}

static void button_task(void) {
    if (button_pressed_clear()) alarm_set(false);   /* button stops the alarm */
}

/* ---------------- Log module (SD card) ------------------------------------ */
static bool s_logging = false;

static void log_measurement(const measurement_t* m) {
    if (!s_logging) return;
    char day[9];  rtc_datestr8(day);            /* "YYYYMMDD" */
    char fname[16]; snprintf(fname, sizeof(fname), "%s.TXT", day);
    char line[64];
    if (logline_format_measurement(line, sizeof(line), m) > 0)
        store_log_write(fname, line);           /* rotate(7d) + append */
}

static void log_event(const lnc_event_t* e) {
    if (!s_logging) return;
    char line[64];
    if (logline_format_event(line, sizeof(line), e) > 0)
        store_events_append(line);
}

/* ---------------- lifecycle ------------------------------------------------ */
static uint32_t t_mon, t_ka, t_wd, t_status, t_obj;

void lnc_app_init(void) {
    reset_cause_t cause = reset_cause();
    /* Config: load-or-default. Stage 1 has no persistence, so always defaults.
       TODO(storage): load from Flash, fall back to defaults on first boot. */
    config_load_defaults(&g_cfg);
    monitor_logic_reset();
    objectdet_logic_reset();
    transport_init();
    rx_start();                     /* arm interrupt-driven command RX */
    led_init();                     /* configure RGB pins PC6/7/8 */
    led_set(LED_GREEN);
    monitor_task();                 /* prime g_latest for the first keep-alive */
    /* §2.7 time sync: Stage 1 runs the RTC standalone; real sync is wired when
       the Central Computer exists. Startup event carries the WD-reset flag. */
    lnc_event_t se;
    init_logic_startup_event(cause, rtc_now(), &se);
    ev_post(&se);
    uint32_t now = HAL_GetTick();
    t_mon = t_ka = t_wd = t_status = t_obj = now;
    s_logging = store_init();       /* mount SD; enable the Log module */
    bool cfg_loaded = false;
    if (s_logging) {                /* §2.6: load persisted config, else save defaults */
        uint8_t buf[sizeof(lnc_config_t)]; uint32_t n = 0;
        if (store_config_load(buf, sizeof(buf), &n) && config_deserialize(&g_cfg, buf, n))
            cfg_loaded = true;
        else
            config_persist();       /* first boot: write defaults to CONFIG.BIN */
    }
    console_print("\r\n=== LNC console ===  (type 'help')\r\n");
    console_print(s_logging ? "SD: mounted - logging enabled\r\n"
                            : "SD: mount failed - logging disabled\r\n");
    console_print(cfg_loaded ? "Config: loaded from CONFIG.BIN\r\n"
                             : "Config: defaults\r\n");
}

void lnc_app_poll(void) {
    uint32_t now = HAL_GetTick();
    sensors_ir_poll();                     /* fast-sample the IR receiver (latches) */
    if (s_protocol) process_binary_rx();   /* binary TLV commands from Central */
    else            process_console_rx();  /* ASCII console commands */
    button_task();
    if (sched_due(&t_obj, 200, now)) objectdet_task();   /* IR sample @5 Hz (debounce) */
    handle_events();
    /* Spec samples every 5 s; console mode samples every 2 s so the mode reacts
       quickly to a breath on the sensor during a live demo. */
    if (sched_due(&t_mon, s_protocol ? 5000u : 2000u, now)) monitor_task();
    if (s_protocol) {
        if (sched_due(&t_ka, 6000, now)) keepalive_task();   /* §2.8: every 6 s */
    } else {
        if (sched_due(&t_status, 2000, now)) {
            char line[80];
            snprintf(line, sizeof(line),
                     "T=%dC  H=%u%%  L=%u  B=%u  mode=%s\r\n",
                     (int)g_latest.temperature, (unsigned)g_latest.humidity,
                     (unsigned)g_latest.light, (unsigned)g_latest.battery,
                     mode_name(g_latest.mode));
            console_print(line);
        }
    }
    if (sched_due(&t_wd, 500, now)) wd_refresh();
    tx_drain();
}
