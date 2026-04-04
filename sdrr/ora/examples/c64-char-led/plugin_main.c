// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

// Watches activity on a C64 character ROM.  If CHAR_LED_ON is detected,
// turns on the status LED, and if CHAR_LED_OFF is detected, turns it off.
//
// Note that the VIC-II is constantly retrieving character data from the ROM,
// even on cycles nothing is drawn on screen, and this causes spurious address
// captures.  CHAR_LED_ON and CHAR_LED_OFF are chosen to be uncommonly
// spuriously generated.

#include "plugin.h"
#include "apio.h"
#include "piodma/dmareg.h"
#include "config_base.h"

// Logic to allow this plugin to be built as either a system or user plugin,
// based on the PLUGIN_TYPE passed in on make.
ORA_DEFINE_USER_PLUGIN(
    eye_main,
    0, 1, 0, 0,     // Plugin version
    0, 6, 7         // Minimum One ROM firmware version required
);

// 64 word ring buffer - must be aligned to its own size for DMA RING_SIZE
static uint16_t __attribute__((aligned(128))) ring_buf[64];

#define RING_BUF_ENTRIES    64
#define CHAR_LED_ON         0x3E    // >
#define CHAR_LED_OFF        0x3F    // ?

#define EYE_BLOCK           0
#define EYE_CS_IRQ          0
#define EYE_SM_CS_MON       0
#define EYE_SM_ADDR_CAP     1
#define EYE_SMS_MASK        ((1 << EYE_SM_CS_MON) | (1 << EYE_SM_ADDR_CAP))
#define EYE_DMA_CH          2
#define EYE_RING_SIZE_LOG2  7       // 2^7 = 128 bytes

#define EYE_CS_BASE_PIN     10
#define EYE_NUM_CS_PINS     3
#define EYE_ADDR_BASE_PIN   8

static void eye_pio_dma_init(ora_log_fn_t log);
static void calc_char_match(
    ora_log_fn_t log,
    const sdrr_info_t *info,
    uint8_t char_num,
    uint16_t *match,
    uint16_t *mask
);

void eye_main(
    ora_lookup_fn_t ora_lookup_fn,
    ora_plugin_type_t plugin_type,
    const ora_entry_args_t *entry_args
) {
    (void)plugin_type;
    (void)entry_args;

    // Get plugin APIs
    ora_set_status_led_fn_t set_status_led = ora_lookup_fn(ORA_ID_SET_STATUS_LED);
    ora_log_fn_t log = ora_lookup_fn(ORA_ID_LOG);
    ora_get_firmware_info_fn_t get_sdrr_info = ora_lookup_fn(ORA_ID_GET_FIRMWARE_INFO);

    log("One ROM - The Eye of Sauron");

    // Start with status LED off
    log("Status LED off");
    set_status_led(0);

    // Set up PIOs and DMA
    eye_pio_dma_init(log);

    // Precalculate the addresses to match on for the different characters
    uint16_t led_on_match;
    uint16_t led_on_mask;
    uint16_t led_off_match;
    uint16_t led_off_mask;
    sdrr_info_t *info = (sdrr_info_t *)get_sdrr_info();
    calc_char_match(log, info, CHAR_LED_ON, &led_on_match, &led_on_mask);
    calc_char_match(log, info, CHAR_LED_OFF, &led_off_match, &led_off_mask);
    log("LED on match: 0x%04x, mask: 0x%04x", led_on_match, led_on_mask);
    log("LED off match: 0x%04x, mask: 0x%04x", led_off_match, led_off_mask);

    log("Starting main loop");

    // Set the read pointer to the last DMA write address (which will be the
    // start of the ring buffer if it hasn't done anything yet).  This ensures
    // we throw away anything captured up to this point, which should just be
    // garbage due to the VIC-II starting before the CPU and One ROM.
    uint16_t *read_ptr = (uint16_t *)DMA_CH_REG(EYE_DMA_CH)->write_addr;
    while (1) {
        uint16_t *write_ptr = (uint16_t *)DMA_CH_REG(EYE_DMA_CH)->write_addr;

        while (read_ptr != write_ptr) {
            uint16_t addr = *read_ptr;

            if ((addr & led_on_mask) == led_on_match) {
                log("Status LED on");
                set_status_led(1);
            } else if ((addr & led_off_mask) == led_off_match) {
                log("Status LED off");
                set_status_led(0);
            }

            if (++read_ptr >= ring_buf + 64)
                read_ptr = ring_buf;
        }
    }
}

