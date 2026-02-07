// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

// RP2350 PIO/DMA autonomous ROM serving support

#include "include.h"

#if defined(RP235X)

#include "piodma/piodma.h"

// # Introduction
//
// This file contains a completely autonomous PIO and DMA based ROM serving
// implementation.  Once started, the PIO state machines and DMA channels
// serve ROM data in response to external chip select and address lines
// without any further CPU intervention.
//
// # Algorithm Summary
//
// The implementation uses three PIO state machines and 2 DMA channels, with
// the following overall operation:
// - PIO SM0 - Chip Select/Output Data Handler
// - PIO SM1 - Address Reader
// - DMA0    - Address Forwarder
// - DMA1    - Data Byte Fetcher
// - PIO SM2 - Data Byte Writer 
//
//     CS active   Data to Outputs                 CS Inactive  Data to Inputs
//             |   |                                         |  |
//             v   v                                         v  v
// SM0 ----------+-------------------------------------------------->
//     ^         |                                                  |
//     |         | (Optional IRQ0)                                  |
//     |         v                                                  |
//     |        SM1 ------> DMA0 --------> DMA1 -------> SM2        |
//     |         |            |             |             |         |
//     |         v            v             v             v         |
//     |     Read Addr  Forward Addr  Get Data Byte  Write Data     |
//     |  (Optional Loop)                                           |
//     |                                                            v
//     <-------------------------------------------------------------
//                                                   (Not to scale)
//
// # Timings
//
// It is difficult to be sure, but based on observed data, and theoretical
// estimates, the timings are estimated as follows:
// - Address valid to correct data byte is 11-14 cycles
// - Previous data valid after address change delay 14-11 cycles (although
//   it is much less than this is CS is made inactive, which is very likely)
// - CS active to data output is 5-6 cycles
// - CS inactive to data inputs is 3 cycles
//
// Physical settling time of lines will add to this.  Also, experience has
// shown that the system is likely to introduce other, unplanned for, stalls
// and other delays.  In particular if running _anything_ else, such as having
// an SWD debug probe connected, may introduce delays and jitter due to bus
// contention.
//
// At a max rated RP2350 clock speed of 150MHz this is:
// - 73-93ns from address to data
// - 33-40ns from CS active to data output
// - 20ns from CS inactive to data inputs
//
// At 50MHz:
// - 220-280ns from address to data
// - 280-220ns from previous data valid after address change
// - 100-120ns from CS active to data output
// - 60ns from CS inactive to data inputs
//
// Overclocked to 300MHz:
// - 37-47ns from address to data
// - 17-20ns from CS active to data output
// - 10ns from CS inactive to data inputs
//
// Address to data breakdown:
// - 2 cycle delay in GPIO state reaching PIO due to input-sync
// - SM1 address read 3-4 cycles:
//   - 3 is best case scenario
//   - 6 is worst case, but this "swallows" the input-sync delay, leading to 4
// - Triggering DMA via DREQ from SM1 RX FIFO 1 cycle
// - DMAs take 2-3 cycles each:
//   - 3 cycles is likely due to single cycle stall due to contention, likely
//     with other DMA channel.
//   - Assume no stall in transfer between them.
// - SM2 data byte output 1 cycle
//
// Previous data valid after address change breakdown:
// - Inverse of address to data breakdown
//
// CS active to data output breakdown:
// - 2 cycle delay in GPIO state reaching PIO due to input-sync
// - SM0 best case is 3 cycles - mov x, pins; jmp x--, N; mov pindirs, ~null
// - SM0 worst case adds 3 cycles, 2 of which "swallow" the input-sync delay
//
// CS active to inactive breakdown:
// - 2 cycle delay in GPIO state reaching PIO due to input-sync
// - SM0 best case is 3 cycles - mov x, pins; jmp !x, N; mov pindirs, null
// - SM0 worst case add 2 cycles, but these "swallow" the input-sync delay
//
// These timings do not quite add up.  The C64 character ROM is a 2332A, with
// 350ns access time - the maximum time allowed to go from address valid to
// valid.  As we can serve this ROM successfully at around 50MHz - with our
// worse cast estimate of 280ns for this time - either our estimates are wrong,
// or the C64 VIC-II requires better of the ROM than its specification - or
// both.  Worst case it seems like our estimates may be 20% under (i.e add 25%
// to them).
// 
// Therefore 50ns operation may require the RP2350 to be clocked closer to
// 400MHz than 300Mhz.  This is still likely to be within the RP2350's
// capabilities.
//
// # Detailed Operation
//
// The detailed operation is as follows:
//
// PIO0 SM0 - CS Handler
//  - (Initially ensures data pins are inputs.)
//  - Monitors the chip select lines.
//  - When all CS lines are active, optionally triggers an IRQ to signal the
//    address read SM to read the address lines.
//  - Sets the data pins to outputs after an optional delay.  The data lines
//    will not be serving the correct byte yet.
//  - Tight loops, checking for CS going inactive.
//  - When CS goes inactive again, sets data pins back to inputs and starts
//    over.
//
// PIO0 SM1 - Address Read
//  - (One time - reads high 16 bits of ROM table address from its TX FIFO.
//    This is preloaded to the TX FIFO by the CPU before starting the PIOs.)
//  - Prepares by pushing high 16 bits of ROM table address into its OSR.
//  - Optionally waits for IRQ from CS Handler SM.
//  - After optional delay (used in non-IRQ case), reads the address lines (16
//    bits) into OSR, completing the ROM table lookup address for the byte to
//    be served.
//  - Pushes the complete 32 bit ROM table lookup address into its RX FIFO 
//    (triggering DMA Channel 0).
//  - Loops back to 2nd step (pushing high 16 bits of ROM table address into
//    OSR).
//
// DMA Channel 0 - Address Forwarder
//  - Triggered by PIO0 SM1 RX FIFO using DREQ_PIO0_RX1 (SM1 RX FIFO).
//  - Reads the 32 bit ROM table lookup address from PIO0 SM1 RX FIFO.
//  - Writes the address into DMA Channel 1 READ_ADDR or READ_ADDR_TRIG
//    register.
//
// DMA Channel 1 - Data Byte Fetcher
//  - Triggered either DMA Channel 0 writing to this channels READ_ADDR_TRIG
//    or using DREQ_PIO0_RX1 (SM1 RX FIFO) - in which case this DMA is paced
//    identically to DMA Channel 0.
//  - Reads the ROM byte from the address specified in its READ_ADDR register.
//  - Writes the byte into PIO0 SM2 TX FIFO.
//  - Waits to be re-triggered by DMA Channel 0 writing to READ_ADDR_TRIG or
//    DREQ_PIO_RX1 (SM1 RX FIFO).
//
// PIO0 SM2 - Data Byte Output
//  - Waits for a data byte to become available in its TX FIFO.
//  - When data byte available, outputs the data byte on the data pins.
//  - Loops back to waiting for next data byte.
//
// There are a number of hardware pre-requisites for this to work correctly:
// - RP2350, not the RP2040.  This implementation uses:
//   - pinsdirs as a mov destination
//   - mov using pins as a source, only moving the configured "IN" pins.
//   Neither of these are supported by the RP2040's PIOs.
// - All Chip Select (or CE/OE) lines must be connected to contiguous GPIOs.
// - Any active high chip seledct lines must be inverted prior to use, by
//   using GPIO input inversion (INOVER).
// - All Data lines must be connected to contiguous GPIOs.
// - All Address lines must be connected to contiguous GPIOs, and be limited
//   to a 64KB address space.  (Strictly other powers of two could be
//   supported.)
//
// In order to minimise jitter, it is advisable to ensure the following:
// - The DMA channels have high AHB5 bus priority for both reads
//   and writes using the BUS_PRIORITY register.
// - Nothing else attempts to read or write to the 4 banks of SRAM the
//   64KB ROM table is striped across.
// - If other DMAs are enabled, the DMAs within this module should have a
//   higher priority set.
// - Nothing else accesses peripherals on the AHB5 splitter during operation.
//
// Possible enhancements:
// - May want to check CS is still active before setting data pins to outputs
//   in SM2.
//
// Note that a combined PIO/CPU implementation has also been explored (see
// PIO_CONFIG_NO_DMA).  This is discussed further below, but in summary, it
// matches DMA performance, while consuming a CPU core.
//
// # Supported PIO configuration options
//
// Note where min/max clock speeds are given below they tended to vary by
// 1-2Mhz, based on the day.  Likely due to temperature variations affecting
// the host's timing.  It is unlikely the RP2350's timing varies, given it
// has a modern, extremely accurate, clock source.
//
// For these tests, the RP2350 was not overclocked - the max supported clock
// speed it known to be higher than 150MHz for these ROMs, but there is a max
// speed, particularly for character ROMs, due to the video chip requiring a
// byte to be held after CS is dectivated.
//
// # PIO_CONFIG_DEFAULT
//
// - READ_IRQ = 1
// - ADDR_READ_DELAY = 0
//
// Here the IRQ from CS handler SM is used to trigger the address read SM.
// This works well serving a C64 charaxcter ROM at higher clock speeds
// (roughly 115-150MHz).
//
// Min/Max speeds:
// - PAL C64 Char ROM: 115-150MHz
// - PAL C64 Kernal ROM: 45-150MHz
// - PAL VIC-20 Char ROM: 44-150MHz
//
// # PIO_CONFIG_SLOW_CLOCK_KERNAL
//
// - READ_IRQ = 0
// - ADDR_READ_DELAY = 1
//
// Here 1 cycles is sufficient time to allow DMA chain to avoid backing up.
// However, the VIC-II requires a 2 cycle delay from the character ROM - see
// PIO_CONFIG_SLOW_CLOCK_CHAR.
//
// Min/Max speeds:
// - PAL C64 Kernal ROM: 41-150MHz
// - PAL VIC-20 Kernal ROM: 22-150MHz
//
// # PIO_CONFIG_SLOW_CLOCK_CHAR
//
// - READ_IRQ = 0
// - ADDR_READ_DELAY = 2
//
// Add an additional cycle of delay before reading address lines to allow the
// byte to remain on the bus slightly later, as seems to be required by a
// VIC-II chip of a character ROM
//
// Min/Max speeds:
// - PAL C64 Char ROM: 51-150MHz
// - PAL VIC-20 Char ROM: 51-150MHz

