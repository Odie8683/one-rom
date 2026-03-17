/* Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
 *
 * MIT License
 */

/*
 * One ROM plugin linker script
 *
 * Links a plugin binary as a flat, position-dependent image at a base address
 * supplied by the build system via --defsym PLUGIN_BASE=<address>.
 *
 * Use 0x10010000 for system plugins and 0x10020000 for user plugins.
 *
 * The plugin header must be defined in the plugin source using
 * ORA_DEFINE_SYSTEM_PLUGIN() or ORA_DEFINE_USER_PLUGIN() from ora/plugin.h.
 *
 * Mutable global state (.data, .bss) is explicitly forbidden and will cause
 * a linker error.
 */

SECTIONS
{
    . = PLUGIN_BASE;

    .plugin_header : {
        KEEP(*(.plugin_header))
    }

    .text : {
        *(.text*)
        *(.rodata*)
    }

    .data : { *(.data*) }
    .bss  : { *(.bss*)  }

    ASSERT(SIZEOF(.plugin_header) > 0, "ERROR: .plugin_header section is empty")
    ASSERT(SIZEOF(.data) == 0, "ERROR: .data section is non-empty - plugins must not use mutable global state")
    ASSERT(SIZEOF(.bss)  == 0, "ERROR: .bss section is non-empty - plugins must not use mutable global state")

    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}