static void eye_pio_dma_init(ora_log_fn_t log) {
    APIO_ASM_INIT();
    APIO_SET_BLOCK(EYE_BLOCK);
    APIO_GPIOBASE_0();

    // SM D - CS monitor
    // Non-contiguous CS pins: GPIO 10 (/CS1) and GPIO 12 (/CS2), GPIO 11 between them
    // CS active when pins read 0b000 (0) or 0b010 (2)
    APIO_SET_SM(EYE_SM_CS_MON);

    APIO_ADD_INSTR(APIO_SET_Y(2));

    APIO_LABEL_NEW(eye_inactive);
    APIO_ADD_INSTR(APIO_MOV_X_PINS);

    APIO_LABEL_NEW_OFFSET(eye_active, 2);
    APIO_ADD_INSTR(APIO_JMP_NOT_X(APIO_LABEL(eye_active)));
    APIO_ADD_INSTR(APIO_JMP_X_NOT_Y(APIO_LABEL(eye_inactive)));

    // eye_active:
    APIO_ADD_INSTR(APIO_IRQ_SET(EYE_CS_IRQ));

    APIO_WRAP_BOTTOM();
    APIO_LABEL_NEW(eye_test_if_inactive);
    APIO_ADD_INSTR(APIO_MOV_X_PINS);
    APIO_ADD_INSTR(APIO_JMP_NOT_X(APIO_LABEL(eye_test_if_inactive)));
    APIO_WRAP_TOP();
    APIO_ADD_INSTR(APIO_JMP_X_NOT_Y(APIO_LABEL(eye_inactive)));

    APIO_SM_CLKDIV_SET(1, 0);
    APIO_SM_EXECCTRL_SET(0);
    APIO_SM_SHIFTCTRL_SET(
        APIO_IN_COUNT(EYE_NUM_CS_PINS) |
        APIO_IN_SHIFTDIR_L
    );
    APIO_SM_PINCTRL_SET(
        APIO_IN_BASE(EYE_CS_BASE_PIN)
    );
    APIO_SM_JMP_TO_START();

    // SM E - waits for IRQ from SM D, captures 16-bit address
    APIO_SET_SM(EYE_SM_ADDR_CAP);

    APIO_ADD_INSTR(APIO_WAIT_IRQ_HIGH(EYE_CS_IRQ));
    APIO_WRAP_TOP();
    APIO_ADD_INSTR(APIO_IN_PINS(16));

    APIO_SM_CLKDIV_SET(1, 0);
    APIO_SM_EXECCTRL_SET(0);
    APIO_SM_SHIFTCTRL_SET(
        APIO_AUTOPUSH        |
        APIO_PUSH_THRESH(16) |
        APIO_IN_SHIFTDIR_L
    );
    APIO_SM_PINCTRL_SET(
        APIO_IN_BASE(EYE_ADDR_BASE_PIN)
    );
    APIO_SM_JMP_TO_START();

    APIO_END_BLOCK();

    // DMA2 - SM E RX FIFO -> ring_buf circular write
    volatile dma_ch_reg_t *dma_reg = DMA_CH_REG(EYE_DMA_CH);
    dma_reg->read_addr      = (uint32_t)&APIO0_SM_RXF(EYE_SM_ADDR_CAP);
    dma_reg->write_addr     = (uint32_t)ring_buf;
    dma_reg->transfer_count = 0xffffffff;
    dma_reg->ctrl_trig =
        DMA_CTRL_TRIG_EN |
        DMA_CTRL_TRIG_DATA_SIZE_16BIT |
        DMA_CTRL_RING_SIZE(EYE_RING_SIZE_LOG2) |
        DMA_CTRL_RING_SEL |
        DMA_CTRL_INCR_WRITE |
        DMA_CTRL_TRIG_CHAIN_TO(EYE_DMA_CH) |
        DMA_CTRL_TRIG_TREQ_SEL(APIO_DREQ_PIO_X_SM_Y_RX(0,
            EYE_SM_ADDR_CAP));

    log("Enabling The Eye's PIOs");
    APIO_ENABLE_SMS(EYE_BLOCK, EYE_SMS_MASK);
}

static void calc_char_match(
    ora_log_fn_t log,
    const sdrr_info_t *info,
    uint8_t char_num,
    uint16_t *match,
    uint16_t *mask
) {
    (void)log;
    // Shift the character 3 bits left, as A0-2 are the row within the
    // character, and we want to ignore those
    uint32_t char_addr = char_num * 8;
    *match = 0;
    *mask = 0;
    // Skip A0-2 which are the rows within the character
    for (int i = 3; i < 12; i++) {
        //log("Checking if addr bit %d (GPIO %d) is used for addr bit %d",
        //    i, info->pins->addr[i], i - 3);
        uint8_t bit_pos = info->pins->addr[i] - EYE_ADDR_BASE_PIN;
        *mask |= (1 << bit_pos);
        if (char_addr & (1 << i)) {
            *match |= (1 << bit_pos);
        }
    }
}