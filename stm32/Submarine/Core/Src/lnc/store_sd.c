#include "store.h"
#include "fatfs.h"       /* FatFS + USERFatFS/USERPath */
#include "logplan.h"     /* LNC_MAX_LOG_DAYS + logplan_evict (pure, host-tested) */
#include <string.h>
#include <stdlib.h>

/* Day-log files are 8.3 names "YYYYMMDD.TXT" (LFN is disabled in ffconf).
 * The events file is "EVENTS.TXT". Measurement/event lines are formatted by the
 * pure logline module; this file only does the FatFS I/O and the 7-day rotation. */

static bool s_mounted = false;

bool store_init(void) {
    s_mounted = (f_mount(&USERFatFS, USERPath, 1) == FR_OK);
    return s_mounted;
}

static bool is_day_log(const char* nm) {
    if (strlen(nm) != 12) return false;               /* 8 digits + ".TXT" */
    for (int i = 0; i < 8; ++i)
        if (nm[i] < '0' || nm[i] > '9') return false;
    return strcmp(nm + 8, ".TXT") == 0;               /* FatFS 8.3 names are upper-case */
}

/* List day-log filenames into names[], sorted ascending. Returns the count. */
static int list_day_logs(char names[][16], int max) {
    DIR dir; FILINFO fno;
    int n = 0;
    if (f_opendir(&dir, USERPath) != FR_OK) return 0;
    while (n < max && f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        if (is_day_log(fno.fname)) {
            strncpy(names[n], fno.fname, 15);
            names[n][15] = '\0';
            ++n;
        }
    }
    f_closedir(&dir);
    for (int i = 0; i + 1 < n; ++i)                   /* simple ascending sort */
        for (int j = i + 1; j < n; ++j)
            if (strcmp(names[i], names[j]) > 0) {
                char t[16]; strcpy(t, names[i]); strcpy(names[i], names[j]); strcpy(names[j], t);
            }
    return n;
}

/* Append a NUL-terminated line to a file (create if absent). */
static bool append_line(const char* fname, const char* line) {
    if (!s_mounted) return false;
    FIL f;
    if (f_open(&f, fname, FA_OPEN_ALWAYS | FA_WRITE) != FR_OK) return false;
    f_lseek(&f, f_size(&f));                           /* seek to end = append */
    UINT len = (UINT)strlen(line), bw = 0;
    FRESULT wr = f_write(&f, line, len, &bw);
    f_close(&f);
    return wr == FR_OK && bw == len;
}

bool store_log_write(const char* today_fname, const char* line) {
    if (!s_mounted) return false;
    char names[LNC_MAX_LOG_DAYS + 1][16];
    int count = list_day_logs(names, LNC_MAX_LOG_DAYS + 1);
    char evict[16];
    if (logplan_evict(names, count, today_fname, evict))
        f_unlink(evict);                               /* drop the oldest on day 8 */
    return append_line(today_fname, line);
}

bool store_events_append(const char* line) {
    return append_line("EVENTS.TXT", line);
}

#include <stdio.h>

int store_ls(char* out, int cap) {
    out[0] = '\0';
    if (!s_mounted) return 0;
    DIR dir; FILINFO fno;
    int n = 0, len = 0;
    if (f_opendir(&dir, USERPath) != FR_OK) return 0;
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        int w = snprintf(out + len, (size_t)(cap - len), "%-13s %lu\r\n",
                         fno.fname, (unsigned long)fno.fsize);
        if (w < 0 || w >= cap - len) break;
        len += w; ++n;
    }
    f_closedir(&dir);
    return n;
}

int store_cat(const char* fname, char* out, int cap) {
    if (!s_mounted) return -1;
    FIL f;
    if (f_open(&f, fname, FA_READ) != FR_OK) return -1;
    UINT br = 0;
    f_read(&f, out, (UINT)(cap - 1), &br);
    out[br] = '\0';
    f_close(&f);
    return (int)br;
}

bool store_config_save(const uint8_t* buf, uint32_t len) {
    if (!s_mounted) return false;
    FIL f;
    if (f_open(&f, "CONFIG.BIN", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) return false;
    UINT bw = 0;
    FRESULT wr = f_write(&f, buf, (UINT)len, &bw);
    f_close(&f);
    return wr == FR_OK && bw == len;
}

bool store_config_load(uint8_t* buf, uint32_t cap, uint32_t* out_len) {
    if (!s_mounted) return false;
    FIL f;
    if (f_open(&f, "CONFIG.BIN", FA_READ) != FR_OK) return false;
    UINT br = 0;
    FRESULT rd = f_read(&f, buf, (UINT)cap, &br);
    f_close(&f);
    if (rd != FR_OK) return false;
    *out_len = (uint32_t)br;
    return true;
}

/* Range retrieval: append every CSV line whose leading timestamp is in [from,to]
 * from one file into out. Reuses the stored line verbatim (its first field is the
 * epoch). Returns matches found; advances *len. */
static int query_file(const char* fname, uint32_t from, uint32_t to,
                      char* out, int* len, int cap) {
    FIL f;
    if (f_open(&f, fname, FA_READ) != FR_OK) return 0;
    char line[80];
    int n = 0;
    while (f_gets(line, sizeof(line), &f)) {
        unsigned long ts = strtoul(line, NULL, 10);   /* first CSV field */
        if (ts >= from && ts <= to) {
            size_t L = strlen(line);                   /* strip stored newline */
            while (L && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
            int room = cap - *len;
            int w = snprintf(out + *len, (size_t)room, "%s\r\n", line);  /* clean CRLF */
            if (w < 0 || w >= room) break;             /* out of space */
            *len += w; ++n;
        }
    }
    f_close(&f);
    return n;
}

int store_query_data(uint32_t from, uint32_t to, char* out, int cap) {
    out[0] = '\0';
    if (!s_mounted) return 0;
    char names[LNC_MAX_LOG_DAYS + 1][16];
    int fc = list_day_logs(names, LNC_MAX_LOG_DAYS + 1);   /* sorted ascending */
    int len = 0, total = 0;
    for (int i = 0; i < fc; ++i)
        total += query_file(names[i], from, to, out, &len, cap);
    return total;
}

int store_query_events(uint32_t from, uint32_t to, char* out, int cap) {
    out[0] = '\0';
    if (!s_mounted) return 0;
    int len = 0;
    return query_file("EVENTS.TXT", from, to, out, &len, cap);
}
