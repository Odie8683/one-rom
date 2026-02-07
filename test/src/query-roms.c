// Query generated roms.c

// Copyright (C) 2025 Piers Finlayson <piers@piers.rocks>
//
// MIT License

#include "roms-test.h"
#include "json-config.h"

// The address mangler uses CSx, not /CE or /OE.  Where /CE and /OE are used
// instead, this address mangler refers to them as CS1 and CS2.

typedef struct {
    uint8_t addr_pins[MAX_ADDR_LINES];
    uint8_t cs1_pin;
    uint8_t cs2_pin;
    uint8_t cs3_pin;
    uint8_t x1_pin;
    uint8_t x2_pin;
    int initialized;
} address_mangler_t;
static address_mangler_t address_mangler;

static void init_address_mangler(
    const json_config_t* config,
    const sdrr_rom_type_t rom_type,
    address_mangler_t *mangler
) {
    // Initialize
    mangler->initialized = 0;
    mangler->cs1_pin = 255;
    mangler->cs2_pin = 255;
    mangler->cs3_pin = 255;
    mangler->x1_pin = 255;
    mangler->x2_pin = 255;
    memset(mangler->addr_pins, 255, sizeof(mangler->addr_pins));

    // Set CS pins b ased on ROM type
    switch (rom_type) {
        case CHIP_TYPE_2316:
            mangler->cs1_pin = config->mcu.pins.cs1.pin_2316;
            mangler->cs2_pin = config->mcu.pins.cs2.pin_2316;
            mangler->cs3_pin = config->mcu.pins.cs3.pin_2316;
            break;

        case CHIP_TYPE_2332:
            mangler->cs1_pin = config->mcu.pins.cs1.pin_2332;
            mangler->cs2_pin = config->mcu.pins.cs2.pin_2332;
            mangler->cs3_pin = config->mcu.pins.cs3.pin_2332;
            break;

        case CHIP_TYPE_2364:
            mangler->cs1_pin = config->mcu.pins.cs1.pin_2364;
            mangler->cs2_pin = config->mcu.pins.cs2.pin_2364;
            mangler->cs3_pin = config->mcu.pins.cs3.pin_2364;
            break;

        case CHIP_TYPE_23128:
            mangler->cs1_pin = config->mcu.pins.cs1.pin_23128;
            mangler->cs2_pin = config->mcu.pins.cs2.pin_23128;
            mangler->cs3_pin = config->mcu.pins.cs3.pin_23128;
            break;

        case CHIP_TYPE_23256:
            mangler->cs1_pin = config->mcu.pins.cs1.pin_23256;
            mangler->cs2_pin = config->mcu.pins.cs2.pin_23256;
            mangler->cs3_pin = config->mcu.pins.cs3.pin_23256;
            break;

        case CHIP_TYPE_23512:
            mangler->cs1_pin = config->mcu.pins.cs1.pin_23512;
            mangler->cs2_pin = config->mcu.pins.cs2.pin_23512;
            break;

        case CHIP_TYPE_231024:
            mangler->cs1_pin = config->mcu.pins.cs1.pin_231024;
            break;

        case CHIP_TYPE_2716:
            mangler->cs1_pin = config->mcu.pins.ce.pin_2716;
            mangler->cs2_pin = config->mcu.pins.oe.pin_2716;
            break;

        case CHIP_TYPE_2732:
            mangler->cs1_pin = config->mcu.pins.ce.pin_2732;
            mangler->cs2_pin = config->mcu.pins.oe.pin_2732;
            break;

        case CHIP_TYPE_2764:
            mangler->cs1_pin = config->mcu.pins.ce.pin_2764;
            mangler->cs2_pin = config->mcu.pins.oe.pin_2764;
            break;

        case CHIP_TYPE_27128:
            mangler->cs1_pin = config->mcu.pins.ce.pin_27128;
            mangler->cs2_pin = config->mcu.pins.oe.pin_27128;
            break;

        case CHIP_TYPE_27256:
            mangler->cs1_pin = config->mcu.pins.ce.pin_27256;
            mangler->cs2_pin = config->mcu.pins.oe.pin_27256;
            break;

        case CHIP_TYPE_27512:
            mangler->cs1_pin = config->mcu.pins.ce.pin_27512;
            mangler->cs2_pin = config->mcu.pins.oe.pin_27512;
            break;

        default:
            printf("Error: Unsupported ROM type %d\n", rom_type);
            exit(1);
            break;
    }


    memcpy(address_mangler.addr_pins, config->mcu.pins.addr, sizeof(address_mangler.addr_pins));

    // There is a special case for 24 pin ROMs - the 2732.  It has A11 as pin
    // 21, whereas the other ROM types have it at pin 18.  For the 2732
    // therefore we swap the A11 and A12 pins.
    if (rom_type == CHIP_TYPE_2732) {
        // Find logical A11 and A12 pins
        uint8_t pin_a11 = address_mangler.addr_pins[11];
        uint8_t pin_a12 = address_mangler.addr_pins[12];
        // Swap them
        address_mangler.addr_pins[11] = pin_a12;
        address_mangler.addr_pins[12] = pin_a11;
#if defined(DEBUG_TEST)
        printf("    Note: Swapped A11 and A12 pins %d/%d for 2732 ROM type\n", pin_a11, pin_a12);
#endif // DEBUG_TEST
    }

    address_mangler.x1_pin = config->mcu.pins.x1;
    address_mangler.x2_pin = config->mcu.pins.x2;
    address_mangler.initialized = 1;
}

