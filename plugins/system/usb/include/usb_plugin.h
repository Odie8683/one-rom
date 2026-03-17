// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

#ifndef USB_PLUGIN_H
#define USB_PLUGIN_H

#include <stdint.h>
#include <stdbool.h>
#include "plugin.h"
#include "tusb.h"
#include "include.h"
#include "usb_custom_pbx.h"
#include "usb_led.h"

// Context structure for our plugin
typedef struct {
    ora_lookup_fn_t ora_lookup_fn;
    ora_log_fn_t log;
    ora_debug_log_fn_t debug;
    ora_err_log_fn_t err_log;
    ora_set_status_led_fn_t set_status_led;
    uint32_t timer_ms;
    const sdrr_runtime_info_t *runtime;
    const sdrr_info_t *firmware;
    ora_get_chip_size_from_type_fn_t get_chip_size_from_type;
    const sdrr_rom_set_t *active_rom_set;
    onerom_pending_t pending;
    led_status_t led_status;
} usb_plugin_context_t;

// Forward declaration of the context, which we define in usb_main.c
extern usb_plugin_context_t context;

// Forward declaration of plugin's Picoboot functions, from usb_picoboot.c
void usb_picoboot_init(uint8_t ep_out, uint8_t ep_in);
bool usb_picoboot_control_xfer_cb(
    uint8_t rhport,
    uint8_t stage,
    tusb_control_request_t const *request
);
void usb_picoboot_tx_cb(uint8_t idx, uint32_t sent_bytes);
void usb_picoboot_rx_cb(uint8_t idx, uint8_t const *buf, uint32_t count);
void usb_picoboot_task(void);

// Logging macros
#if defined(DEBUG)
#undef DEBUG
#endif
#define DEBUG(...) do { \
    if (context.debug) { \
        context.debug(__VA_ARGS__); \
    } \
} while (0)

#if defined(LOG)
#undef LOG
#endif
#define LOG(...) do { \
    if (context.log) { \
        context.log(__VA_ARGS__); \
    } \
} while (0)

#if defined(ERR)
#undef ERR
#endif
#define ERR(...) do { \
    if (context.err_log) { \
        context.err_log(__VA_ARGS__); \
    } \
} while (0)

#endif // USB_PLUGIN_H