// Whether to use DMA (or instead, use the CPU to read bytes).  If set,
// ADDR_READ_IRQ is ignored.
//
// This option is not maintained any may be broken.  It was implemented to test
// which was faster - DMA or CPU.  It turns out to be identical performance -
// both serve a C64 character from down to 51MHz but no further without
// glitches.  Similarly, both serve a kernal down to 41MHz.
//
// Therefore the DMA approach has been selected as superior as it frees up the
// CPU for other applications.
//
// (Actually it is possible to implement an even more pathological assembly
// CPU loop which shaves the char ROM down to 50MHz, but it's likely fragile,
// breaking if the CPU loop ever takes an extra cycle, such as when a debug
// probe is connected.)
//
// #define PIO_CONFIG_NO_DMA  1

// Fallback default configuration
#if !defined(PIO_CONFIG_ADDR_READ_IRQ) && !defined(PIO_CONFIG_ADDR_READ_DELAY) && !defined(PIO_CONFIG_CS_TO_DATA_OUTPUT_DELAY) && !defined(PIO_CONFIG_CS_INACTIVE_DATA_HOLD_DELAY)
#if !defined(PIO_CONFIG_DEFAULT) && !defined(PIO_CONFIG_SLOW_CLOCK_KERNAL) && !defined(PIO_CONFIG_SLOW_CLOCK_CHAR) && !defined(PIO_CONFIG_NO_DMA)
#define PIO_CONFIG_SLOW_CLOCK_CHAR  1
#endif // !PIO_CONFIG_DEFAULT && !PIO_CONFIG_SLOW_CLOCK && !PIO_CONFIG_SLOW_CLOCK_CHAR
#endif // Fallback default

// Pre-defined PIO configuration options
#if defined(PIO_CONFIG_DEFAULT)
#define PIO_CONFIG_ADDR_READ_IRQ                1
#define PIO_CONFIG_ADDR_READ_DELAY              0
#define PIO_CONFIG_CS_TO_DATA_OUTPUT_DELAY      0
#define PIO_CONFIG_CS_INACTIVE_DATA_HOLD_DELAY  0
#elif defined(PIO_CONFIG_SLOW_CLOCK_KERNAL)
#define PIO_CONFIG_ADDR_READ_IRQ                0
#define PIO_CONFIG_ADDR_READ_DELAY              1
#define PIO_CONFIG_CS_TO_DATA_OUTPUT_DELAY      0
#define PIO_CONFIG_CS_INACTIVE_DATA_HOLD_DELAY  0
#elif defined(PIO_CONFIG_SLOW_CLOCK_CHAR)
#define PIO_CONFIG_ADDR_READ_IRQ                0
#define PIO_CONFIG_ADDR_READ_DELAY              2
#define PIO_CONFIG_CS_TO_DATA_OUTPUT_DELAY      0
#define PIO_CONFIG_CS_INACTIVE_DATA_HOLD_DELAY  0
#elif defined(PIO_CONFIG_NO_DMA)
#define PIO_CONFIG_ADDR_READ_IRQ                0
#define PIO_CONFIG_ADDR_READ_DELAY              1
#define PIO_CONFIG_CS_TO_DATA_OUTPUT_DELAY      0
#define PIO_CONFIG_CS_INACTIVE_DATA_HOLD_DELAY  0
#endif // PIO_CONFIG_DEFAULT

// Whether to use IRQ from CS handler to address read SM
#if !defined(PIO_CONFIG_ADDR_READ_IRQ)
#define PIO_CONFIG_ADDR_READ_IRQ  1
#endif // PIO_CONFIG_ADDR_READ_IRQ

// Whether to delay setting data pins to outputs at the start of the address
// read SM, after any optional IRQ, and, if so, by how many PIO cycles.
//
// Counter intuitively, this is useful to ensure the data remains valid longer,
// by delaying when it is actually read.  It is hard to add delays later in the
// chain, as the DMA transfers are tightly coupled to the PIO state machines.
//
// If PIO_CONFIG_ADDR_READ_IRQ=0 then this delay is essential to allow time for
// the DMA chain to process the address read before the next one.  So, set this
// to _at least_ 1 in that case.
//
// It may be that DMA Channel 0 requires only 2 cycles most of the time, but
// occassionally requires 3 (e.g. due to bus contention from the other DMA
// channel), because a C64 kernal _almost_ fully boots with both IRQ and this
// set to 0.  But not quite!
#if !defined(PIO_CONFIG_ADDR_READ_DELAY)
#define PIO_CONFIG_ADDR_READ_DELAY  0
#endif // PIO_CONFIG_ADDR_READ_DELAY

// Whether to delay setting data pins to outputs after CS goes active, and,
// if so, by how many PIO cycles.
//
// This may not be useful in practice, as ROM specifications tend to require
// that data become valid within a certain time after CS goes active - not
// that it _doesn't_ go active for a certain time.
#if !defined(PIO_CONFIG_CS_TO_DATA_OUTPUT_DELAY)
#define PIO_CONFIG_CS_TO_DATA_OUTPUT_DELAY  0
#endif // PIO_CONFIG_CS_TO_DATA_OUTPUT_DELAY

// Whether to hold data lines as outputs for a number of cycles after CS goes
// inactive, before setting them back to inputs, and, if so, by how many PIO
// cycles.
//
// This may not be useful in practice, as ROM specifications tend not to
// require a hold time after CS goes inactive.  (They do specify a hold time
// after address changes - see PIO_CONFIG_ADDR_READ_DELAY.)
#if !defined(PIO_CONFIG_CS_INACTIVE_DATA_HOLD_DELAY)
#define PIO_CONFIG_CS_INACTIVE_DATA_HOLD_DELAY  0
#endif // PIO_CONFIG_CS_INACTIVE_DATA_HOLD_DELAY

// IRQ to trigger ROM read by the CS Handler SM.
#define ROM_ADDR_READ_TRIGGER_IRQ  0

// Number of data and address lines
#define NUM_DATA_LINES    8
#define NUM_ADDR_LINES    16

