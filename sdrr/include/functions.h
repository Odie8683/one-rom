// Function prototypes

// Copyright (C) 2025 Piers Finlayson <piers@piers.rocks>
//
// MIT License

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

// main.c
extern uint32_t check_sel_pins(uint32_t *sel_mask);
extern void check_enter_bootloader(uint32_t sel_pins, uint32_t sel_mask);
extern uint8_t metadata_present(const sdrr_info_t *info);
extern void limp_mode(limp_mode_pattern_t pattern);
extern void process_firmware_overrides(
    sdrr_runtime_info_t *runtime_info,
    const sdrr_rom_set_t *set
);
extern int main(void);

// utils.c
extern uint32_t check_sel_pins(uint32_t *sel_mask);
#if defined(BOOT_LOGGING)
extern void log_init();
extern void log_roms(const onerom_metadata_header_t *metadata);
extern void do_log(const char *, ...);
extern void err_log(const char *, ...);
#endif // BOOT_LOGGING
#if defined(MAIN_LOOP_LOGGING) || defined(DEBUG_LOGGING)
typedef void (*ram_log_fn)(const char*, ...);
#endif // MAIN_LOOP_LOGGING
#if defined(EXECUTE_FROM_RAM)
extern void copy_func_to_ram(void (*fn)(void), uint32_t ram_addr, size_t size);
extern void execute_ram_func(uint32_t ram_addr);
#endif // EXECUTE_FROM_RAM
extern void setup_status_led(void);
extern void delay(volatile uint32_t count);
extern void blink_pattern(uint32_t on_time, uint32_t off_time, uint8_t repeats);

// stm32f4.c and rp235x.c external functions
//
// If adding a new platform, these are the functions you need to implement,
// plus those in include/*inlines.h
extern void platform_specific_init(void);
void setup_vbus_interrupt(void);
void vbus_connect_handler(void);
extern void setup_clock(void);
extern void setup_gpio(void);
extern void setup_mco(void);
extern uint32_t setup_sel_pins(uint64_t *sel_mask, uint64_t *flip_bits);
extern uint64_t get_sel_value(uint64_t sel_mask, uint64_t flip_bits);
extern void disable_sel_pins(void);
extern void setup_status_led(void);
extern void blink_pattern(uint32_t on_time, uint32_t off_time, uint8_t repeats);
extern void enter_bootloader(void);
extern void check_config(
    const sdrr_info_t *info,
    const sdrr_runtime_info_t *runtime,
    const sdrr_rom_set_t *set
);
extern void platform_logging(void);
#if defined(STM32F4)
void dfu(void);
#endif // STM32F4

// pio.c
extern void pio(
    const sdrr_info_t *info,
    const sdrr_rom_set_t *set,
    uint32_t rom_table_addr
);
// piorom.c
#if defined(RP235X)
extern void piorom(
    const sdrr_info_t *info,
    const sdrr_rom_set_t *set,
    uint32_t rom_table_addr
);
extern void pioram(
    const sdrr_info_t *info,
    uint32_t rom_table_addr
);
// piodis.c
void pio_log_sm(
    const char *sm_name,
    uint8_t pio_block,
    uint8_t pio_sm,
    uint16_t *instr_scratch,
    uint8_t first_instr,
    uint8_t start,
    uint8_t end
);

#endif // RP235X

// rom_impl.c
#if !defined(TIMER_TEST) && !defined(TOGGLE_PA4)
extern void main_loop(
    const sdrr_info_t *info,
    const sdrr_runtime_info_t *runtime,
    const sdrr_rom_set_t *set
);
extern uint8_t get_rom_set_index(uint32_t sel_pins, uint32_t sel_mask);
extern void* preload_rom_image(const sdrr_rom_set_t *set);
#endif // !TIMER_TEST && !TOGGLE_PA4

// test function prototypes
#if defined(TIMER_TEST) || defined(TOGGLE_PA4)
extern void main_loop(void);
#endif // TIMER_TEST || TOGGLE_PA4

#endif // FUNCTIONS_H