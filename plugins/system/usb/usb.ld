/* Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
 *
 * MIT License
 */

SECTIONS
{
    SYSTEM_PLUGIN_DATA_BASE = 0x20081000;
    SYSTEM_PLUGIN_DATA_LIMIT = 0x20081800;

    . = PLUGIN_BASE;

    .plugin_header : {
        KEEP(*(.plugin_header))
        . = ALIGN(4);
    }

    ASSERT(SIZEOF(.plugin_header) > 0, "ERROR: .plugin_header section is empty")

    .text : {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        __text_end = .;
    }

    .ramfunc SYSTEM_PLUGIN_DATA_BASE : AT(__text_end) {
        __ramfunc_start = .;
        *(.ramfunc*)
        . = ALIGN(4);
        __ramfunc_end = .;
    }
    __ramfunc_load = LOADADDR(.ramfunc);

    .data (__ramfunc_start + SIZEOF(.ramfunc)) : AT(__text_end + SIZEOF(.ramfunc)) {
        __data_start = .;
        *(.data*)
        . = ALIGN(4);
        __data_end = .;
    }
    __data_load = LOADADDR(.data);

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*)
        . = ALIGN(4);
        __bss_end = .;
    }
    ASSERT(. <= SYSTEM_PLUGIN_DATA_LIMIT, "ERROR: .data and .bss exceed 0x20081800")

    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}