// PIO ROM serving configuration structure
typedef struct piorom_config {
    // How many CS pins are used (1-3), and which ones to invert, as they are
    // active high.  This inversion is done in hardware before the PIOs read
    // the pins.
    uint8_t num_cs_pins;
    uint8_t invert_cs[3];

    // 4 bytes to here

    // Base CS pin.  Note that a single break in otherwise contiguous pins is
    // allows - see contiguous_cs_pins and cs_pin_2nd_match below.
    uint8_t cs_base_pin;

    // Base data pin.  Data pins must be contiguous.
    uint8_t data_base_pin;

    // Number of data pins (typically 8, but will be 16 as/when 40 pin ROMs
    // are supported).
    uint8_t num_data_pins;

    // Lowest address pin.  For 24 pin ROMs, this includes all CS and X pins.
    uint8_t addr_base_pin;

    // 8 bytes to here
    
    // Number of address pins.  This is 16 for a Fire 24 board - as
    // they include X and CS pins.  For a Fire 28 board is is also, normally,
    // 16 (as 2^16 is 512Kbits = 64KB), as CS lines are _not_ part of the
    // address space.  However, the 231024 is a 28 pin board and requires 17-18
    // pins, depending on layout, to allow the full 128KB to be addressed.
    uint8_t num_addr_pins;

    // Whether to use IRQ from CS handler to address read SM (0 = don't use)
    uint8_t addr_read_irq;

    // Number of PIO cycles to delay between address reads (in addition to any
    // delay from the instructions themselves)
    uint8_t addr_read_delay;

    // Number of cycles to wait after detecting CS going active before setting
    // data pins to outputs.
    uint8_t cs_active_delay;

    // 12 bytes to here

    // Number of cycles to wait after CS goes inactive before setting data
    // pins back to inputs.
    uint8_t cs_inactive_delay;

    // Whether to use DMA (0 = use)
    uint8_t no_dma;
    uint8_t pad[2];

    // 16 bytes to here

    // ROM table base address in RAM
    uint32_t rom_table_addr;

    // 20 bytes to here

    // PIO state machine 0 clock dividers
    uint16_t sm0_clkdiv_int;
    uint8_t sm0_clkdiv_frac;
    uint8_t pad0;
    
    // 24 bytes to here

    // PIO state machine 1 clock dividers
    uint16_t sm1_clkdiv_int;
    uint8_t sm1_clkdiv_frac;
    uint8_t pad1;

    // 28 bytes to here

    // PIO state machine 2 clock dividers
    uint16_t sm2_clkdiv_int;
    uint8_t sm2_clkdiv_frac;
    uint8_t pad2;

    // 32 bytes to here

    // The PIO CS algorithm supports up to a single break between otherwise
    // contiguous CS pins.  This is handled via a variant of the algorithm
    // which tests for both zero and another value ("2nd match").
    //
    // Consider CS lines ac, being arranged abc.  Here, CS lines are all
    // active if the read value is 000 or 010 - i.e. for both values of b.
    // In this case the "2nd match" value is 2.
    // 
    // The algorithm will hence check for 0 or for 2, and consider CS to be
    // active in either case.
    //
    // This algorithm is slightly less performant (one additional cycle in
    // some cases = 6.67ns), but in reality, the CS algorithm is so quick, it
    // is not likely to be the limiting factor, and hence is not expected to
    // have any impacts.
    //
    // While it might appear to be a PCB layout issue to have CS pins arranged
    // like this (and in some cases it might be), there are some differences in
    // the cs pin arrangements between different ROM types meaning this can be
    // useful.
    //
    // This approach only supports a single break in otherwise contiguous pins
    // and only 1 pin being within the break.
    uint8_t contiguous_cs_pins;

    // Whether multi-ROM mode is enabled (i.e. more than one ROM is being
    // served via the X pins).
    uint8_t multi_rom_mode;

    uint8_t pad3[2];

    // 36 bytes to here

    // See `contiguous_cs_pins` above.
    uint32_t cs_pin_2nd_match;

    // 40 bytes to here
} piorom_config_t;


// SM0 - CS Handler
//
// The program is constructed dynamically in pio_load_programs().  The overall
// algorithm is as follows:
//
// .wrap_target                      ; Start of CS loop
// 0xa063, //  mov    pindirs, null  ; set data pins to inputs
// 0xa020, //  mov    x, pins        ; read CS lines
// 0x0041, //  jmp    x--, 1         ; CS inactive, loop back to re-read CS
//                                   ; Note the decrement of x is unused -
//                                   ; but there is no jmp x instruction
// 0xc000, //  irq    set 0 [N]      ; OPTIONAL: signal CS active to address
//                                   ; read SM
//                                   ; OPTIONAL: N cycle delay before setting
//                                   ; data pins to outputs
// 0xaN42, //  nop    [N]            ; OPTIONAL: N cycle delay before setting
//                                  ROM_ADDR_READ_TRIGGER_IRQ ; data pins to outputs (if not on irq)
// 0xa06b, //  mov    pindirs, ~null ; set data pins to output
// 0xa020, //  mov    x, pins        ; read CS lines again
// 0x002Y, //  jmp    !x, Y [N]      ; CS still active, if so jump back one
//                                   ; instruction.
// 0xaN42, //  nop    [N]            ; OPTIONAL: N cycle delay before setting
//                                   ; data pins to inputs
// .wrap                             ; End of CS loop 

// There is an alternate version to handle non-contiguous CS pins:
//
// set Y, 2nd_match_value
//
// inactive:
// mov pindirs, null
//
// test_if_active:
// mov x, pins                  ; Load pins to X
// jmp !x active                ; CS = 000 Go active, could add single cycle wait to take the same time as if CS = 010
// jmp x!=y test_if_active      ; CS != 010 Check again
// ; CS = 010, so drop into active
//
// active:
// mov pindirs, ~null
//
// .wrap_target: 
// test_if_inactive:
// mov x, pins                  ; Load pins to X
// jmp !x test_if_inactive      ; CS == 000 Stay active, test again
// jmp x!=y inactive            ; CS != 010 So, go inactive
// .wrap                        ; CS = 010, so test again 

// SM1 - Address Read
//
// The program is constructed dynamically in pio_load_programs().  The overall
// algorithm is as follows:
//
// ; One time setup - get high word of ROM table address from TX FIFO.  This
// ; is 0x2001 as of v0.5.5, changed to 0x2000 as of v0.5.10.
// pull   block         ; get high word of ROM table address
// mov    x, osr        ; store high word in X
//
// .wrap_target         ; Start of address read loop
// in     x, 16         ; read high address bits from X
// wait   1 irq, 0 [N]  ; OPTIONAL: wait for CS to go active (and clears IRQ)
//                      ; OPTIONAL: N cycle delay after IRQ before reading
//                      ; address
// in     pins, 16      ; read address lines (autopush)
// .wrap                ; End of address read loop

// SM2 - Data Byte Output
//
// The program is constructed dynamically in pio_load_programs().  The overall
// algorithm is as follows:
//
// .wrap_target
// out    pins, 8       ; Auto-pulls byte from TX FIFO (from DMA Channel 1)
//                      ; and outputs on data pins
// .wrap