void create_address_mangler(const json_config_t* config, const sdrr_rom_type_t rom_type) {
    init_address_mangler(config, rom_type, &address_mangler);

    // Now renamp address/CS/X pins if they're not in the 0..15 range
    if (config->rom.pin_count == 24) {
        if ((config->mcu.ports.data_port == config->mcu.ports.addr_port) && (config->mcu.pins.data[0] < 8)) {
            // If data and address ports are the same, and data lines are 0-7, then
            // address lines must be higher 8-23.  Subtract 8 off them so thare are 0-15.
            for (int ii = 0; ii < MAX_ADDR_LINES; ii++) {
                if (address_mangler.addr_pins[ii] != 255) {
                    address_mangler.addr_pins[ii] -= 8;
                }
            }

            // And the CS and X lines too
            if (address_mangler.cs1_pin != 255) {
                address_mangler.cs1_pin -= 8;
            }
            if (address_mangler.cs2_pin != 255) {
                address_mangler.cs2_pin -= 8;
            }
            if (address_mangler.cs3_pin != 255) {
                address_mangler.cs3_pin -= 8;
            }
            if (address_mangler.x1_pin != 255) {
                address_mangler.x1_pin -= 8;
            }
            if (address_mangler.x2_pin != 255) {
                address_mangler.x2_pin -= 8;
            }
        }
    } else {
#if defined(RP235X)
        // RP235X: CS pins ARE part of address space for 28 pin ROMs for
        // 231024, but not for other 28 pin chip types.

        // Find the minimum across address AND CS pins
        uint8_t min_pin = 255;
        uint8_t max_pins;

        // <231024 have 16 pins, 231024 has 18 pins
        if (rom_type == CHIP_TYPE_231024) {
            max_pins = 18;
        } else {
            max_pins = 16;
        }

        for (int ii = 0; ii < MAX_ADDR_LINES; ii++) {
            if (ii < max_pins) {
                if (address_mangler.addr_pins[ii] < min_pin) {
                    min_pin = address_mangler.addr_pins[ii];
                }
            } else {
                // Ensure any pins above max are set to 255 so mangler
                // doesn't try to use them
                address_mangler.addr_pins[ii] = 255;
            }
        }

        if (rom_type == CHIP_TYPE_231024) {
            if (address_mangler.cs1_pin != 255 && address_mangler.cs1_pin < min_pin) {
                min_pin = address_mangler.cs1_pin;
            }
            if (address_mangler.cs2_pin != 255 && address_mangler.cs2_pin < min_pin) {
                min_pin = address_mangler.cs2_pin;
            }
            if (address_mangler.cs3_pin != 255 && address_mangler.cs3_pin < min_pin) {
                min_pin = address_mangler.cs3_pin;
            }

            // Subtract minimum from all CS pins
            if (address_mangler.cs1_pin != 255) {
                address_mangler.cs1_pin -= min_pin;
            }
            if (address_mangler.cs2_pin != 255) {
                address_mangler.cs2_pin -= min_pin;
            }
            if (address_mangler.cs3_pin != 255) {
                address_mangler.cs3_pin -= min_pin;
            }
        }

        // Subtract minimum from all address pins
        for (int ii = 0; ii < MAX_ADDR_LINES; ii++) {
            if (address_mangler.addr_pins[ii] != 255) {
                address_mangler.addr_pins[ii] -= min_pin;
            }
        }
        
#endif // RP235X
#if defined(STM32F4)
        // STM32F4: CS pins are NOT part of address space for 28 pin ROMs
        // Only left shift address pins
        uint8_t min_addr_pin = 255;
        for (int ii = 0; ii < MAX_ADDR_LINES; ii++) {
            if (address_mangler.addr_pins[ii] < min_addr_pin) {
                min_addr_pin = address_mangler.addr_pins[ii];
            }
        }

        for (int ii = 0; ii < MAX_ADDR_LINES; ii++) {
            if (address_mangler.addr_pins[ii] != 255) {
                address_mangler.addr_pins[ii] -= min_addr_pin;
            }
        }
#endif // STM32F4
    }

#if defined(DEBUG_TEST)
    printf("  Address Mangler Configuration:\n");
    printf("    CS1 pin: %d\n", address_mangler.cs1_pin);
    printf("    CS2 pin: %d\n", address_mangler.cs2_pin);
    printf("    CS3 pin: %d\n", address_mangler.cs3_pin);
    printf("    X1 pin: %d\n", address_mangler.x1_pin);
    printf("    X2 pin: %d\n", address_mangler.x2_pin);
    printf("    Address pins mapping (after any left shift to base 0):\n");
    for (int ii = 0; ii < MAX_ADDR_LINES; ii++) {
        if (address_mangler.addr_pins[ii] != 255) {
            printf("      Logical A%d -> GPIO %d\n", ii, address_mangler.addr_pins[ii]);
        }
    }
#endif // DEBUG_TEST
}

