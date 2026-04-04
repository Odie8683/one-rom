// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

// Detects knock sequence on the low order byte of the address bus, then
// awaits a mode command byte and reprograms the kernal ROM accordingly.
//
// Designed to be controlled using the provided modifykernal.prg, which should
// be loaded then run on a real C64, with a One ROM serving the kernal with
// this plugin installed.
//
// Works for 2364 ROMs being served by a Fire 24 C/D/E.  (Only tested on C.)

#include "plugin.h"
#include "apio.h"
#include "piodma/dmareg.h"
#include "config_base.h"

// Plugin header
ORA_DEFINE_USER_PLUGIN(
    c64_kernal_mod_main,
    0, 1, 0, 0,
    0, 6, 7
);

// Ring buffer for captured addresses from PIO, written by DMA.  Must be
// algined to its size, and be a power of 2 in length.
#define RING_BUF_ENTRIES    64
#define RING_SIZE_LOG2      7
static volatile uint16_t __attribute__((aligned(128))) ring_buf[RING_BUF_ENTRIES];

// PIO and DMA configuration
#define EYE_BLOCK           0
#define EYE_CS_IRQ          0
#define EYE_SM_CS_MON       0
#define EYE_SM_ADDR_CAP     1
#define EYE_SMS_MASK        ((1 << EYE_SM_CS_MON) | (1 << EYE_SM_ADDR_CAP))
#define EYE_DMA_CH          2

// Pin configuration
#define EYE_NUM_CS_PINS     1
#define EYE_ADDR_BASE_PIN   8
#define EYE_DATA_BASE_PIN   0

// Default knock sequence: ONEROM! in ASCII
#define KNOCK_LEN 7
static const uint8_t knock_seq[KNOCK_LEN] = {
    'O', 'N', 'E', 'R', 'O', 'M', '!'
};

// The data to update in the kernal ROM.  This updates the boot banner.
static const uint8_t updated_kernal_segment[] = {
    0x20, 0x20, 0x20, 0x20, 0x20, 0x2A, 0x2A, 0x2A,
    0x2A, 0x20, 0x50, 0x4F, 0x57, 0x45, 0x52, 0x45,
    0x44, 0x20, 0x42, 0x59, 0x20, 0x4F, 0x4E, 0x45,
    0x20, 0x52, 0x4F, 0x4D, 0x20, 0x2A, 0x2A, 0x2A,
    0x2A, 0x20, 0x0A, 0x0D, 0x0D, 0x46, 0x52, 0x4F,
    0x4D, 0x20, 0x50, 0x49, 0x45, 0x52, 0x53, 0x2E,
    0x52, 0x4F, 0x43, 0x4B, 0x53, 0x20
};
#define START_MODIFIED_KERNAL_SEGMENT_OFFSET    0x475
#define SRAM_BASE                               0x20000000
#define START_KERNAL_UPDATE_ADDR                (SRAM_BASE + START_MODIFIED_KERNAL_SEGMENT_OFFSET) 

// State for main loop
typedef enum {
    STATE_WATCHING,
    STATE_AWAITING_MODE,
    STATE_DONE,
} eye_state_t;

static void eye_pio_dma_init(ora_log_fn_t log, const sdrr_info_t *info);
static void reprogram_kernal(ora_log_fn_t log, const sdrr_info_t *info);
static void calc_knock_mask(
    const sdrr_info_t *info,
    uint16_t *mask
);
static void calc_knock_match(
    const sdrr_info_t *info,
    uint8_t target,
    uint16_t *match
);
static uint16_t demangle_addr(ora_log_fn_t log, const sdrr_info_t *info, uint16_t physical_addr);