// Build and load the PIO programs for ROM serving
//
// Uses the single-pass PIO assembler macros from pioasm.h
static void piorom_load_programs(piorom_config_t *config) {
    // Get the high X bits of the RAM table address for preloading into the
    // address reader SM.
    uint8_t rom_table_num_addr_bits = 32 - config->num_addr_pins;
    uint32_t high_bits_mask = (1 << rom_table_num_addr_bits) - 1;
    uint32_t low_bits_mask = (1 << config->num_addr_pins) - 1;
    uint32_t __attribute__((unused)) alignment_size = (1 << config->num_addr_pins) / 1024;
    DEBUG("Checking RAM table address 0x%08X is %uKB aligned", config->rom_table_addr, alignment_size);
    DEBUG("High bits mask: 0x%08X, low bits mask: 0x%08X", high_bits_mask, low_bits_mask);
    if (config->rom_table_addr & low_bits_mask) {
        ERR("PIO ROM serving requires ROM table address to be %uKB aligned",
            alignment_size);
        limp_mode(LIMP_MODE_INVALID_CONFIG);
    }
    uint32_t rom_table_high_bits = (config->rom_table_addr >> config->num_addr_pins) & high_bits_mask;
    DEBUG("ROM table high %d bits: 0x%08X", rom_table_num_addr_bits, rom_table_high_bits);

#if defined(DEBUG_LOGGING)
    // Log other config values
    uint8_t num_cs_pins = config->num_cs_pins;
    uint8_t cs_base_pin = config->cs_base_pin;
    uint8_t num_data_pins = config->num_data_pins;
    uint8_t data_base_pin = config->data_base_pin;
    uint8_t num_addr_pins = config->num_addr_pins;
    uint8_t addr_base_pin = config->addr_base_pin;
    uint8_t addr_read_irq = config->addr_read_irq;
    uint8_t addr_read_delay = config->addr_read_delay;
    uint8_t cs_active_delay = config->cs_active_delay;
    uint8_t no_dma = config->no_dma;
    uint16_t cs_handler_clkdiv_int = config->sm0_clkdiv_int;
    uint8_t cs_handler_clkdiv_frac = config->sm0_clkdiv_frac;
    uint16_t addr_reader_clkdiv_int = config->sm1_clkdiv_int;
    uint8_t addr_reader_clkdiv_frac = config->sm1_clkdiv_frac;
    uint16_t data_writer_clkdiv_int = config->sm2_clkdiv_int;
    uint8_t data_writer_clkdiv_frac = config->sm2_clkdiv_frac;
    uint8_t contiguous_cs_pins = config->contiguous_cs_pins;
    uint8_t multi_rom_mode = config->multi_rom_mode;
    uint32_t cs_pin_2nd_match = config->cs_pin_2nd_match;
    DEBUG("PIO ROM Serving Config:");
    DEBUG("- CS pins: %d-%d", cs_base_pin, cs_base_pin + num_cs_pins - 1);
    DEBUG("- CS invert: %s %s %s",
        (config->invert_cs[0] ? "Y" : "N"),
        (config->invert_cs[1] ? "Y" : "N"),
        (config->invert_cs[2] ? "Y" : "N"));
    DEBUG("- Contiguous CS pins: %s, 0x%02X", (contiguous_cs_pins ? "Y" : "N"), cs_pin_2nd_match);
    DEBUG("- Multi-ROM mode: %s", (multi_rom_mode ? "Y" : "N"));
    DEBUG("- Data pins: %d-%d", data_base_pin, data_base_pin + num_data_pins - 1);
    DEBUG("- Address pins: %d-%d", addr_base_pin, addr_base_pin + num_addr_pins - 1);
    DEBUG("- CS Handler CLKDIV: %d.%02d", cs_handler_clkdiv_int, cs_handler_clkdiv_frac);
    DEBUG("- Addr Reader CLKDIV: %d.%02d", addr_reader_clkdiv_int, addr_reader_clkdiv_frac);
    DEBUG("- Data Writer CLKDIV: %d.%02d", data_writer_clkdiv_int, data_writer_clkdiv_frac);
    DEBUG("- PIO algorithm config:");
    DEBUG("  - Addr read IRQ: %s, DMA: %s", (addr_read_irq ? "Y" : "N"), (no_dma ? "N" : "Y"));
    DEBUG("  - Addr read delay: %u, CS active delay: %u", addr_read_delay, cs_active_delay);
    DEBUG("  - CS active to data output delay: %u", cs_active_delay);
#endif // DEBUG_LOGGING

    // Set up the PIO assembler
    PIO_ASM_INIT();
    
    // Clear all PIO IRQs
    PIO_CLEAR_ALL_IRQS();

    // PIO0 Programs
    //
    // All ROM serving handlers
    PIO_SET_BLOCK(0);

    // SM0 - CS handler
    //
    // Handles detecting CS active/inactive and setting data pins to
    // outputs/inputs accordingly.  Also triggers address read SM via IRQ if
    // configured to do so.
    PIO_SET_SM(0);

    if (config->contiguous_cs_pins) {
        // "Normal" case - all CS pins contiguous
        PIO_ADD_INSTR(MOV_PINDIRS_NULL);

        PIO_LABEL_NEW(load_cs);
        PIO_ADD_INSTR(MOV_X_PINS);
        if (!config->multi_rom_mode) {
            PIO_ADD_INSTR(JMP_X_DEC(PIO_LABEL(load_cs)));
        } else {
            PIO_ADD_INSTR(JMP_NOT_X(PIO_LABEL(load_cs)));
        }
        if (config->addr_read_irq) {
            if (!config->cs_active_delay) {
                PIO_ADD_INSTR(IRQ_SET(ROM_ADDR_READ_TRIGGER_IRQ));
            } else {
                PIO_ADD_INSTR(ADD_DELAY(IRQ_SET(ROM_ADDR_READ_TRIGGER_IRQ), config->cs_active_delay));
            }
        } else {
            if (config->cs_active_delay) {
                PIO_ADD_INSTR(ADD_DELAY(NOP, (config->cs_active_delay - 1)));
            }
        }
        PIO_ADD_INSTR(MOV_PINDIRS_NOT_NULL);
        PIO_LABEL_NEW(check_cs_gone_inactive)
        PIO_ADD_INSTR(MOV_X_PINS);
        PIO_WRAP_TOP();
        if (!config->multi_rom_mode) {
            PIO_ADD_INSTR(JMP_NOT_X(PIO_LABEL(check_cs_gone_inactive)));
        } else {
            PIO_ADD_INSTR(JMP_X_DEC(PIO_LABEL(check_cs_gone_inactive)));
        }
        if (config->cs_inactive_delay) {
            PIO_WRAP_TOP();
            PIO_ADD_INSTR(ADD_DELAY(NOP, (config->cs_inactive_delay - 1)));
        }
    } else {
        // Non-contiguous CS pins - need to check for 2 different possible
        // CS values
        PIO_ADD_INSTR(SET_Y(config->cs_pin_2nd_match));
        
        // inactive:
        PIO_LABEL_NEW(inactive_offset);
        PIO_ADD_INSTR(MOV_PINDIRS_NULL);

        // test_if_active:
        PIO_LABEL_NEW(test_if_active_offset);
        PIO_ADD_INSTR(MOV_X_PINS);

        PIO_LABEL_NEW_OFFSET(active_offset, 2);
        PIO_ADD_INSTR(JMP_NOT_X(PIO_LABEL(active_offset)));
        PIO_ADD_INSTR(JMP_X_NOT_Y(PIO_LABEL(test_if_active_offset)));

        // active:
        if (config->addr_read_irq) {
            if (!config->cs_active_delay) {
                PIO_ADD_INSTR(IRQ_SET(ROM_ADDR_READ_TRIGGER_IRQ));
            } else {
                PIO_ADD_INSTR(ADD_DELAY(IRQ_SET(ROM_ADDR_READ_TRIGGER_IRQ), config->cs_active_delay));
            }
        } else {
            if (config->cs_active_delay) {
                PIO_ADD_INSTR(ADD_DELAY(NOP, (config->cs_active_delay - 1)));
            }
        }
        PIO_ADD_INSTR(MOV_PINDIRS_NOT_NULL);

        // .wrap_target:
        // test_if_inactive:
        PIO_WRAP_BOTTOM();
        PIO_LABEL_NEW(test_if_inactive_offset);
        PIO_ADD_INSTR(MOV_X_PINS);
        PIO_ADD_INSTR(JMP_NOT_X(PIO_LABEL(test_if_inactive_offset)));
        PIO_WRAP_TOP();
        PIO_ADD_INSTR(JMP_X_NOT_Y(PIO_LABEL(inactive_offset)));
        if (config->cs_inactive_delay) {
            PIO_WRAP_TOP();
        }
    }

    // Configure the CS handler SM
    PIO_SM_CLKDIV_SET(
        config->sm0_clkdiv_int,
        config->sm0_clkdiv_frac
    );
    PIO_SM_EXECCTRL_SET(0);
    PIO_SM_SHIFTCTRL_SET(
        PIO_IN_COUNT(config->num_cs_pins) |
        PIO_IN_SHIFTDIR_L           // Direction left important for non-
                                    // contiguous CS pin handling
    );
    PIO_SM_PINCTRL_SET(
        PIO_OUT_COUNT(config->num_data_pins) |
        PIO_OUT_BASE(config->data_base_pin) |
        PIO_IN_BASE(config->cs_base_pin)
    );

    // Jump to start and log
    PIO_SM_JMP_TO_START();
    PIO_LOG_SM("CS Handler");

    // SM1 - Address reader
    //
    // Reads address lines and pushes complete ROM table lookup address to the
    // DMA chain.
    PIO_SET_SM(1);

    // The ADDR_READ_DELAY gets added either to the IRQ (if it exists) or the
    // IN instruction (if no IRQ).  In the no IRQ case it is not important on
    // which instruction we add the delay, as it doesn't affect how "old" the
    // address will be went sent to the DMA, just how _frequently_ it is read.
    if (!config->addr_read_irq && config->addr_read_delay) {
        PIO_ADD_INSTR(ADD_DELAY(IN_X(rom_table_num_addr_bits), config->addr_read_delay));
    } else {
        PIO_ADD_INSTR(IN_X(rom_table_num_addr_bits));
    }
    if (config->addr_read_irq || config->no_dma) {
        if (!config->addr_read_delay) {
            PIO_ADD_INSTR(WAIT_IRQ_HIGH(ROM_ADDR_READ_TRIGGER_IRQ));
        } else {
            PIO_ADD_INSTR(ADD_DELAY(WAIT_IRQ_HIGH(ROM_ADDR_READ_TRIGGER_IRQ), config->addr_read_delay));
        }
    }
    PIO_WRAP_TOP();
    PIO_ADD_INSTR(IN_PINS(config->num_addr_pins));

    // Configure the address read SM
    PIO_SM_CLKDIV_SET(
        config->sm1_clkdiv_int,
        config->sm1_clkdiv_frac
    );
    PIO_SM_EXECCTRL_SET(0);
    PIO_SM_SHIFTCTRL_SET(
        PIO_IN_COUNT(config->num_addr_pins) |   // Reading the address pins (unused
                                                // as this is for mov instructions)
        PIO_AUTOPUSH |                          // Auto push when we hit threshold
        PIO_PUSH_THRESH(32) |                   // Push when we have 32 bits (from
                                                // X and from address pins)
        PIO_IN_SHIFTDIR_L |     // Shift left, so address lines are in low bits
        PIO_OUT_SHIFTDIR_L      // Direction doesn't matter, as we push 32 bits
    );
    PIO_SM_PINCTRL_SET(
        PIO_IN_BASE(config->addr_base_pin)
    );

    // Preload the ROM table address into the X register
    PIO_TXF = rom_table_high_bits;
    PIO_SM_EXEC_INSTR(PULL_BLOCK);  // Pull it into OSR
    PIO_SM_EXEC_INSTR(MOV_X_OSR);   // Store it in X

    // Jump to start and log
    PIO_SM_JMP_TO_START();
    PIO_LOG_SM("Address Reader");

    // SM2 - Data byte output
    //
    // Outputs a data byte received from the DMA chain on the data pins.
    PIO_SET_SM(2);

    // Load the data byte output program
    PIO_ADD_INSTR(OUT_PINS(config->num_data_pins));

    // Configure the data byte SM
    PIO_SM_CLKDIV_SET(
        config->sm2_clkdiv_int,
        config->sm2_clkdiv_frac
    );
    PIO_SM_EXECCTRL_SET(0);
    PIO_SM_SHIFTCTRL_SET(
        PIO_OUT_SHIFTDIR_R |                    // Writes LSB of OSR
        PIO_AUTOPULL |                          // Auto pull when we hit threshold
        PIO_PULL_THRESH(config->num_data_pins)  // Pull when we have 8 bits
    );
    PIO_SM_PINCTRL_SET(
        PIO_OUT_BASE(config->data_base_pin) |
        PIO_OUT_COUNT(config->num_data_pins)
    );

    // Jump to start and log
    PIO_SM_JMP_TO_START();
    PIO_LOG_SM("Data Byte Output");

    //
    // PIO 0 - End of block
    //
    PIO_END_BLOCK();
}