static struct {
    uint8_t data_pins[NUM_DATA_LINES];
    int initialized;
} byte_demangler = {0};

void create_byte_demangler(const json_config_t* config) {
    memcpy(byte_demangler.data_pins, config->mcu.pins.data, sizeof(byte_demangler.data_pins));
    if (!strcmp(config->mcu.family, "rp2350")) {
        // See how many data pins are valid
        uint8_t num_valid_data_pins = 0;
        for (int ii = 0; ii < NUM_DATA_LINES; ii++) {
            if (config->mcu.pins.data[ii] < 255) {
                num_valid_data_pins++;
            } else {
                break;
            }
        }
        for (int ii = 0; ii < num_valid_data_pins; ii++) {
            // RP2350 uses a higher byte for data lines, but still expects to
            // read a single byte at a time - the RP2350 hardware takes care
            // of getting the value shifted. 
            byte_demangler.data_pins[ii] = byte_demangler.data_pins[ii] % 8;
        }
    }
    byte_demangler.initialized = 1;
}

// lookup_rom_byte - Simulates the lookup of a byte from the ROM image based on the mangled address
uint8_t lookup_rom_byte(uint8_t set, uint32_t mangled_addr) {  // Removed unused CS parameters
    return rom_set[set].data[mangled_addr];
}

uint32_t create_mangled_address(
    size_t rom_pins,
    uint32_t logical_addr,
    uint8_t cs1,
    uint8_t cs2,
    uint8_t cs3,
    uint8_t x1,
    uint8_t x2
) {
    assert(address_mangler.initialized);
    
    uint32_t mangled = 0;

    uint8_t max_addr_pins;
    
    if (rom_pins == 24) {
        // Strictly these asserts aren't valid for RP2350 as one could use later pins for CS lines,
        // but OK for now
        assert(address_mangler.cs1_pin <= 15);
        assert(cs1 <= 1);
        if (address_mangler.cs2_pin != 255) {
            assert(address_mangler.cs2_pin <= 15);
            // CS2 does not have to be provided
            if (address_mangler.cs3_pin != 255) {
                assert(address_mangler.cs3_pin <= 15);
            }
            // CS3 does not have to be provided
        }
        assert(address_mangler.x1_pin <= 15);
        assert(address_mangler.x2_pin <= 15);
        assert(x1 <= 1);
        assert(x2 <= 1);

        // Set CS selection bits (active low)
        if (cs1 == 1) mangled |= (1 << address_mangler.cs1_pin);
        if (cs2 == 1) mangled |= (1 << address_mangler.cs2_pin);
        if (cs3 == 1) mangled |= (1 << address_mangler.cs3_pin);
        if (x1 == 1)  mangled |= (1 << address_mangler.x1_pin);  
        if (x2 == 1)  mangled |= (1 << address_mangler.x2_pin);

        max_addr_pins = 16;
    } else {
    // 28-pin ROMs
        max_addr_pins = 16;
#if defined(RP235X)
        // RP235X: CS lines are part of the address space
        // Map CS bits to their physical pin positions
        if (cs1 != 255) {
            if (cs1 == 1) mangled |= (1 << address_mangler.cs1_pin);
        }
        if (cs2 != 255 && address_mangler.cs2_pin != 255) {
            if (cs2 == 1) mangled |= (1 << address_mangler.cs2_pin);
        }
        if (cs3 != 255 && address_mangler.cs3_pin != 255) {
            if (cs3 == 1) mangled |= (1 << address_mangler.cs3_pin);
        }
        max_addr_pins = 18;
#endif // RP235X
    }
    
    // Map logical address bits to configured GPIO positions
    for (int i = 0; i < MAX_ADDR_LINES; i++) {
        if (logical_addr & (1 << i)) {
            assert(address_mangler.addr_pins[i] <= (max_addr_pins-1));
            mangled |= (1 << address_mangler.addr_pins[i]);
        }
    }

    return mangled;
}

