// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

// Provides ROM image functionality 

#include "usb_plugin.h"
#include "usb_rom.h"

// ---------------------------------------------------------------------------
// Access ROM set information
// ---------------------------------------------------------------------------
const sdrr_rom_set_t *app_get_active_rom_set(const usb_plugin_context_t *ctx) {
    if ((ctx->firmware == NULL) || (ctx->runtime == NULL)) {
        ERR("Firmware or runtime info not available in context");
        return NULL;
    }

    uint8_t rom_set_index = ctx->runtime->rom_set_index;
    const onerom_metadata_header_t *metadata = ctx->firmware->metadata_header;
    if (rom_set_index >= metadata->rom_set_count) {
        ERR("Invalid ROM set index %u (only %u sets available)", rom_set_index, metadata->rom_set_count);
        return NULL;
    }

    const sdrr_rom_set_t *rom_set = &metadata->rom_sets[rom_set_index];
    DEBUG("Active ROM set index: %u", rom_set_index);
    return rom_set;
}

uint32_t app_get_active_rom_size(const usb_plugin_context_t *ctx) {
    if (ctx->active_rom_set == NULL) {
        ERR("Active ROM set not available");
        return 0u;
    }
    const sdrr_rom_info_t *rom = ctx->active_rom_set->roms[0];
    const sdrr_rom_type_t rom_type = rom->rom_type;

    if (ctx->get_chip_size_from_type == NULL) {
        ERR("Chip size from type not available in context");
        return 0u;
    }
    uint32_t rom_size = ctx->get_chip_size_from_type(rom_type);
    //DEBUG("Active ROM size: %u bytes (type 0x%02x)", rom_size, rom_type);

    return rom_size;
}

// Figures out the pin layout for this board, which is used by the address and
// data bit mapping functions below.
pb_status_t app_retrieve_pin_layout(
    const usb_plugin_context_t *ctx,
    rom_pin_layout_t           *layout
) {
    if (ctx->firmware == NULL) {
        ERR("Firmware info not available in context");
        return PB_STATUS_NOT_FOUND;
    }

    const sdrr_pins_t *pins = ctx->firmware->pins;

    // Populate address GPIOs and find minimum pin number
    layout->min_addr_pin = 0xFF;
    for (int ii = 0; ii < 16; ii++) {
        layout->addr_gpio[ii] = pins->addr[ii];
        layout->addr_gpio[ii + 16] = pins->addr2[ii];
        if (pins->addr[ii] < layout->min_addr_pin) layout->min_addr_pin = pins->addr[ii];
        if (pins->addr2[ii] < layout->min_addr_pin) layout->min_addr_pin = pins->addr2[ii];
    }

    // Populate data GPIOs and find minimum pin number
    layout->min_data_pin = 0xFF;
    for (int ii = 0; ii < 8; ii++) {
        layout->data_gpio[ii] = pins->data[ii];
        layout->data_gpio[ii + 8] = pins->data2[ii];
        if (pins->data[ii] < layout->min_data_pin) layout->min_data_pin = pins->data[ii];
        if (pins->data2[ii] < layout->min_data_pin) layout->min_data_pin = pins->data2[ii];
    }

    // x1, x2, cs1 — also contribute to min_addr_pin for offset calculation
    layout->x1 = pins->x1;
    layout->x2 = pins->x2;
    layout->cs1 = pins->cs1;
    if (pins->x1 < layout->min_addr_pin) layout->min_addr_pin = pins->x1;
    if (pins->x2 < layout->min_addr_pin) layout->min_addr_pin = pins->x2;
    if (pins->cs1 < layout->min_addr_pin) layout->min_addr_pin = pins->cs1;

    // TODO: derive num_addr_bits and num_data_bits from rom_type rather than
    // hardcoding
    layout->num_addr_bits = 13;
    layout->num_data_bits = 8;

    return PB_STATUS_OK;
}