// Starts the PIO state machines for ROM serving.
static void piorom_start_pios() {
    PIO0_CTRL_SM_ENABLE(0x7); // Enable SM0, SM1 and SM2
}

// Set GPIOs to PIO function for ROM serving
static void piorom_set_gpio_func(piorom_config_t *config) {
    uint8_t num_cs_pins = config->num_cs_pins;
    uint8_t cs_base_pin = config->cs_base_pin;
    uint8_t *cs_pin_invert = config->invert_cs;
    uint8_t data_base_pin = config->data_base_pin;
    uint8_t addr_base_pin = config->addr_base_pin;

    // Data pins
    for (int ii = data_base_pin;
        ii < (data_base_pin + NUM_DATA_LINES);
        ii++) {
        GPIO_CTRL(ii) = GPIO_CTRL_FUNC_PIO0;
    }

    // Address pins
    for (int ii = addr_base_pin;
        ii < (addr_base_pin + NUM_ADDR_LINES);
        ii++) {
        GPIO_CTRL(ii) = GPIO_CTRL_FUNC_PIO0;
    }

    // CS pins
    //
    // We MUST set these after the address pins, as the CS pins may be part of
    // the address pin range (they are on a 24 pin ROM).
    for (int ii = 0; ii < num_cs_pins; ii++) {
        uint8_t pin = cs_base_pin + ii;
        uint8_t invert = cs_pin_invert[ii];
        // Set to PIO function - this clears everything else.
        GPIO_CTRL(pin) = GPIO_CTRL_FUNC_PIO0;
        if (!invert) {
            DEBUG("  CS pin %d active low CTRL=0x%08X", pin, GPIO_CTRL(pin));
        } else {
            // Turn CS line into active low by inverting the GPIO before the
            // PIO reads it
            GPIO_CTRL(pin) |= GPIO_CTRL_INOVER_INVERT;
            DEBUG("  CS pin %d active high CTRL=0x%08X", pin, GPIO_CTRL(pin));
        }
    }
}

// Setup the DMA channels for ROM serving
static void piorom_setup_dma(
    piorom_config_t *config,
    uint8_t pio_block,
    uint8_t sm_addr_read,
    uint8_t sm_data_byte
) {
    volatile dma_ch_reg_t *dma_reg;

    // DMA Channel 0 - Receives ROM table lookup address from PIO0 SM1 and
    // sends it onto DMA Channel 1.  Paced by PIO0 SM1 RX FIFO DREQ.
    dma_reg = DMA_CH_REG(0);
    dma_reg->read_addr = (uint32_t)&PIO0_SM_RXF(sm_addr_read);
    if (config->addr_read_irq) {
        // When address read is triggerd by IRQ, we only want a single
        // transfer per IRQ.  We need to trigger channel 1 manually.
        dma_reg->write_addr = (uint32_t)&DMA_CH_READ_ADDR_TRIG(1);
        dma_reg->transfer_count = 1;
    } else {
        // When address read is not triggered by IRQ, we want continuous
        // transfers to channel 1.  No triggering is necessary, as channel 1
        // will be paced by the PIO0 SM1 RX FIFO DREQ, like this channel.
        dma_reg->write_addr = (uint32_t)&DMA_CH_READ_ADDR(1);
        dma_reg->transfer_count = 0xffffffff;
    }
    dma_reg->ctrl_trig =
        DMA_CTRL_TRIG_TREQ_SEL(DREQ_PIO_X_SM_Y_RX(pio_block, sm_addr_read)) |
        DMA_CTRL_TRIG_EN |
        DMA_CTRL_TRIG_DATA_SIZE_32BIT;

    // DMA Channel 1 - Reads ROM data from memory and sends to PIO0 SM2.
    // Also paced by PIO0 SM1 RX FIF DREQ, so runs in lock-step with channel
    // 0.
    // Pre-load the READ_ADDR register with the first byte of the ROM table.
    // This byte will never actually get served, as the data lines will be
    // inputs, but it's more valid than setting to 0.
    dma_reg = DMA_CH_REG(1);
    dma_reg->read_addr = config->rom_table_addr;
    dma_reg->write_addr = (uint32_t)&PIO0_SM_TXF(sm_data_byte);
    uint32_t ctrl_trig = 
        DMA_CTRL_TRIG_EN |
        DMA_CTRL_TRIG_DATA_SIZE_8BIT;
    if (config->addr_read_irq) {
        // When address read is triggerd by IRQ, we only want a single
        // transfer per IRQ.  We need to re-trigger channel 1 manually.
        dma_reg->transfer_count = 1;
        ctrl_trig |= DMA_CTRL_TRIG_TREQ_SEL(DMA_CTRL_TRIG_TREQ_PERM);
    } else {
        // When address read is not triggered by IRQ, we want continuous
        // transfers.
        dma_reg->transfer_count = 0xffffffff;
        ctrl_trig |= DMA_CTRL_TRIG_TREQ_SEL(DREQ_PIO_X_SM_Y_RX(pio_block, sm_addr_read));
    }
    dma_reg->ctrl_trig = ctrl_trig;

    // Set DMA Read as high priority on the AHB5 bus for both:
    // - Reads (from RAM and PIO RX FIFO)
    // - Writes (to PIO TX FIFO and DMA READ_ADDR)
    BUSCTRL_BUS_PRIORITY |=
        BUSCTRL_BUS_PRIORITY_DMA_R_BIT |
        BUSCTRL_BUS_PRIORITY_DMA_W_BIT;
}

