// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

// A minimal One ROM plugin example.  This plugin logs "Hello One ROM" in an
// infinite loop.

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

    // Look up the logging function from the API.
    ora_log_fn_t log = ora_lookup_fn(ORA_ID_LOG);

    while (1) {
        // Log a message every so often
        log("Hello One ROM");
        for (volatile int i = 0; i < 10000000; i++);
    }
}