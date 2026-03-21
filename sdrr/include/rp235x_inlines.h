// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

// One ROM STM32F4 Specific Routines

#ifndef RP235X_INLINES_H
#define RP235X_INLINES_H

// Does any preparatory GPIO setup within main_loop()
//
// Inlined as used by main_loop (which may be in RAM)
static inline void __attribute__((always_inline)) main_loop_gpio_init() {
    // No-op
}

// Sets pull-ups or pull-downs on the X1/X2 pins
//
// Inlined as used by main_loop (which may be in RAM)
static inline void __attribute__((always_inline)) set_x_pulls(
    const sdrr_pins_t *pins,
    uint8_t x1_pull,
    uint8_t x2_pull
) {
    if (pins->x1 < MAX_USED_GPIOS) {
        if (x1_pull) {
            GPIO_PAD(pins->x1) = PAD_INPUT_PU;
        } else {
            GPIO_PAD(pins->x1) = PAD_INPUT_PD;
        }
    }

    if (pins->x2 < MAX_USED_GPIOS) {
        if (x2_pull) {
            GPIO_PAD(pins->x2) = PAD_INPUT_PU;
        } else {
            GPIO_PAD(pins->x2) = PAD_INPUT_PD;
        }
    }
}

// Set up the masks to turn data into an input/output
//
// Inlined as used by main_loop (which may be in RAM)
static inline void __attribute__((always_inline)) setup_data_masks(
    const sdrr_info_t *info,
    const sdrr_runtime_info_t *runtime_info,
    uint32_t *data_output_mask_val,
    uint32_t *data_input_mask_val
) {
    // We use GPIO_OE register to set lines as inputs/outputs.  We are
    // therefore controlling all GPIOs at once on the RP235x.  This means we
    // need to correctly handle the status LED, if used.
    uint8_t data_bits = 8;
    if (runtime_info->bit_mode == BIT_MODE_16) {
        data_bits = 16;
    }
    if (info->pins->data[0] < data_bits) {
        *data_output_mask_val = 0xFF;
    } else {
        *data_output_mask_val = 0xFF << 16;
    }
    *data_input_mask_val = 0;

    // This doesn't properly handle 16 bit mode with data lines, but this is
    // for the CPU algorithm anyway, which isn't supported.
    if ((info->status_led_enabled) && (info->pins->status <= MAX_USED_GPIOS)) {
        uint8_t pin = info->pins->status;
        *data_output_mask_val |= (1 << pin);
        *data_input_mask_val |= (1 << pin);
    }
}

// Inlined as may be used by main_loop (which may be in RAM)
static inline void __attribute__((always_inline)) status_led_on(uint8_t pin) {
    // Set to 0 to turn on
    SIO_GPIO_OUT_CLR_PIN(pin);
}

// Inlined as may be used by main_loop (which may be in RAM)
static inline void __attribute__((always_inline)) status_led_off(uint8_t pin) {
    // Set to 1 to turn on
    SIO_GPIO_OUT_SET_PIN(pin);
}

static inline void __attribute__((always_inline)) status_led_disable(uint8_t pin) {
    // Disable the status LED by disabling output
    GPIO_PAD(pin) |= PAD_OUTPUT_DISABLE;
}

#endif // RP235X_INLINES_H