uint8_t demangle_byte(uint8_t mangled_byte) {
    assert(byte_demangler.initialized);
    
    uint8_t logical = 0;
    
    for (int i = 0; i < 8; i++) {
        assert(byte_demangler.data_pins[i] <= 7);
        if (mangled_byte & (1 << byte_demangler.data_pins[i])) {
            logical |= (1 << i);
        }
    }

    return logical;
}

// Convert ROM type number to string
const char* rom_type_to_string(sdrr_rom_type_t rom_type) {
    switch (rom_type) {
        case CHIP_TYPE_2316: return "2316";
        case CHIP_TYPE_2332: return "2332";  
        case CHIP_TYPE_2364: return "2364";
        case CHIP_TYPE_23128: return "23128";
        case CHIP_TYPE_23256: return "23256";
        case CHIP_TYPE_23512: return "23512";
        case CHIP_TYPE_231024: return "231024";
        case CHIP_TYPE_2716: return "2716";
        case CHIP_TYPE_2732: return "2732";
        case CHIP_TYPE_2764: return "2764";
        case CHIP_TYPE_27128: return "27128";
        case CHIP_TYPE_27256: return "27256";
        case CHIP_TYPE_27512: return "27512";
        default: return "unknown";
    }
}

uint8_t get_num_cs(sdrr_rom_type_t rom_type) {
    switch (rom_type) {
        case CHIP_TYPE_2316:
        case CHIP_TYPE_23128:
            return 3;
        case CHIP_TYPE_2332:
        case CHIP_TYPE_23256:
        case CHIP_TYPE_23512:
        case CHIP_TYPE_2716:
        case CHIP_TYPE_2732:
        case CHIP_TYPE_2764:
        case CHIP_TYPE_27128:
        case CHIP_TYPE_27256:
        case CHIP_TYPE_27512:
            return 2;
        case CHIP_TYPE_2364:
        case CHIP_TYPE_231024:
            return 1;
        default:
            assert(0 && "Unknown ROM type in num_cs");
            return 0;
    }
}

static const uint8_t cs_combos_1[2][3] = {{0,255,255}, {1,255,255}};
static const uint8_t cs_combos_2[4][3] = {{0,0,255}, {0,1,255}, {1,0,255}, {1,1,255}};
static const uint8_t cs_combos_3[8][3] = {{0,0,0}, {0,0,1}, {0,1,0}, {0,1,1},
                                           {1,0,0}, {1,0,1}, {1,1,0}, {1,1,1}};

uint8_t cs_combinations(sdrr_rom_type_t rom_type, uint8_t **combos) {
    uint8_t num_cs = get_num_cs(rom_type);
    switch (num_cs) {
        case 1:
            *combos = (uint8_t *)cs_combos_1;
            return 2;
        case 2:
            *combos = (uint8_t *)cs_combos_2;
            return 4;
        case 3:
            *combos = (uint8_t *)cs_combos_3;
            return 8;
        default:
            assert(0 && "Unknown number of CS lines in cs_combinations");
            return 0;
    }
}

// Convert CS state number to string
const char* cs_state_to_string(int cs_state) {
    switch (cs_state) {
        case CS_ACTIVE_LOW: return "active_low";
        case CS_ACTIVE_HIGH: return "active_high";
        case CS_NOT_USED: return "not_used";
        default: return "unknown";
    }
}

// Get expected ROM size for type
size_t get_expected_rom_size(sdrr_rom_type_t rom_type) {
    switch (rom_type) {
        case CHIP_TYPE_2316: return 2048;
        case CHIP_TYPE_2332: return 4096;
        case CHIP_TYPE_2364: return 8192;
        case CHIP_TYPE_23128: return 16384;
        case CHIP_TYPE_23256: return 32768;
        case CHIP_TYPE_23512: return 65536;
        case CHIP_TYPE_231024: return 131072;
        case CHIP_TYPE_2716: return 2048;
        case CHIP_TYPE_2732: return 4096;
        case CHIP_TYPE_2764: return 8192;
        case CHIP_TYPE_27128: return 16384;
        case CHIP_TYPE_27256: return 32768;
        case CHIP_TYPE_27512: return 65536;
        default: return 0;
    }
}