// Get lowest data GPIO from the pin info
static uint8_t get_lowest_data_gpio(
    const sdrr_info_t *info
) {
    uint8_t lowest = MAX_USED_GPIOS;
    for (int ii = 0; ii < 8; ii++) {
        if (info->pins->data[ii] < lowest) {
            lowest = info->pins->data[ii];
        }
    }
    return lowest;
}

// Get lowest address GPIO from the pin info.
//
// For 24 pin ROMs this includes CS lines and X pins.
// For 28 pin ROMs this include CS lines.
static uint8_t get_lowest_addr_gpio(
    const sdrr_info_t *info,
    uint32_t img_size,
    const uint8_t cs_base_pin
) {
    uint8_t lowest = MAX_USED_GPIOS;
    uint8_t chip_pins = info->pins->chip_pins;

    for (int ii = 0; ii < 16; ii++) {
        if (info->pins->addr[ii] < lowest) {
            lowest = info->pins->addr[ii];
        }
    }

    if (chip_pins != 24) {
        // For 28 pin ROMs, we check the image size to decide how many address
        // pins to consider, not the ROM type.  This is because some version of
        // Studio uses 256KB images for <231024 ROMS.  However, since 0.6.4,
        // only a 231024 _needs_ a 256KB image.
        if (img_size > (64*1024)) {
            // Consider addr2 pins, but only when serving a > 64KB image, as
            // for 28 pin ROMs, these are only used in this case.
            for (int ii = 0; ii < 8; ii++) {
                if (info->pins->addr2[ii] < lowest) {
                    lowest = info->pins->addr2[ii];
                }
            }
        }
    }

    if (chip_pins == 24) {
        // Consider X pins
        if (info->pins->x1 < lowest) {
            lowest = info->pins->x1;
        }
        if (info->pins->x2 < lowest) {
            lowest = info->pins->x2;
        }
    }

    if ((chip_pins == 24) ||
        ((chip_pins == 28) && (img_size > (64*1024)))) {
        // Consider CS pins - only need to check the base as this will be the
        // lowest.
        //
        // 24 pin One ROMs always include CS pins.  However, 28 pins only
        // include them if the image size is 256KB.  check_config() checks
        // that image size is either 64KB or 256Kb so the > test here is
        // sufficient.
        if (cs_base_pin < lowest) {
            lowest = cs_base_pin;
        }
    }

    return lowest;
}

// Handle non-contiguous CS pins - changes configuration so that a different
// CS PIO algorithm is used.
//
// Args:
// - config: PIO ROM serving configuration
// - num_cs_pins: Number of CS pins originally detected
// - lowest_cs: Lowest CS pin number
// - low_cs: Highest of the bottom set of contiguous CS pins
// - high_cs: Lowest of the top set of contiguous CS pins
static void piorom_handle_non_contiguous_cs_pins(
    piorom_config_t *config,
    uint8_t num_cs_pins,
    uint8_t lowest_cs,
    uint8_t low_cs,
    uint8_t high_cs
) {
    DEBUG("Handle non-contig pins num_cs_pins=%d lowest_cs=%d low_cs=%d high_cs=%d",
        num_cs_pins,
        lowest_cs,
        low_cs,
        high_cs
    );
    if (!config->contiguous_cs_pins) {
        ERR("Multiple non-contiguous CS pin ranges not supported");
        limp_mode(LIMP_MODE_INVALID_CONFIG);
        return;
    }

    // We have non-contiguous CS pins.  Only support a single pin break.
    if ((high_cs - low_cs) != 2) {
        ERR("Non-contiguous CS pins with break of more than 1 pin not supported");
        limp_mode(LIMP_MODE_INVALID_CONFIG);
        return;
    }

    config->contiguous_cs_pins = 0;
    config->num_cs_pins = num_cs_pins+1;
    config->cs_pin_2nd_match = 1 << (low_cs - lowest_cs + 1);
}

