// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

// Implement's One ROM Fire's plugin API 

#include "include.h"

#if defined(RP235X)

#define RP235X_INCLUDES

#include "plugin.h"

uint8_t check_plugin_valid(
    const ora_plugin_header_t *header,
    const ora_plugin_type_t expected_type,
    uint8_t index
) {
    if (header->magic != ORA_PLUGIN_MAGIC) {
        ERR("Invalid plugin - badmagic 0x%08x", header->magic);
        return 0;
    }
    if (header->api_version != ORA_PLUGIN_VERSION_1) {
        ERR("Invalid plugin - version 0x%08x", header->api_version);
        return 0;
    }
    if (header->plugin_type != expected_type) {
        ERR("Invalid plugin - type %d, expected %d", header->plugin_type, expected_type);
        return 0;
    }
    
    // A plugin is expected to be located at 0x10010000, 0x10020000, etc based
    // on the specific ROM set it is.
    uint32_t expected_launch_region = (0x1001 + index) << 16;
    uint32_t entry_addr = (uint32_t)(uintptr_t)header->entry;
    if ((entry_addr & ~expected_launch_region) >= 0x10000) {
        ERR("Invalid plugin - ep 0x%08x", entry_addr, expected_launch_region);
        return 0;
    }

    return 1;
}

#if !defined(TEST_BUILD)

void ora_reboot_bootsel(void) {
    enter_bootloader();

    // Do not return
    while (1);
}

void *ora_alloc(size_t size) {
    (void)size;
    return NULL;
}

void *ora_get_firmware_info(void) {
    void *info = (void *)&sdrr_info;
    return info;
}

void *ora_get_runtime_info(void) {
    void *info = (void *)&sdrr_runtime_info;
    return info;
}

void ora_log(const char* msg, ...) {
#if defined(BOOT_LOGGING)
    va_list args;
    va_start(args, msg);
    do_log_v(msg, args);
    va_end(args);
#else
    (void)msg;
#endif // BOOT_LOGGING
}

void ora_err_log(const char* msg, ...) {
#if defined(BOOT_LOGGING)
    do_err_log_prefix();
    va_list args;
    va_start(args, msg);
    do_log_v(msg, args);
    va_end(args);
#else
    (void)msg;
#endif // BOOT_LOGGING
}

void ora_debug_log(const char* msg, ...) {
#if defined(BOOT_LOGGING) && defined(DEBUG_LOGGING)
    do_debug_log_prefix();
    va_list args;
    va_start(args, msg);
    do_log_v(msg, args);
    va_end(args);
#else
    (void)msg;
#endif // BOOT_LOGGING && DEBUG_LOGGING
}

size_t plugin_get_free_mem(void) {
    return 0;
}

void ora_set_status_led(uint8_t on) {
    uint8_t pin = sdrr_info.pins->status;
    if (sdrr_info.status_led_enabled && 
        sdrr_info.extra->runtime_info->status_led_enabled &&
        sdrr_info.pins->status_port == PORT_0 &&
        pin <= MAX_USED_GPIOS) {
        if (on) {
            status_led_on(pin);
        } else {
            status_led_off(pin);
        }
    }
}

void ora_setup_usb(void) {
    setup_usb_pll();
    setup_usb_controller();
}

void ora_setup_adc(void) {
    setup_usb_pll();
    setup_adc();
}

void ora_enable_irq(ora_irq_t irq, uint8_t enable) {
    if (enable) {
        if (irq < 32) {
            NVIC_ISER0 = (1u << irq);
        } else {
            NVIC_ISER1 = (1u << (irq - 32));
        }
    } else {
        if (irq < 32) {
            NVIC_ICER0 = (1u << irq);
        } else {
            NVIC_ICER1 = (1u << (irq - 32));
        }
    }
}

void ora_register_irq(ora_irq_t irq, ora_irq_handler_t handler) {
    switch (irq) {
        case ORA_IRQ_TIMER0_IRQ_0:
            sdrr_runtime_info.timer0_irq_0_handler = handler;
            if (handler == NULL) {
                ora_enable_irq(ORA_IRQ_TIMER0_IRQ_0, 0);
            }
            break;
        case ORA_IRQ_USBCTRL_IRQ:
            sdrr_runtime_info.usbctrl_irq_handler = handler;
            if (handler == NULL) {
                ora_enable_irq(ORA_IRQ_USBCTRL_IRQ, 0);
            }
            break;
        default:
            ERR("Invalid IRQ number for registration: %d", irq);
            break;
    }
}