// Convert a logical address (i.e. the address as seen by the ROM) to an offset
// into the RAM table
static pb_status_t app_logical_addr_to_offset(
    uint32_t logical_addr,
    const rom_pin_layout_t *layout,
    uint32_t *offset_out,
    const usb_plugin_context_t *ctx
) {
    uint32_t rom_table_size = ctx->runtime->rom_table_size;

    uint32_t offset_addr = 0u;
    for (int ii = 0; ii < layout->num_addr_bits; ii++) {
        if (layout->addr_gpio[ii] < 0xFF) {
            uint8_t bit = (logical_addr >> ii) & 1u;
            offset_addr |= ((uint32_t)bit << (layout->addr_gpio[ii] - layout->min_addr_pin));
        }
    }

    // TODO: apply x1/x2/cs1 values once not hardcoded to zero
    (void)layout->x1;
    (void)layout->x2;
    (void)layout->cs1;

    if (offset_addr >= rom_table_size) {
        ERR("Offset address 0x%08x out of bounds for ROM table", offset_addr);
        return PB_STATUS_INVALID_ADDRESS;
    }

    *offset_out = offset_addr;

    return PB_STATUS_OK;
}

// Get the logical byte value at a logical ROM address by reading the raw byte
// from the appropriate RAM location and remapping it to a logical byte.
pb_status_t app_get_logical_byte_from_logical_addr(
    uint32_t logical_addr,
    uint32_t  *value_out,
    const rom_pin_layout_t *layout,
    const usb_plugin_context_t *ctx
) {
    if (ctx->runtime == NULL) {
        ERR("Runtime info not available in context");
        return PB_STATUS_NOT_FOUND;
    }

    // Get the physical RAM address
    uint32_t offset_addr;
    pb_status_t st = app_logical_addr_to_offset(logical_addr, layout, &offset_addr, ctx);
    if (st != PB_STATUS_OK) {
        return st;
    }

    // Get the raw byte from RAM
    const uint8_t *rom_data = (const uint8_t *)(uintptr_t)(ctx->runtime->rom_table);
    uint8_t raw = rom_data[offset_addr];

    // Reverse the data pin permutation: raw bit (data_gpio[ii] - min_data_pin) → logical bit ii
    uint32_t logical_value = 0u;
    for (int ii = 0; ii < layout->num_data_bits; ii++) {
        if (layout->data_gpio[ii] < 0xFF) {
            uint8_t bit = (raw >> (layout->data_gpio[ii] - layout->min_data_pin)) & 1u;
            logical_value |= ((uint32_t)bit << ii);
        }
    }

    *value_out = logical_value;
    return PB_STATUS_OK;
}

// Writes a logical byte value to a logical ROM address by remapping it to a
// raw byte and writing that to the appropriate RAM location.
pb_status_t app_set_logical_byte_at_logical_addr(
    uint32_t                    logical_addr,
    uint8_t                     logical_value,
    const rom_pin_layout_t     *layout,
    const usb_plugin_context_t *ctx
) {
    if (ctx->runtime == NULL) {
        ERR("Runtime info not available in context");
        return PB_STATUS_NOT_FOUND;
    }

    uint32_t offset_addr;
    pb_status_t st = app_logical_addr_to_offset(logical_addr, layout, &offset_addr, ctx);
    if (st != PB_STATUS_OK) {
        return st;
    }

    // Reverse the data pin permutation: logical bit ii → raw bit (data_gpio[ii] - min_data_pin)
    uint8_t raw = 0u;
    for (int ii = 0; ii < layout->num_data_bits; ii++) {
        if (layout->data_gpio[ii] < 0xFF) {
            uint8_t bit = (logical_value >> ii) & 1u;
            raw |= (uint8_t)(bit << (layout->data_gpio[ii] - layout->min_data_pin));
        }
    }

    // Write the raw byte to RAM
    //DEBUG("Writing logical value 0x%02x to logical address 0x%08x (offset 0x%08x, raw value 0x%02x)", logical_value, logical_addr, offset_addr, raw);
    uint8_t  *rom_data = (uint8_t *)(uintptr_t)(ctx->runtime->rom_table);
    rom_data[offset_addr] = raw;
    return PB_STATUS_OK;
}