// Construct the PIO ROM serving configuration from the SDRR and ROM set info
static void piorom_finish_config(
    piorom_config_t *config,
    const sdrr_info_t *info,
    const sdrr_rom_set_t *set,
    uint32_t rom_table_addr
) {
    // Figure out number of CS pins from ROM type
    const sdrr_rom_info_t *rom = set->roms[0];
    switch (rom->rom_type) {
        case CHIP_TYPE_2364:
            if (set->serve != SERVE_ADDR_ON_ANY_CS) {
                config->num_cs_pins = 1;
            } else {
                if ((set->rom_count < 2) || (set->rom_count > 3)) {
                    ERR("PIO ROM serving invalid multi-ROM count %d for 2364",
                        set->rom_count);
                    limp_mode(LIMP_MODE_INVALID_CONFIG);
                    config->num_cs_pins = 1;
                } else {
                    config->num_cs_pins = set->rom_count;
                    config->multi_rom_mode = 1;
                }
            }
            break;

        case CHIP_TYPE_2332:
        case CHIP_TYPE_23256:
        case CHIP_TYPE_23512:
            config->num_cs_pins = 2;
            break;

        case CHIP_TYPE_2316:
        case CHIP_TYPE_23128:
            config->num_cs_pins = 3;
            break;

        case CHIP_TYPE_2716:
        case CHIP_TYPE_2732:
        case CHIP_TYPE_2764:
        case CHIP_TYPE_27128:
        case CHIP_TYPE_27256:
        case CHIP_TYPE_27512:
            config->num_cs_pins = 2;
            break;

        case CHIP_TYPE_231024:
            config->num_cs_pins = 1;
            break;

        default:
            ERR("PIO ROM serving invalid ROM type %d", rom->rom_type);
            limp_mode(LIMP_MODE_INVALID_CONFIG);
            config->num_cs_pins = 1;
            break;
    }

    // Figure out CS pin base
    uint8_t series_23 = 0;
    switch (rom->rom_type) {
        // 23 series ROMs - use CS lines
        case CHIP_TYPE_2364:
            // Special case for handling multi-ROM serving
            if (config->multi_rom_mode) {
                // For 2 ROMs, use CS and X1.  For 3 ROMs use CS, X1 and X2.
                // The base pin is the lowest of these.
                series_23 = 1;
                uint8_t lowest = info->pins->cs1;
                if (info->pins->x1 < lowest) {
                    lowest = info->pins->x1;
                }
                if (config->num_cs_pins == 3) {
                    if (info->pins->x2 < lowest) {
                        lowest = info->pins->x2;
                    }
                }
                config->cs_base_pin = lowest;

                // For now, insist on contiguity (it may be possible to lift
                // this restriction)
                if ((info->pins->x1 > (info->pins->cs1 + 1)) ||
                    (info->pins->x1 < (info->pins->cs1 - 1))) {
                    ERR("PIO ROM serving non-contiguous CS/X1 pins not supported");
                    limp_mode(LIMP_MODE_INVALID_CONFIG);
                }
                if (config->num_cs_pins == 3) {
                    if ((info->pins->x2 > (info->pins->x1 + 1)) ||
                        (info->pins->x2 < (info->pins->x1 - 1))) {
                        ERR("PIO ROM serving non-contiguous CS/X1/X2 pins not supported");
                        limp_mode(LIMP_MODE_INVALID_CONFIG);
                    }
                }
                break;
            }
            // GCC notices the following comment and allows compilation
            // fall through 
        case CHIP_TYPE_2316:
        case CHIP_TYPE_2332:
        case CHIP_TYPE_23128:
        case CHIP_TYPE_23256:
        case CHIP_TYPE_23512:
        case CHIP_TYPE_231024:
            series_23 = 1;
            // Figure out base CS pin from SDRR info

            // Store off num_cs_pins as it gets modified by
            // piorom_handle_non_contiguous_cs_pins()
            uint8_t num_cs_pins = config->num_cs_pins;
            if (num_cs_pins == 1) {
                config->cs_base_pin = info->pins->cs1;
            } else {
                uint8_t next_pin;
                if (num_cs_pins > 2) {
                    next_pin = info->pins->cs3;
                } else {
                    next_pin = info->pins->cs2;
                }
                if (info->pins->cs1 < next_pin) {
                    if (next_pin > (info->pins->cs1 + 1)) {
                        piorom_handle_non_contiguous_cs_pins(
                            config,
                            num_cs_pins,
                            info->pins->cs1,
                            info->pins->cs1,
                            next_pin
                        );
                    }
                    config->cs_base_pin = info->pins->cs1;
                } else {
                    if (info->pins->cs1 > (next_pin + 1)) {
                        piorom_handle_non_contiguous_cs_pins(
                            config,
                            num_cs_pins,
                            next_pin,
                            next_pin,
                            info->pins->cs1
                        );
                    }
                    config->cs_base_pin = next_pin;
                }

                if (num_cs_pins > 2) {
                    uint8_t final_pin = info->pins->cs2;

                    // piorom_handle_non_contiguous_cs_pins() handles if there
                    // are already too many breaks in contiguity
                    if (final_pin == (config->cs_base_pin - 1)) {
                        config->cs_base_pin = final_pin;
                    } else if (final_pin == (config->cs_base_pin + 2)) {
                        // cs_base_pin is already correct
                    } else if (final_pin > (config->cs_base_pin + 2)) {
                        piorom_handle_non_contiguous_cs_pins(
                            config,
                            num_cs_pins,
                            config->cs_base_pin,
                            config->cs_base_pin+1,
                            final_pin
                        );
                        // cs_base_pin is already correct
                    } else {
                        // cs3 is less than cs_base_pin - 1
                        piorom_handle_non_contiguous_cs_pins(
                            config,
                            num_cs_pins,
                            final_pin,
                            final_pin,
                            config->cs_base_pin
                        );
                        config->cs_base_pin = final_pin;
                    }
                }
            }
            break;

        // 27 series ROMs - use OE/CE lines
        case CHIP_TYPE_2716:
        case CHIP_TYPE_2732:
        case CHIP_TYPE_2764:
        case CHIP_TYPE_27128:
        case CHIP_TYPE_27256:
        case CHIP_TYPE_27512:
            // Use OE/CE instead of CS pins
            config->cs_base_pin = info->pins->oe;
            if (info->pins->ce == (config->cs_base_pin + 1)) {
                // OK
            } else if (info->pins->ce == (config->cs_base_pin - 1)) {
                config->cs_base_pin = info->pins->ce;
            } else if (info->pins->ce > (config->cs_base_pin + 1)) {
                piorom_handle_non_contiguous_cs_pins(
                    config,
                    config->num_cs_pins,
                    config->cs_base_pin,
                    config->cs_base_pin,
                    info->pins->ce
                );
            } else {
                // ce is less than oe
                piorom_handle_non_contiguous_cs_pins(
                    config,
                    config->num_cs_pins,
                    info->pins->ce,
                    info->pins->ce,
                    config->cs_base_pin
                );
                config->cs_base_pin = info->pins->ce;
            }
            break;

        default:
            ERR("PIO ROM serving invalid ROM type %d", rom->rom_type);
            limp_mode(LIMP_MODE_INVALID_CONFIG);
            config->num_cs_pins = 1;
            break;
    }

    // Find any CS lines which need to be inverted.  Make sure to make CS
    // lines against the pin numbers - the lower pin number is first, whether
    // that is CS1 or CS2 (or CS3).
    //
    // This isn't required for 27 series ROMs, as both OE and CE are active
    // low.
    //
    // Where non-contiguous CS pins are used, we may check non CS pins here.
    // That's OK as they won't match an actual CS pin.
    if (series_23) {
        if (!config->multi_rom_mode) {
            for (int ii = 0; (ii < config->num_cs_pins) && (ii < 3); ii++) {
                if (info->pins->cs1 == (config->cs_base_pin + ii)) {
                    if (rom->cs1_state == CS_ACTIVE_HIGH) {
                        config->invert_cs[ii] = 1;
                    } else {
                        config->invert_cs[ii] = 0;
                    }
                } else if (info->pins->cs2 == (config->cs_base_pin + ii)) {
                    if (rom->cs2_state == CS_ACTIVE_HIGH) {
                        config->invert_cs[ii] = 1;
                    } else {
                        config->invert_cs[ii] = 0;
                    }
                } else if (info->pins->cs3 == (config->cs_base_pin + ii)) {
                    if (rom->cs3_state == CS_ACTIVE_HIGH) {
                        config->invert_cs[ii] = 1;
                    } else {
                        config->invert_cs[ii] = 0;
                    }
                }
            }
        } else {
            // In multi-ROM mode, CS1, X1 and potentiall X2 are CS lines.
            // Also, invert logic is reversed compared to the normal case, as
            // _any_ CS line active is supported.
            for (int ii = 0; (ii < config->num_cs_pins) && (ii < 3); ii++) {
                if (info->pins->cs1 == (config->cs_base_pin + ii)) {
                    if (rom->cs1_state == CS_ACTIVE_LOW) {
                        config->invert_cs[ii] = 1;
                    } else {
                        config->invert_cs[ii] = 0;
                    }
                } else if (info->pins->x1 == (config->cs_base_pin + ii)) {
                    // Inversion is per CS1
                    if (rom->cs1_state == CS_ACTIVE_LOW) {
                        config->invert_cs[ii] = 1;
                    } else {
                        config->invert_cs[ii] = 0;
                    }
                } else if (info->pins->x2 == (config->cs_base_pin + ii)) {
                    // Inversion is per CS1
                    if (rom->cs1_state == CS_ACTIVE_LOW) {
                        config->invert_cs[ii] = 1;
                    } else {
                        config->invert_cs[ii] = 0;
                    }
                }
            }
        }
    }

    // Figure out base address pin from SDRR info
    uint32_t img_size = set->size;
    config->addr_base_pin = get_lowest_addr_gpio(info, img_size, config->cs_base_pin);

    // Figure out base data pin from SDRR info
    config->data_base_pin = get_lowest_data_gpio(info);

    // Set the ROM table address
    config->rom_table_addr = rom_table_addr;

    // Set the number of address lines from ROM pins
    if (info->pins->chip_pins == 24) {
        config->num_addr_pins = 16;
    } else if (info->pins->chip_pins == 28) {
        // For 28 pin ROMs, we check the image size to decide how many address
        // pins to consider, not the ROM type.  This is because some version of
        // Studio uses 256KB images for <231024 ROMS.  However, since 0.6.4,
        // only a 231024 _needs_ a 256KB image.
        if (img_size > (64*1024)) {
            config->num_addr_pins = 18; // Includes OE/CE (OE also A16 for 231024)
        } else {
            config->num_addr_pins = 16; // Doesn't include OE/CE
        }
    }

    // Final checks
    if ((config->rom_table_addr == 0) || (config->rom_table_addr == 0xFFFFFFFF)) {
        ERR("PIO ROM serving requires valid ROM table address");
        limp_mode(LIMP_MODE_INVALID_CONFIG);
    }
    if (config->cs_base_pin >= 26) {
        ERR("PIO ROM serving requires CS pins to be GPIO 0-25");
        limp_mode(LIMP_MODE_INVALID_CONFIG);
    }
    if (config->data_base_pin >= 26) {
        ERR("PIO ROM serving requires Data pins to be GPIO 0-25");
        limp_mode(LIMP_MODE_INVALID_CONFIG);
    }
    if (config->addr_base_pin >= 26) {
        ERR("PIO ROM serving requires Address pins to be GPIO 0-25");
        limp_mode(LIMP_MODE_INVALID_CONFIG);
    }
    if (config->addr_read_irq > 1) {
        ERR("PIO ROM serving invalid addr_read_irq config");
        limp_mode(LIMP_MODE_INVALID_CONFIG);
    }
    if (config->addr_read_delay > 32) {
        ERR("PIO ROM serving invalid addr_read_delay config");
        limp_mode(LIMP_MODE_INVALID_CONFIG);
    }
    if (config->cs_active_delay > 32) {
        ERR("PIO ROM serving invalid cs_active_delay config");
        limp_mode(LIMP_MODE_INVALID_CONFIG);
    }

    // Log final configuration
    DEBUG("PIO ROM serving configuration:");
    DEBUG("Multi-ROM mode: %d", config->multi_rom_mode);
    DEBUG("  CS GPIOs: %d-%d", config->cs_base_pin, config->cs_base_pin + config->num_cs_pins - 1);
    for (int ii = 0; ii < config->num_cs_pins; ii++) {
        DEBUG("  - CS GPIO %d invert: %d", config->cs_base_pin + ii, config->invert_cs[ii]);
    }
    DEBUG("  Data GPIOs: %d-%d", config->data_base_pin, config->data_base_pin + config->num_data_pins - 1);
    DEBUG("  Address GPIOs: %d-%d", config->addr_base_pin, config->addr_base_pin + config->num_addr_pins - 1);
    DEBUG("  PIO algorithm options:");
    DEBUG("  - Address Read IRQ:   %d", config->addr_read_irq);
    DEBUG("  - Address Read Delay: %d", config->addr_read_delay);
    DEBUG("  - CS Active Delay:    %d", config->cs_active_delay);
    DEBUG("  - CS Inactive Delay:  %d", config->cs_inactive_delay);
    DEBUG("  - No DMA:             %d", config->no_dma);
    DEBUG("  ROM Table Address:  0x%08X", config->rom_table_addr);
}