// Plugin entry point and main routine
void c64_kernal_mod_main(
    ora_lookup_fn_t ora_lookup_fn,
    ora_plugin_type_t plugin_type,
    const ora_entry_args_t *entry_args
) {
    (void)plugin_type;
    (void)entry_args;

    // Get the plugin API functions we need
    ora_set_status_led_fn_t set_status_led = ora_lookup_fn(ORA_ID_SET_STATUS_LED);
    ora_log_fn_t log = ora_lookup_fn(ORA_ID_LOG);
    ora_get_firmware_info_fn_t get_sdrr_info = ora_lookup_fn(ORA_ID_GET_FIRMWARE_INFO);

    log("One ROM - C64 Kernal Mod example plugin");

    // Turn status LED off.  It will be turned on when the kernal has been
    // modified.
    set_status_led(0);

    // Get the firmware info, which contains the pin assignments
    sdrr_info_t *info = (sdrr_info_t *)get_sdrr_info();

    // Initialize the PIO state machines and DMA channel to capture addresses
    // when CS is active.
    eye_pio_dma_init(log, info);

    // Precalculate the addresses to match on for the different characters, as
    // address lines are mangled and not necessarily contiguous.
    uint16_t knock_mask;
    uint16_t knock_match[KNOCK_LEN];
    calc_knock_mask(info, &knock_mask);
    for (int i = 0; i < KNOCK_LEN; i++) {
        calc_knock_match(info, knock_seq[i], &knock_match[i]);
        log("Knock[%d]: 0x%02x -> match 0x%04x", i, knock_seq[i], knock_match[i]);
    }
    log("Knock mask: 0x%04x", knock_mask);

    // Initialize state for main loop
    eye_state_t state = STATE_WATCHING;
    int knock_pos = 0;

    // Discard anything captured before we're ready
    volatile uint16_t *read_ptr = (uint16_t *)DMA_CH_REG(EYE_DMA_CH)->write_addr;

    // Main loop: wait for knock sequence, then mode byte, then reprogram kernal
    log("Starting main loop, waiting for knock sequence");
    while (state != STATE_DONE) {
        uint16_t *write_ptr = (uint16_t *)DMA_CH_REG(EYE_DMA_CH)->write_addr;

        while (read_ptr != write_ptr && state != STATE_DONE) {
            uint16_t addr = *read_ptr;
            if (addr & (1<<(10-EYE_ADDR_BASE_PIN))) {
                // Skip address read when CS is inactive.  This is a crude CS
                // debounce handler - avoids matching against addresses
                // captured if CS only goes low for a very short period of
                // time
                if (++read_ptr >= ring_buf + RING_BUF_ENTRIES) {
                    read_ptr = ring_buf;
                }
                continue;
            }
            //log("Captured address: 0x%04x", demangle_addr(log, info, addr));

            switch (state) {
                case STATE_WATCHING:
                    if ((addr & knock_mask) == knock_match[knock_pos]) {
                        //log("Knock %d matched (0x%04x)", knock_pos, demangle_addr(log, info, addr));
                        if (++knock_pos == KNOCK_LEN) {
                            //log("Knock sequence matched");
                            knock_pos = 0;
                            state = STATE_AWAITING_MODE;
                        }
                    } else {
                        // Didn't match wherever we were in the sequence, so
                        // check against the start.
                        if (knock_pos > 0) {
                            //log("Knock broke at %d, addr=0x%04x", knock_pos, demangle_addr(log, info, addr));
                        }
                        knock_pos = ((addr & knock_mask) == knock_match[0]) ? 1 : 0;
                        if (knock_pos == 1) {
                            //log("Knock matched start of sequence (0x%04x)", demangle_addr(log, info, addr));
                        }
                    }
                    break;

                case STATE_AWAITING_MODE:
                    // Ingore the mode byte
                    log("Mode byte received");
                    reprogram_kernal(log, info);
                    state = STATE_DONE;
                    break;

                case STATE_DONE:
                    break;
            }

            if (++read_ptr >= ring_buf + RING_BUF_ENTRIES) {
                read_ptr = ring_buf;
            }
        }
    }

    log("Reprogramming complete - soft reset (SYS64738) or power cycle to activate new kernal");
    set_status_led(1);
}

