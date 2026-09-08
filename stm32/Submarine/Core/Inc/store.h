#ifndef STORE_H
#define STORE_H
#include <stdbool.h>
#include <stdint.h>

/* SD-card persistence for the Log module (FatFS over SPI1). */
bool store_init(void);                                     /* mount; true on success */
bool store_log_write(const char* today_fname, const char* line);  /* rotate(7d) + append */
bool store_events_append(const char* line);                /* append to EVENTS.TXT */
int  store_ls(char* out, int cap);                         /* list files -> "NAME SIZE\r\n"... */
int  store_cat(const char* fname, char* out, int cap);     /* read file into out; -1 if absent */
bool store_config_save(const uint8_t* buf, uint32_t len);  /* write CONFIG.BIN */
bool store_config_load(uint8_t* buf, uint32_t cap, uint32_t* out_len); /* read CONFIG.BIN */
int  store_query_data(uint32_t from, uint32_t to, char* out, int cap);   /* GET_DATA_RANGE */
int  store_query_events(uint32_t from, uint32_t to, char* out, int cap); /* GET_EVENTS_RANGE */

#endif
