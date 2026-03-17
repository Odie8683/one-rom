// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

// A minimal One ROM plugin example.  This plugin blinks the status LED.

#include "plugin.h"

ORA_DEFINE_USER_PLUGIN(plugin_main, 0, 1, 0, 0);

void plugin_main(
    ora_lookup_fn_t ora_lookup_fn,
    ora_plugin_type_t plugin_type,
    ora_core_t core
) {
    // Unused variables
    (void)plugin_type;
    (void)core;

    // Look up the status LED control function.
    ora_set_status_led_fn_t set_status_led = ora_lookup_fn(ORA_ID_SET_STATUS_LED);

    uint8_t on = 0;
    while (1) {
        // Blink the LED
        set_status_led(on);
        on = !on;
        for (volatile int i = 0; i < 10000000; i++);
    }
}