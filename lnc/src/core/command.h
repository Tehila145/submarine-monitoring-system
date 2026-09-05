#ifndef COMMAND_H
#define COMMAND_H
#include "lnc_types.h"
#include "config.h"

typedef enum { QUERY_NONE=0, QUERY_DATA, QUERY_EVENTS } query_kind_t;

typedef struct {
    bool handled;          /* tag recognized                       */
    bool nack;             /* recognized but value malformed       */
    bool config_changed;   /* a SET_* was applied to *cfg          */
    bool set_rtc;          /* caller must rtc_set(rtc_epoch)        */
    uint32_t rtc_epoch;
    query_kind_t query;    /* caller runs a range query            */
    uint32_t range_from, range_to;
    uint8_t reply[16];     /* e.g. RSP_TIME frame to send back      */
    uint8_t reply_len;
} command_result_t;

command_result_t command_handle(const uint8_t* frame, uint32_t len,
                                lnc_config_t* cfg, uint32_t now_epoch);
#endif