// Default PIO ROM serving configuration
static piorom_config_t piorom_config = {
    .num_cs_pins = 0,
    .invert_cs = {0, 0, 0},
    .cs_base_pin = 255,
    .data_base_pin = 255,
    .num_data_pins = NUM_DATA_LINES,
    .addr_base_pin = 255,
    .num_addr_pins = NUM_ADDR_LINES,
    .addr_read_irq = PIO_CONFIG_ADDR_READ_IRQ,
    .addr_read_delay = PIO_CONFIG_ADDR_READ_DELAY,
    .cs_active_delay = PIO_CONFIG_CS_TO_DATA_OUTPUT_DELAY,
    .cs_inactive_delay = PIO_CONFIG_CS_INACTIVE_DATA_HOLD_DELAY,
#if defined(PIO_CONFIG_NO_DMA) && !PIO_CONFIG_NO_DMA
    .no_dma = 1,
#else // !PIO_CONFIG_NO_DMA
    .no_dma = 0,
#endif // PIO_CONFIG_NO_DMA
    .pad = {0, 0},
    .rom_table_addr = 0,
    .sm0_clkdiv_int = 1,
    .sm0_clkdiv_frac = 0,
    .pad0 = 0,
    .sm1_clkdiv_int = 1,
    .sm1_clkdiv_frac = 0,
    .pad1 = 0,
    .sm2_clkdiv_int = 1,
    .sm2_clkdiv_frac = 0,
    .pad2 = 0,
    .contiguous_cs_pins = 1,
    .multi_rom_mode = 0,
    .cs_pin_2nd_match = 255
};

// Apply any ROM set specific overrides to the PIO ROM serving configuration.
void piorom_overrides(
    const sdrr_rom_set_t *set,
    piorom_config_t *config
) {
    if ((set->extra_info) &&
        (set->serve_config != NULL) &&
        (set->serve_config != (void*)0xFFFFFFFF)) {
            const uint8_t *serve_config = (const uint8_t*)set->serve_config;

            // Current supported PIO serve override format:
            // Byte 0: 0xFE (signature)
            // Byte 1: addr_read_irq
            // Byte 2: addr_read_delay
            // Byte 3: cs_active_delay
            // Byte 4: cs_inactive_delay
            // Byte 5: no_dma
            // Byte 6: 0xFE (end signature)
            // Byte 7: 0xFF (padding)
            if ((serve_config[0] == 0xFE) &&
                (serve_config[1] != 0xFF) && 
                (serve_config[2] != 0xFF) && 
                (serve_config[3] != 0xFF) && 
                (serve_config[4] != 0xFF) && 
                (serve_config[5] != 0xFF) &&
                (serve_config[6] == 0xFE) &&
                (serve_config[7] == 0xFF)) {
                config->addr_read_irq = serve_config[1];
                config->addr_read_delay = serve_config[2];
                config->cs_active_delay = serve_config[3];
                config->cs_inactive_delay = serve_config[4];
                config->no_dma = serve_config[5];
                LOG("PIO found valid overriding serve config: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
                    config->addr_read_irq,
                    config->addr_read_delay,
                    config->cs_active_delay,
                    config->cs_inactive_delay,
                    config->no_dma
                );
            } else {
                ERR("PIO ROM serving invalid serve_config signature");
                DEBUG("  Bytes: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
                    serve_config[0],
                    serve_config[1],
                    serve_config[2],
                    serve_config[3],
                    serve_config[4],
                    serve_config[5],
                    serve_config[6],
                    serve_config[7]
                );
                limp_mode(LIMP_MODE_INVALID_CONFIG);
            }
    }
}

// Configure and start the Autonomous PIO/DMA ROM serving implementation.
void piorom(
    const sdrr_info_t *info,
    const sdrr_rom_set_t *set,
    uint32_t rom_table_addr
) {
    piorom_config_t config;

    DEBUG("%s", log_divider);

    memcpy(&config, &piorom_config, sizeof(piorom_config_t));

    // Apply any ROM set overrides
    piorom_overrides(set, &config);

    piorom_finish_config(&config, info, set, rom_table_addr);

    // Bring PIO0 and DMA out of reset
    RESET_RESET &= ~(RESET_PIO0 | RESET_DMA);
    while (!(RESET_DONE & (RESET_PIO0 | RESET_DMA)));

    // Setup the DMA channels:
    // - PIO block 0
    // - SM1 is the address read SM
    // - SM2 is the data byte output SM
    if (!config.no_dma) {
        piorom_setup_dma(&config, 0, 1, 2);
    }

    // Configure GPIOs for PIO function
    // - 2 CS pins
    // - CS pins start at GPIO 10
    // - CS active high/low config
    // - Data pins start at GPIO 0
    // - Address pins start at GPIO 8
    piorom_set_gpio_func(&config);

    // Load and configure the PIO programs
    // - 2 CS pins
    // - CS pins start at GPIO 10
    // - Data pins start at GPIO 0
    // - Address pins start at GPIO 8
    piorom_load_programs(&config);

    if (!config.no_dma) {
        // Start the PIOs.  This kicks off the autonomous ROM serving.
        piorom_start_pios();

        while (1) {
            // Low power wait for (VBUS) interrupt.  Avoids any potential SRAM or
            // peripheral access that might introduce jitter on the PIO/DMA
            // serving.
            __asm volatile("wfi");
        }
    } else {
        DEBUG("PIO ROM serving running without DMA - CPU active loop");

        register volatile uint32_t *ctrl asm("r0") = &PIO0_CTRL;
        register volatile uint32_t *rxf1 asm("r2") = &PIO0_SM_RXF(1);
        register volatile uint32_t *txf2 asm("r3") = &PIO0_SM_TXF(2);
        register volatile uint32_t *irq  asm("r4") = &PIO0_IRQ_FORCE;
        register uint32_t irq_set asm("r5") = 0x1;  // Set IRQ 0

        asm volatile (
            // Enable SM0/1/2
            "movs r1, #0x7\n"
            "str  r1, [r0]\n"

            // 6 cycle version with IRQ triggering SM1 to read address -
            // essentially pacing SM1, avoiding addr reads backing up
            "1:\n"
            "ldr  r0, [r2]\n"       // Read address from SM1 RX (1 cycle + 1 stall)
            "ldrb r1, [r0]\n"       // Read byte from that address (1 cycle)
            "str  r5, [r4]\n"       // Set IRQ triggering SM1 to re-read (1 cycle)
            "str  r1, [r3]\n"       // Write byte to SM2 TX (1 cycle)
            "b    1b\n"             // Loop (1 cycle)

            /*
            // 5 cycle version, eliminating read address stall with branch
            "ldr  r0, [r2]\n"
            "1:\n"
            "ldrb r1, [r0]\n"
            "str  r5, [r4]\n"
            "str  r1, [r3]\n"
            "ldr  r0, [r2]\n"
            "b    1b\n"
            */

            // Pathological 5 cycle version, requires no IRQ detection in SM1.
            // Shaves char ROM ser ing down to 50MHz.
            //"1:\n"
            //"str  r1, [r3]\n"       // Write byte to SM2 TX (1 cycle)
            //"ldr  r0, [r2]\n"       // Read address from SM1 RX (1 cycle + 1 stall)
            //"ldrb r1, [r0]\n"       // Read byte from that address (1 cycle)
            //"b    1b\n"             // Loop (1 cycle)

            :
            : "r"(ctrl), "r"(rxf1), "r"(txf2), "r"(irq), "r"(irq_set)
            : "r1", "memory"
        );
    }
}

#endif // RP235X