void ora_set_plugin_context(void *context) {
    sdrr_runtime_info.system_plugin_context = context;
}

void *ora_get_plugin_context(void) {
    return sdrr_runtime_info.system_plugin_context;
}

uint32_t ora_get_sysclk_mhz(void) {
    uint16_t sysclk_mhz = sdrr_runtime_info.sysclk_mhz;
    return (uint32_t)sysclk_mhz;
}

uint32_t ora_get_clkref_mhz(void) {
    // The clkref frequency is fixed at 12 MHz on Fire, so we can just return
    // that here.
#define CLKREF_MHZ 12
    uint32_t clk_ref_div = (CLOCK_REF_DIV >> 16) & 0xFF;
    clk_ref_div = clk_ref_div ? clk_ref_div : 1;
    return (CLKREF_MHZ / clk_ref_div);
}

uint32_t ora_get_chip_size_from_type(uint32_t chip_type) {
    if (chip_type < NUM_CHIP_TYPES) {
        return chip_size_from_type[chip_type];
    }
    return 0u;
}

void *ora_fn_lookup(api_id_t id) {
    switch (id) {
        case ORA_ID_REBOOT_BOOTSEL:
            return ora_reboot_bootsel;
        case ORA_ID_ALLOC:
            return ora_alloc;
        case ORA_ID_GET_FIRMWARE_INFO:
            return ora_get_firmware_info;
        case ORA_ID_LOG:
            return ora_log;
        case ORA_ID_ERR_LOG:
            return ora_err_log;
        case ORA_ID_DEBUG_LOG:
            return ora_debug_log;
        case ORA_ID_GET_FREE_MEM:
            return plugin_get_free_mem;
        case ORA_ID_SET_STATUS_LED:
            return ora_set_status_led;
        case ORA_ID_SETUP_USB:
            return ora_setup_usb;
        case ORA_ID_SETUP_ADC:
            return ora_setup_adc;
        case ORA_ID_REGISTER_IRQ:
            return ora_register_irq;
        case ORA_ID_SET_PLUGIN_CONTEXT:
            return ora_set_plugin_context;
        case ORA_ID_GET_PLUGIN_CONTEXT:
            return ora_get_plugin_context;
        case ORA_ID_GET_SYSCLK_MHZ:
            return ora_get_sysclk_mhz;
        case ORA_ID_ENABLE_IRQ:
            return ora_enable_irq;
        case ORA_ID_GET_CLKREF_MHZ:
            return ora_get_clkref_mhz;
        case ORA_ID_GET_RUNTIME_INFO:
            return ora_get_runtime_info;
        case ORA_ID_GET_CHIP_SIZE_FROM_TYPE:
            return ora_get_chip_size_from_type;
        default:
            return NULL;
    }
}

static void fifo_drain(void) {
    while (SIO_FIFO_ST & 1u)
        (void)SIO_FIFO_RD;
}

static void fifo_push_blocking(uint32_t val) {
    while (!(SIO_FIFO_ST & 2u))
        ;
    SIO_FIFO_WR = val;

    // Wake up core 1 if it's in WFE waiting for data
    __asm volatile ("sev");
}

static uint32_t fifo_pop_blocking(void) {
    while (!(SIO_FIFO_ST & 1u))
        ;
    return SIO_FIFO_RD;
}

static void reset_core1(void) {
    // Hard reset core 1
    PSM_FRCE_OFF_SET = PSM_PROC1_BIT;
    // Read back to confirm and fence any store buffering
    while (!(PSM_FRCE_OFF & PSM_PROC1_BIT))
        ;

    // Bring core 1 out of reset - its bootrom will drain its FIFO
    // then push a 0 to tell us it's ready
    PSM_FRCE_OFF_CLR = PSM_PROC1_BIT;

    // Wait for core 1 bootrom ready signal
    uint32_t value = fifo_pop_blocking();
    if (value != 0) {
        ERR("Unexpected value from core 1 bootrom: 0x%08x", value);
    }
}

