#ifndef PROTOCOL_H
#define PROTOCOL_H
/* Project-defined TLV tag numbers. These MUST be identical in the Central
 * Computer's protocol definition. Values are a design choice, not fixed by
 * the specification; keep them stable once the Central side is written. */

/* Commands: Central -> LNC */
#define CMD_SET_TEMP_NORMAL    0x20  /* value: int16 lo, int16 hi */
#define CMD_SET_TEMP_WARNING   0x21  /* value: int16 lo, int16 hi */
#define CMD_SET_HUM_NORMAL     0x22  /* value: uint16 lower bound  */
#define CMD_SET_HUM_WARNING    0x23  /* value: uint16 lower bound  */
#define CMD_SET_LIGHT_NORMAL   0x24  /* value: uint16 lower bound  */
#define CMD_SET_LIGHT_WARNING  0x25  /* value: uint16 lower bound  */
#define CMD_SET_BATT_NORMAL    0x26  /* value: uint16 lower bound  */
#define CMD_SET_BATT_WARNING   0x27  /* value: uint16 lower bound  */
#define CMD_SET_RTC            0x28  /* value: uint32 epoch        */
#define CMD_GET_TIME           0x29  /* value: none                */
#define CMD_GET_DATA_RANGE     0x2A  /* value: uint32 from, uint32 to */
#define CMD_GET_EVENTS_RANGE   0x2B  /* value: uint32 from, uint32 to */

/* Reports/replies: LNC -> Central */
#define RPT_KEEPALIVE          0x01  /* nested: timestamp, measurement, mode */
#define RPT_EVENT              0x02  /* nested: an event record              */
#define RPT_DATA               0x03  /* nested: a measurement record         */
#define RSP_TIME               0x04  /* value: uint32 epoch                  */

/* Child tags used inside nested values */
#define TAG_TIMESTAMP          0x10
#define TAG_MEASUREMENT        0x11
#define TAG_MODE               0x12
#define TAG_EVENT_SRC          0x13
#define TAG_EVENT_FLAG         0x14  /* 1 byte: OBJECT->detected (0/1)        */

#endif
