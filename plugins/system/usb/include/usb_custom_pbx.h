// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

#if !defined(USB_CUSTOM_PBX_H)
#define USB_CUSTOM_PBX_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// One ROM picobootx protocol extensions
// ---------------------------------------------------------------------------

#define ONEROM_PICOBOOTX_MAGIC  ('O' | ('N' << 8) | ('E' << 16) | ('R' << 24))

typedef enum {
    ONEROM_CMD_SET_LED = 0x01,
} onerom_cmd_id_t;

typedef enum {
    ONEROM_LED_OFF    = 0x00,
    ONEROM_LED_ON     = 0x01,
    ONEROM_LED_BEACON = 0x02,
    ONEROM_LED_FLAME  = 0x03,
} onerom_led_subcmd_t;

typedef struct __attribute__((packed)) {
    uint8_t  led_id;
    uint8_t  sub_cmd;    // onerom_led_subcmd_t
    uint8_t  reserved[2];
    uint32_t p0;
    uint32_t p1;
    uint32_t p2;
} onerom_set_led_args_t;
_Static_assert(sizeof(onerom_set_led_args_t) == 16, "onerom_set_led_args_t size mismatch");

// ---------------------------------------------------------------------------
// Internal types to handle One ROM picoboot protocol extensions
// ---------------------------------------------------------------------------
typedef enum {
    ONEROM_PENDING_NONE = 0,
    ONEROM_PENDING_SET_LED = 1,
} onerom_pending_cmd_t;
_Static_assert(sizeof(onerom_pending_cmd_t) == 1, "onerom_pending_cmd_t size mismatch");

typedef struct {
    onerom_pending_cmd_t cmd;
    union {
        struct {
            uint8_t led_id;
            onerom_led_subcmd_t sub_cmd;
        } set_led;
    } args;
} onerom_pending_t;

#endif // USB_CUSTOM_PBX_H