static void core1_main(void) {
    // Read a single uint32_t from the FIFO
    DEBUG("Core 1 started");
    uint32_t core1_plugin_entry = fifo_pop_blocking();
    core1_plugin_entry |= 1;
    ora_plugin_entry_t entry = (ora_plugin_entry_t)(uintptr_t)core1_plugin_entry;
    DEBUG("Core 1 launching plugin at 0x%08x", core1_plugin_entry);
    entry(ora_fn_lookup, ORA_PLUGIN_TYPE_SYSTEM, ORA_CORE_1);
}

void launch_core1(ora_plugin_entry_t plugin_entry) {
    extern uint32_t _estack;
    uint32_t core1_stack_top = (uint32_t)&_estack - 1024;

    // Reset core 1
    DEBUG("Resetting core 1");
    reset_core1();

    uint32_t cmd_sequence[] = {
        0,
        0,
        1,
        SCB_VTOR,   // Share vector table with core 0
        core1_stack_top,
        (uint32_t)(uintptr_t)core1_main | 1, // Set thumb bit
    };

    uint32_t seq = 0;
    uint32_t count = sizeof(cmd_sequence) / sizeof(cmd_sequence[0]);

    do {
        uint32_t cmd = cmd_sequence[seq];
        if (!cmd) {
            fifo_drain();
            __asm volatile ("sev");
        }
        fifo_push_blocking(cmd);
        uint32_t response = fifo_pop_blocking();
        seq = (cmd == response) ? seq + 1 : 0;
    } while (seq < count);

    uint32_t entry = (uint32_t)(uintptr_t)plugin_entry | 1; // Set thumb bit
    fifo_push_blocking(entry);
}

void ora_launch_plugins(const sdrr_info_t *info) {
    // Launch any available system plugin on core 1
    uint8_t system_plugin = 0;
    if (info->metadata_header->rom_set_count >= 1) {
        const sdrr_rom_set_t *set0 = &info->metadata_header->rom_sets[0];
        if (set0->roms[0]->rom_type == CHIP_TYPE_SYSTEM_PLUGIN) {
            ora_plugin_header_t *header = (ora_plugin_header_t *)set0->data;
            if (!check_plugin_valid(header, ORA_PLUGIN_TYPE_SYSTEM, 0)) {
                ERR("Invalid system plugin");
            } else {
                const char *filename = set0->roms[0]->filename;
                if (filename != NULL) {
                    LOG("Launching system plugin: %s", filename);
                } else {
                    LOG("Launching system plugin");
                }
                launch_core1(header->entry);
                system_plugin = 1;
            }
        }
    }

    // Launch any available user plugin on core 0 (this core)
    if (info->metadata_header->rom_set_count >= 2) {
        const sdrr_rom_set_t *set1 = &info->metadata_header->rom_sets[1];
        if (set1->roms[0]->rom_type == CHIP_TYPE_USER_PLUGIN) {
            ora_plugin_header_t *header = (ora_plugin_header_t *)set1->data;
            if (!check_plugin_valid(header, ORA_PLUGIN_TYPE_USER, 1)) {
                ERR("Invalid user plugin");
            } else if (!system_plugin) {
                ERR("User plugin present but no valid system plugin - not launching");
            } else {
                const char *filename = set1->roms[0]->filename;
                if (filename != NULL) {
                    LOG("Lauching user plugin: %s", filename);
                } else {
                    LOG("Lauching user plugin");
                }
                // Set thumb bit
                uint32_t entry_addr = (uint32_t)(uintptr_t)header->entry | 1;
                ora_plugin_entry_t entry = (ora_plugin_entry_t)(uintptr_t)entry_addr;
                entry(ora_fn_lookup, ORA_PLUGIN_TYPE_USER, ORA_CORE_0);
            }
        }
    }
}

void irq_handler_timer0_irq_0(void) {
    if (sdrr_runtime_info.timer0_irq_0_handler) {
        ora_irq_handler_t handler = (ora_irq_handler_t)sdrr_runtime_info.timer0_irq_0_handler;
        handler();
    }
}

void irq_handler_usbctrl_irq(void) {
    if (sdrr_runtime_info.usbctrl_irq_handler) {
        ora_irq_handler_t handler = (ora_irq_handler_t)sdrr_runtime_info.usbctrl_irq_handler;
        handler();
    }
}

#endif // !TEST_BUILD
#endif // RP235X