sdrr_rom_type_t rom_type_from_string(const char* type_str) {
    if (strcmp(type_str, "2316") == 0) return CHIP_TYPE_2316;
    else if (strcmp(type_str, "2332") == 0) return CHIP_TYPE_2332;
    else if (strcmp(type_str, "2364") == 0) return CHIP_TYPE_2364;
    else if (strcmp(type_str, "23128") == 0) return CHIP_TYPE_23128;
    else if (strcmp(type_str, "23256") == 0) return CHIP_TYPE_23256;
    else if (strcmp(type_str, "23512") == 0) return CHIP_TYPE_23512;
    else if (strcmp(type_str, "231024") == 0) return CHIP_TYPE_231024;
    else if (strcmp(type_str, "2704") == 0) return CHIP_TYPE_2704;
    else if (strcmp(type_str, "2708") == 0) return CHIP_TYPE_2708;
    else if (strcmp(type_str, "2716") == 0) return CHIP_TYPE_2716;
    else if (strcmp(type_str, "2732") == 0) return CHIP_TYPE_2732;
    else if (strcmp(type_str, "2764") == 0) return CHIP_TYPE_2764;
    else if (strcmp(type_str, "27128") == 0) return CHIP_TYPE_27128;
    else if (strcmp(type_str, "27256") == 0) return CHIP_TYPE_27256;
    else if (strcmp(type_str, "27512") == 0) return CHIP_TYPE_27512;
    else return -1; // Unknown type
}

void print_compiled_rom_info(void) {
    printf("\n=== Compiled ROM Sets Analysis ===\n");
    printf("Total ROM images: %d\n", SDRR_NUM_IMAGES);
    printf("Total ROM sets: %d\n", sdrr_rom_set_count);
    
    // Print details for each ROM set
    for (int set_idx = 0; set_idx < sdrr_rom_set_count; set_idx++) {
        printf("\nROM Set %d:\n", set_idx);
        printf("  Size: %u bytes (%s)\n", rom_set[set_idx].size, 
               (rom_set[set_idx].size == 16384) ? "16KB" : 
               (rom_set[set_idx].size == 65536) ? "64KB" : "other");
        printf("  ROM count: %d\n", rom_set[set_idx].rom_count);
        
        // Expected image size based on ROM count
#if defined(RP235X)
        const char* expected_size = "64KB";
        const size_t expected_size_bytes = 65536;
#else // ! RP235X
        const char* expected_size = (rom_set[set_idx].rom_count == 1) ? "16KB" : "64KB";
        const size_t expected_size_bytes = (rom_set[set_idx].rom_count == 1) ? 16384 : 65536;
#endif // RP235X
        printf("  Expected size: %s", expected_size);
        if (rom_set[set_idx].size == expected_size_bytes) {
            printf(" ✓\n");
        } else {
            printf(" ✗\n");
        }
        
        // Print details for each ROM in this set
        for (int rom_idx = 0; rom_idx < rom_set[set_idx].rom_count; rom_idx++) {
            const sdrr_rom_info_t *rom_info = rom_set[set_idx].roms[rom_idx];
            
            printf("  ROM %d:\n", rom_idx);
#if defined(BOOT_LOGGING)
            printf("    File: %s\n", rom_info->filename);
#endif // BOOT_LOGGING
            printf("    Type: %s (%d)\n", rom_type_to_string(rom_info->rom_type), rom_info->rom_type);
            printf("    CS1: %s (%d)", cs_state_to_string(rom_info->cs1_state), rom_info->cs1_state);
            
            if (rom_info->cs2_state != CS_NOT_USED) {
                printf(", CS2: %s (%d)", cs_state_to_string(rom_info->cs2_state), rom_info->cs2_state);
            }
            if (rom_info->cs3_state != CS_NOT_USED) {
                printf(", CS3: %s (%d)", cs_state_to_string(rom_info->cs3_state), rom_info->cs3_state);
            }
            printf("\n");
            
            // Expected ROM size check
            size_t expected_rom_size = get_expected_rom_size(rom_info->rom_type);
            printf("    Expected ROM size: %zu bytes\n", expected_rom_size);
        }
        
        // Show first 8 bytes of the ROM set data
        printf("  First 8 bytes of mangled set data: ");
        for (size_t j = 0; j < 8 && j < rom_set[set_idx].size; j++) {
            printf("0x%02X ", rom_set[set_idx].data[j]);
        }
        printf("\n");
    }
}