// Sets up the PIO state machines and DMA channel to capture addresses when CS
// is active.
// 
// - SM D monitors the CS line, and triggers an IRQ when it goes active, then
//   waits for it to go inactive again.
//
// - SM E waits for the IRQ from SM D, then captures the address lines into
//   its RX FIFO, which is read by the DMA.
//
// - DMA channel transfers captured addresses from SM E's RX FIFO into a ring
//   buffer in RAM, for the main loop to process.
static void eye_pio_dma_init(ora_log_fn_t log, const sdrr_info_t *info) {
    APIO_ASM_INIT();
    APIO_SET_BLOCK(EYE_BLOCK);
    APIO_GPIOBASE_0();

    // SM D - CS monitor, single active-low CS1 on GPIO 10
    APIO_SET_SM(EYE_SM_CS_MON);

    // Wait for CS to go active (pin reads 0)
    APIO_WRAP_BOTTOM();
    APIO_LABEL_NEW(eye_inactive);
    APIO_ADD_INSTR(APIO_MOV_X_PINS);
    APIO_ADD_INSTR(APIO_JMP_X_DEC(APIO_LABEL(eye_inactive)));  // CS inactive (non-zero), loop

    // CS active - signal address capture SM
    APIO_ADD_INSTR(APIO_IRQ_SET(EYE_CS_IRQ));

    // Wait for CS to go inactive (pin reads non-zero)
    APIO_LABEL_NEW(eye_test_if_inactive);
    APIO_ADD_INSTR(APIO_MOV_X_PINS);
    APIO_WRAP_TOP();
    APIO_ADD_INSTR(APIO_JMP_NOT_X(APIO_LABEL(eye_test_if_inactive)));  // CS still active (zero), loop

    APIO_SM_CLKDIV_SET(1, 0);
    APIO_SM_EXECCTRL_SET(0);
    APIO_SM_SHIFTCTRL_SET(
        APIO_IN_COUNT(EYE_NUM_CS_PINS) |
        APIO_IN_SHIFTDIR_L
    );
    APIO_SM_PINCTRL_SET(
        APIO_IN_BASE(info->pins->cs1)
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
        DMA_CTRL_RING_SIZE(RING_SIZE_LOG2) |
        DMA_CTRL_RING_SEL |
        DMA_CTRL_INCR_WRITE |
        DMA_CTRL_TRIG_CHAIN_TO(EYE_DMA_CH) |
        DMA_CTRL_TRIG_TREQ_SEL(APIO_DREQ_PIO_X_SM_Y_RX(0,
            EYE_SM_ADDR_CAP));

    log("Enabling The Eye of Sauron's PIOs");
    APIO_ENABLE_SMS(EYE_BLOCK, EYE_SMS_MASK);
}

// Precalculates the bit mask to apply to captured addresses to extract the
// bits to compare for the knock sequence, based on the pin mapping.
static void calc_knock_mask(
    const sdrr_info_t *info,
    uint16_t *mask
) {
    *mask = 0;
    for (int i = 0; i < 8; i++) {
        uint8_t bit_pos = info->pins->addr[i] - EYE_ADDR_BASE_PIN;
        *mask |= (1 << bit_pos);
    }
}

// Precalculates the value to match against for a given target character,
// based on the pin mapping.  This is the value that will be captured by the
// PIO when that character is on the low byte of the address bus.
static void calc_knock_match(
    const sdrr_info_t *info,
    uint8_t target,
    uint16_t *match
) {
    *match = 0;
    for (int i = 0; i < 8; i++) {
        uint8_t bit_pos = info->pins->addr[i] - EYE_ADDR_BASE_PIN;
        if (target & (1 << i)) {
            *match |= (1 << bit_pos);
        }
    }
}

// Converts logical address to physical address as it is captured by the PIO,
// based on the pin mapping.  This is the offset into the SRAM table where the
// byte at this logical address is stored and served by One ROM
static uint32_t remap_addr(const sdrr_info_t *info, uint32_t logical_addr) {
    uint32_t physical_addr = 0;
    for (int b = 0; b < 13; b++) {
        if (logical_addr & (1u << b)) {
            uint8_t bit_pos = info->pins->addr[b] - EYE_ADDR_BASE_PIN;
            physical_addr |= (1u << bit_pos);
        }
    }
    return physical_addr;
}

// Converts logical data byte to physical data byte as it must be stored in
// SRAM, based on the pin mapping
static uint8_t remap_data(const sdrr_info_t *info, uint8_t logical_data) {
    uint8_t physical_data = 0;
    for (int b = 0; b < 8; b++) {
        if (logical_data & (1u << b)) {
            uint8_t bit_pos = info->pins->data[b] - EYE_DATA_BASE_PIN;
            physical_data |= (1u << bit_pos);
        }
    }
    return physical_data;
}

// Reprogram the kernal ROM - by updating the appropriate bytes in the
// RP2350's SRAM
static void reprogram_kernal(ora_log_fn_t log, const sdrr_info_t *info) {
    log("Reprogramming kernal ROM");

    size_t len = sizeof(updated_kernal_segment);
    uint8_t *sram = (uint8_t *)SRAM_BASE;

    for (size_t i = 0; i < len; i++) {
        uint32_t physical_addr = remap_addr(info, START_MODIFIED_KERNAL_SEGMENT_OFFSET + i);
        uint8_t physical_data = remap_data(info, updated_kernal_segment[i]);
        sram[physical_addr] = physical_data;
    }

    log("Reprogrammed %u bytes at offset 0x%04x", len, START_MODIFIED_KERNAL_SEGMENT_OFFSET);
}

// For debugging: demangle a captured physical address back to logical
static uint16_t __attribute__((unused)) demangle_addr(ora_log_fn_t log, const sdrr_info_t *info, uint16_t physical_addr) {
    uint16_t logical_addr = 0;
    for (int b = 0; b < 13; b++) {
        uint8_t bit_pos = info->pins->addr[b] - EYE_ADDR_BASE_PIN;
        if (physical_addr & (1u << bit_pos)) {
            logical_addr |= (1u << b);
        }
    }
    uint8_t x1_pos = info->pins->x1 - EYE_ADDR_BASE_PIN;
    if (physical_addr & (1u << x1_pos)) {
        log("X1=1");
    }
    uint8_t x2_pos = info->pins->x2 - EYE_ADDR_BASE_PIN;
    if (physical_addr & (1u << x2_pos)) {
        log("X2=1");
    }
    uint8_t cs1_pos = info->pins->cs1 - EYE_ADDR_BASE_PIN;
    if (physical_addr & (1u << cs1_pos)) {
        log("CS1=1");
    }
    return logical_addr;
}