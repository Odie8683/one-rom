// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

// One ROM user plugin to blink the status LED if there's ROM activity

#include "plugin.h"

// Logic to allow this plugin to be built as either a system or user plugin,
// based on the PLUGIN_TYPE passed in on make.
ORA_DEFINE_USER_PLUGIN(
    activity_main,
    0, 1, 0, 0,     // Plugin version
    0, 6, 8         // Minimum One ROM firmware version required
);

void activity_main(
    ora_lookup_fn_t ora_lookup_fn,
    ora_plugin_type_t plugin_type,
    const ora_entry_args_t *entry_args
) {
    // Unused variables
    (void)plugin_type;
    (void)entry_args;

    // Look up the status LED control function.
    ora_set_status_led_fn_t set_status_led = ora_lookup_fn(ORA_ID_SET_STATUS_LED);
    ora_is_pin_output_fn_t is_pin_output = ora_lookup_fn(ORA_ID_IS_PIN_OUTPUT);
    ora_get_data_pin_nums_fn_t get_data_pin_nums = ora_lookup_fn(ORA_ID_GET_DATA_PIN_NUMS);
    if (set_status_led == NULL || is_pin_output == NULL || get_data_pin_nums == NULL) {
        while (1) {}
    }

    // Get the data pins.  One ROM 40 supports 16 data pins, so try to get
    // that many.  If this isn't a One ROM 40, the function will return 8.
    uint8_t data_pins[16];
    uint8_t num_data_pins = get_data_pin_nums(data_pins, 16);
    if (num_data_pins == 0) {
        while (1) {}
    }

    // Start with status LED off
    uint8_t on = 0;
    set_status_led(on);

    while (1) {
        // Brief pause
        for (volatile int i = 0; i < 1000000; i++);

        // Check for One ROM activity - are any data lines set to outputs?
        uint8_t activity = 0;
        for (int ii = 0; ii < num_data_pins; ii++) {
            if (is_pin_output(data_pins[ii])) {
                activity = 1;
                break;
            }
        }

        // If there is activity, toggle the LED
        if (activity) {
            if (on) {
                on = 0;
            } else {
                on = 1;
            }
            set_status_led(on);
        }
    }
}