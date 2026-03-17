// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

#if !defined(USB_LED_H)
#define USB_LED_H

void led_handle_pending_set(void);
void led_handle_ongoing_led_modes(void);

// LED timing constants
#define ONEROM_BEACON_DURATION_MS   2500u
#define ONEROM_BEACON_TOGGLE_MS     50u     // 10Hz

typedef struct {
    onerom_led_subcmd_t mode;
    uint8_t led_state;
    uint8_t flame_index;
    uint8_t pre_beacon_state;
    uint32_t last_toggle_ms;
    uint32_t beacon_start_ms;
} led_status_t;

static const struct {
    uint8_t state;
    uint16_t ms;
} flame_table[] = {
    {1, 60}, {1, 40}, {0, 15}, {1, 35}, {1, 55},
    {0, 20}, {1, 70}, {0, 10}, {1, 45}, {1, 30},
    {0, 25}, {1, 50}, {1, 65}, {0, 15}, {1, 40},
};
#define FLAME_TABLE_LEN (sizeof(flame_table) / sizeof(flame_table[0]))

#endif //USB_LED_H