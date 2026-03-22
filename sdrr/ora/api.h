// Copyright (C) 2026 Piers Finlayson <piers@piers.rocks>
//
// MIT License

/**
 * @file api.h
 * @brief One ROM's plugin API
 *
 * This file defines the API for plugins to interact with the One ROM firmware.
 * It includes an enumeration of API function identifiers, a lookup function
 * to retrieve function pointers based on these identifiers, and the prototypes
 * for the API functions themselves.
 */

#if !defined(PLUGIN_API_H)
#define PLUGIN_API_H

#include <stdint.h>
#include <stddef.h>

/**
 * @defgroup plugin_api One ROM Plugin API
 * @brief The complete API for One ROM plugins
 * @{
 */

/**
 * @defgroup plugin_api_ids API Identifiers
 * @brief Enumeration of API function identifiers
 * @{
 */

/**
 * @brief API function identifiers
 *
 * This enumeration defines the identifiers for the API functions available
 * to plugins. Each identifier corresponds to a specific API function.
 */
typedef enum {
    /**
     * @brief Reboot into BOOTSEL mode
     * @sa ora_reboot_bootsel_fn_t
     */
    ORA_ID_REBOOT_BOOTSEL = 0x00000000,

    /**
     * @brief Allocate memory
     * @sa ora_alloc_fn_t
     */
    ORA_ID_ALLOC = 0x00000001,

    /**
     * @brief Get One ROM information
     * @sa ora_get_firmware_info_fn_t
     */
    ORA_ID_GET_FIRMWARE_INFO = 0x00000002,

    /**
     * @brief Log a message
     * @sa ora_log_fn_t
     */
    ORA_ID_LOG = 0x00000003,

    /**
     * @brief Log an error message
     * @sa ora_err_log_fn_t
     */
    ORA_ID_ERR_LOG = 0x00000004,

    /**
     * @brief Log a debug message
     * @sa ora_debug_log_fn_t
     */
    ORA_ID_DEBUG_LOG = 0x00000005,

    /**
     * @brief Get free memory size
     * @sa ora_get_free_mem_fn_t
     */
    ORA_ID_GET_FREE_MEM = 0x00000006,

    /**
     * @brief Set the status LED on or off
     * @sa ora_set_status_led_fn_t
     */
    ORA_ID_SET_STATUS_LED = 0x00000007,

    /**
     * @brief Setup the USB PLL
     * @sa ora_setup_usb_pll_fn_t
     */
    ORA_ID_SETUP_USB = 0x00000008,

    /**
     * @brief Setup the ADC
     * @sa ora_setup_adc_fn_t
     */
    ORA_ID_SETUP_ADC = 0x00000009,

    /**
     * @brief Register an IRQ handler
     * @sa ora_register_irq_fn_t
     */
    ORA_ID_REGISTER_IRQ = 0x0000000A,

    /**
     * @brief Set plugin context
     * @sa ora_set_plugin_context_fn_t
     */
    ORA_ID_SET_PLUGIN_CONTEXT = 0x0000000B,

    /**
     * @brief Get plugin context
     * @sa ora_get_plugin_context_fn_t
     */
    ORA_ID_GET_PLUGIN_CONTEXT = 0x0000000C,

    /**
     * @brief Get the current system clock frequency in MHz
     * @sa ora_get_sysclk_mhz_fn_t
     */
    ORA_ID_GET_SYSCLK_MHZ = 0x0000000D,

    /**
     * @brief Enable an IRQ
     * @sa ora_enable_irq_fn_t
     */
    ORA_ID_ENABLE_IRQ = 0x0000000E,

    /**
    * @brief Get the clkref frequency in MHz
    * @sa ora_get_clkref_mhz_fn_t
    */
    ORA_ID_GET_CLKREF_MHZ = 0x0000000F,

    /**
     * @brief Get a pointer to the runtime info structure
      * @sa ora_get_runtime_info_fn_t
     */
    ORA_ID_GET_RUNTIME_INFO = 0x00000010,

    /**
     * @brief Get the size of a ROM from its type
      * @sa ora_get_chip_size_from_type_fn_t
     */
    ORA_ID_GET_CHIP_SIZE_FROM_TYPE = 0x00000011,

    /** Invalid API identifier */
    ORA_ID_INVALID = 0xFFFFFFFF,
} api_id_t;
#if !defined(TEST_BUILD)
_Static_assert(sizeof(api_id_t) == 4, "api_id_t must be 4 bytes");
#endif // !TEST_BUILD

/** @} */ // plugin_api_ids

/**
 * @defgroup plugin_api_types API Types
 * @brief Types use by the plugin API
 * @{
 */

/**
 * @brief Plugin types
 */
typedef enum {
    ORA_PLUGIN_TYPE_SYSTEM = 0,
    ORA_PLUGIN_TYPE_USER   = 1,
    ORA_PLUGIN_TYPE_PIO    = 2,
} ora_plugin_type_t;
#if !defined(TEST_BUILD)
_Static_assert(sizeof(ora_plugin_type_t) == 1, "ora_plugin_type_t must be 1 byte");
#endif // !TEST_BUILD

/**
 * @brief MCU Cores
 */
typedef enum {
    ORA_CORE_0 = 0,
    ORA_CORE_1 = 1,
} ora_core_t;
#if !defined(TEST_BUILD)
_Static_assert(sizeof(ora_core_t) == 1, "ora_core_t must be 1 byte");
#endif // !TEST_BUILD

/**
 * @brief IRQ numbers
 */
typedef enum {
    ORA_IRQ_TIMER0_IRQ_0 = 0,
    ORA_IRQ_USBCTRL_IRQ = 14,
    ORA_IRQ_INVALID = 0xFF,
} ora_irq_t;
#if !defined(TEST_BUILD)
_Static_assert(sizeof(ora_irq_t) == 1, "ora_irq_t must be 1 byte");
#endif // !TEST_BUILD

/**
 * @brief IRQ handler function type
 *
 * This is used by plugins to define a function that will be called when a
 * specific IRQ occurs.
 */
typedef void (*ora_irq_handler_t)(void);

/** @} */ // plugin_api_types

/**
 * @defgroup plugin_api_macros API Macros
 * @brief Macros for use in plugins
 * @{
 */

/**
 * @brief Get the context for the system plugin
 *
 * This macro retrieves the context pointer previously stored by the system
 * plugin via @ref ora_set_plugin_context_fn_t. It calls the firmware's
 * get_plugin_context function at its fixed absolute address, which is
 * guaranteed to remain stable across firmware versions that support this
 * API version.
 *
 * Intended for use inside IRQ handlers where @ref ora_lookup_fn_t is not
 * available.
 *
 * @return void* pointer to the system plugin's context, or NULL if not set
 */
#define ORA_GET_PLUGIN_CONTEXT_SYSTEM   (uintptr_t)(0x20080000 + 40)

/**
 * @brief Get the context for the user plugin
 *
 * Equivalent to @ref ORA_GET_PLUGIN_CONTEXT_SYSTEM but for the user plugin.
 * Retrieves the context pointer previously stored by the user plugin via
 * @ref ora_set_plugin_context_fn_t.
 *
 * Intended for use inside IRQ handlers where @ref ora_lookup_fn_t is not
 * available.
 *
 * @return void* pointer to the user plugin's context, or NULL if not set
 */
#define ORA_GET_PLUGIN_CONTEXT_USER     (uintptr_t)(0x20080000 + 44)

/** @} */ // plugin_api_macros

/**
 * @defgroup plugin_api_functions Protoypes for API Functions
 * @brief API lookup function, plugin entry point, and individual API function prototypes
 * @{
 */

 /**
 * @brief System plugin entry point arguments
 * 
 * Passed to the plugin entry point function as a pointer.
 */
typedef struct {
    /**
     * @brief The core the plugin is running on
     */
    ora_core_t core;

    /**
     * @brief The plugin's static RAM base address
     * 
     * Can be used by the plugin at runtime to validate it has been built with
     * the correct linker settings, and exit early if not.
     */
    uint32_t static_ram_base;

    /**
     * @brief The plugin's static RAM size in bytes
     * 
     * Can be used by the plugin at runtime to validate it has been built with
     * the correct linker settings, and exit early if not.
     */
    uint32_t static_ram_size;

    /**
     * @brief The top address of the stack for the core running this plugin
     * 
     * Some of the stack will have been used before the plugin entry point is
     * called.  The rest can be assumed unused.
     * 
     * Can be used by the plugin to check it isn't exceeding the stack limits.
     * If careful, a plugin can use unused stack space in this core's stack as
     * additional RAM.
     */
    uint32_t stack_top;

    /**
     * @brief The total size of the stack for the core running this plugin
     * @sa stack_top
     */
    uint32_t stack_size;
} ora_entry_args_t;

/**
 * @brief Lookup an API function pointer by its identifier
 *
 * This function takes an API function identifier and returns a pointer
 * to the corresponding API function. If the identifier is invalid,
 * it returns NULL.
 *
 * If the idenfier is valid and implemented in the current version of the
 * firmware, the returned pointer is guaranteed to be non-NULL, even if the
 * underlying capability (logging, status LED, etc) is not present.
 *
 * For discovery, a pointer to this function is provided to the plugin as an
 * argument to the plugin's entry point.
 *
 * @param id The identifier of the API function to look up.
 * @return A pointer to the API function corresponding to the given identifier,
 * or NULL if the identifier is invalid.
 */
typedef void *(*ora_lookup_fn_t)(api_id_t id);

/**
 * @brief Plugin entry point function type
 *
 * This typedef defines the signature that all plugin entry point functions
 * must conform to.  Use @ref ORA_DEFINE_SYSTEM_PLUGIN or
 * @ref ORA_DEFINE_USER_PLUGIN to define your entry point.
 *
 * @param ora_lookup_fn A pointer to the API lookup function
 * @param plugin_type   Whether this is a system or user plugin
 * @param entry_args    A pointer to a structure containing the plugin's entry
 * arguments
 */
typedef void (*ora_plugin_entry_t)(
    ora_lookup_fn_t ora_lookup_fn,
    ora_plugin_type_t plugin_type,
    const ora_entry_args_t *entry_args
);

/**
 * @brief Reboot into BOOTSEL mode
 * @sa ORA_ID_REBOOT_BOOTSEL
 * 
 * Reboots One ROM into the RP2350's BOOTSEL mode, for repogramming or
 * recovery.  This (obviously) stops One ROM serving the ROM image and does
 * not return.  There may be a slight (e.g. 10ms) delay before the reboot.
 */
typedef void (*ora_reboot_bootsel_fn_t)(void);

/**
 * @brief Allocate memory
 * @sa ORA_ID_ALLOC
 *
 * This allocates memory on a 4 byte boundary from the start of One ROM's
 * spare pool of memory.  Allocations are for the lifetime of the firmware
 * and are not freed until the device is reset.  The firmware does not track
 * individual allocations, so it is the caller's responsibility to ensure that
 * they do not exceed the allocated memory.
 *
 * The amount of free memory will vary dramatically depending on the ROM image
 * currently being served.  A One ROM 24 will have around 450KB of free memory,
 * while a One ROM 32 or 40 will have 1-2KB.
 *
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory, or NULL if allocation failed.
 */
typedef void *(*ora_alloc_fn_t)(size_t size);

/**
 * @brief Get One ROM information
 * @sa ORA_ID_ONEROM_INFO
 *
 * @return A pointer to a structure containing information about the One ROM
 * firmware, the device it is running on, configured ROM sets, and runtime
 * information. The exact structure of this data is defined by the One ROM
 * firmware - see `sdrr_info_t` in `sdrr/include/config_base.h` for details.
 *
 * Plugins must consider this data and any data pointed to it as read-only.
 * Some of the data resides on flash, and other data in SRAM.  However, the
 * firmware itself may rely on the immutability of any data contained within.
 * 
 * This function may be deprecated in a future version of the API with
 * additional targetted API calls replacing it.
 */
typedef const void *(*ora_get_firmware_info_fn_t)(void);

/**
 * @brief Log a message
 * @sa ORA_ID_LOG
 * 
 * If the One ROM firmware is compiled with logging support enabled, this
 * function logs via RTT.  If there is no logging support this function
 * silently fails.
 *
 * @param msg printf-style format string
 * @param ... Format arguments
 */
typedef void (*ora_log_fn_t)(const char *msg, ...);

/**
 * @brief Log an error message
 *
 * Equivalent to @ref ora_log but flags the log as an error.
 * @sa ORA_ID_ERR_LOG
 * 
 * If the One ROM firmware is compiled with logging support enabled, this
 * function logs via RTT.  If there is no logging support this function
 * silently fails.
 *
 * @param msg printf-style format string
 * @param ... Format arguments
 */
typedef void (*ora_err_log_fn_t)(const char *msg, ...);

/**
 * @brief Log a debug message
 *
 * Equivalent to @ref ora_log but flags the log as a debug message.
 * @sa ORA_ID_DEBUG_LOG
 *
 * If the One ROM firmware is compiled with debug logging support enabled,
 * this function logs via RTT.  If there is no logging support this function
 * silently fails.
 *
 * @param msg printf-style format string
 * @param ... Format arguments
 */
typedef void (*ora_debug_log_fn_t)(const char *msg, ...);

/**
 * @brief Get the amount of free memory
 * @sa ORA_ID_GET_FREE_MEM
 *
 * @return The number of bytes of free memory available.
 */
typedef size_t (*ora_get_free_mem_fn_t)(void);

/**
 * @brief Set the status LED on or off
 * @sa ORA_ID_SET_STATUS_LED
 *
 * This controls the status LED, if is enabled and properly configured.  If it
 * is not, this function silently fails.
 *
 * @param on Set to 1 to turn the LED on, or 0 to turn it off.
 */
typedef void (*ora_set_status_led_fn_t)(uint8_t on);

/**
 * @brief Setup the USB controller including clocking it
 * @sa ORA_ID_SETUP_USB
 * 
 * This is used by plugins that want to use the USB functionality of the
 * RP2350, to set up the USB PLL.
 */
typedef void (*ora_setup_usb_fn_t)(void);

/**
 * @brief Setup the ADC
 * @sa ORA_ID_SETUP_ADC
 *
 * This is used by plugins that want to use the ADC functionality of the
 * RP2350, to set up the ADC.  This function setups up the USB PLL as well, if
 * it has not already been set up.
 */
typedef void (*ora_setup_adc_fn_t)(void);

/**
 * @brief Register an IRQ handler
 * @sa ORA_ID_REGISTER_IRQ
 * 
 * This is used by plugins that want to register an IRQ handler for a specific
 * IRQ.
 *
 * An IRQ handler can also be deregistered by calling this function with a NULL
 * handler pointer.
 */
typedef void (*ora_register_irq_fn_t)(ora_irq_t irq, ora_irq_handler_t handler);

/**
 * @brief Set plugin context
 * @sa ORA_ID_SET_PLUGIN_CONTEXT
 *
 * This is used by plugins to set a pointer to their context structure, which
 * the One ROM firmware will store and make available to the plugin when it
 * calls into it.  This is useful for storing RAM dynamically allocated by the
 * plugin and accessing it from elsewhere, like an IRQ handler.
 *
 * @param plugin The type of plugin (system or user) for which to set the context
 * @param context A pointer to the plugin's context structure
 */
typedef void (*ora_set_plugin_context_fn_t)(ora_plugin_type_t plugin, void *context);

/**
 * @brief Get plugin context
 * @sa ORA_ID_GET_PLUGIN_CONTEXT
 *
 * This is used by plugins to get a pointer to their context structure, which
 * the One ROM firmware stores and makes available to the plugin when it calls
 * into it.  This is useful for storing RAM dynamically allocated by the plugin
 * and accessing it from elsewhere, like an IRQ handler.
 *
 * @param plugin The type of plugin (system or user) for which to get the context
 * @return A pointer to the plugin's context structure, or NULL if no context
 * has been set.
 */
typedef void *(*ora_get_plugin_context_fn_t)(ora_plugin_type_t plugin);

/**
 * @brief Get the SYSCLK frequency in MHz
 * @sa ORA_ID_GET_SYSCLK_MHZ
 * 
 * Returns the current SYSCLK frequency in MHz, as configured by the main
 * firmware.
 */
typedef uint32_t (*ora_get_sysclk_mhz_fn_t)(void);

/**
 * @brief Enable an IRQ
 * @sa ORA_ID_ENABLE_IRQ
 * 
 * This is used by plugins to enable an IRQ for which they have registered a
 * handler.  If the plugin has not registered a handler for the specified IRQ,
 * this function silently fails.
 *
 * @param irq The IRQ to enable
 * @param enable Set to 1 to enable the IRQ, or 0 to disable it.
 */
typedef void (*ora_enable_irq_fn_t)(ora_irq_t irq, uint8_t enable);

/**
 * @brief Get the CLKREF frequency in MHz
 * @sa ORA_ID_GET_CLKREF_MHZ
 * 
 * Returns the current CLKREF frequency in MHz, as defined by the hardware.
 * Currently returns a fixed 12 MHz.
 */
typedef uint32_t (*ora_get_clkref_mhz_fn_t)(void);

/**
 * @brief Get a pointer to the runtime info structure
 * @sa ORA_ID_GET_RUNTIME_INFO
 *
 * Returns a pointer to the runtime info structure, which contains information
 * about the current state of the firmware and device that may be useful for
 * plugins. The exact structure of this data is defined by the One ROM firmware
 * - see `sdrr_runtime_info_t` in `sdrr/include/config_base.h` for details.
 *
 * Plugins must consider this data and any data pointed to it as read-only.
 * Some of the data resides on flash, and other data in SRAM.  However, the
 * firmware itself may rely on the immutability of any data contained within.
 */
typedef const void *(*ora_get_runtime_info_fn_t)(void);

/**
 * @brief Get the size of a chip type
 *
 * This is used by plugins to get the size of a chip type in bytes, for use in
 * memory management and bounds checking.
 *
 * @param chip_type The chip type to get the size of
 * @return The size of the specified chip type in bytes, or 0 if the chip type
 * is invalid
 */
typedef uint32_t (*ora_get_chip_size_from_type_fn_t)(uint32_t chip_type);

/** @} */ // plugin_api_functions

/**
 * @defgroup plugin_header Plugin Header
 * @brief Constants defining the plugin header format
 * @{
 */

/**
 * @brief Magic number identifying a valid One ROM plugin
 *
 * This magic number is used to verify that a plugin is valid and compatible
 * with the One ROM firmware. Plugins must include this magic number in their
 * header to be recognized by the firmware.
 */
#define ORA_PLUGIN_MAGIC 0x2041524F  // 'ORA ' backwards, so it appears forwards in little-endian

/**
 * @brief Plugin API version - major version bump indicates breaking changes
 *
 * This version number is used to ensure compatibility between plugins and the
 * One ROM firmware. Plugins must specify the API version they are compatible
 * with in their header.  A One ROM firmware only supports a defined set of API
 * versions, and will refuse to run plugins with incompatible versions.
 */
#define ORA_PLUGIN_VERSION_1 0x00000001

/**
 * @brief Plugin header structure
 *
 * This structure defines the header for a One ROM plugin and the appropriate
 * data in this format must be placed at the start of the plugin binary for it
 * to be recognized and loaded by the One ROM firmware.
 */
typedef struct {
    /**
     * @brief Magic number identifying a valid One ROM plugin
     * @sa ORA_PLUGIN_MAGIC
     */
    uint32_t magic;

    /**
     * @brief Plugin API version
     * @sa ORA_PLUGIN_VERSION
     */
    // offset 4
    uint32_t api_version;

    /** 
     * @brief Plugin's major version
     */
    // offset 8
    uint16_t major_version;

    /**
     * @brief Plugin's minor version
     */
    // offset 10
    uint16_t minor_version;

    /**
    * @brief Plugin's patch version
    */
    // offset 12
    uint16_t patch_version;

    /**
    * @brief Plugin's build version
    */
    // offset 14
    uint16_t build_version;

    /**
     * @brief Plugin's main function location.
     *
     * One ROM launches the pluging by calling the @ref ora_plugin_entry_t
     * function at this location.
     */
    // offset 16
    ora_plugin_entry_t entry;

    /**
     * @brief Plugin type
     * @sa ora_plugin_type_t
     */
    // offset 20
    ora_plugin_type_t plugin_type;

    /**
     * @brief Statically allocated memory usage
     * 
     * Each type of plugin is reserved a portion of SRAM at link time, so it
     * can use this without needing to call the API to allocate memory, if it
     * is sufficient.
     *
     * Each of system and user plugins are reserved different amounts of RAM
     * and in different locations.
     *
     * This field indicates how much of this plugin type's reserved RAM the
     * plugin uses, so that the firmware can allow the remainder to be used
     * as dynamically allocated pool memory.
     *
     * If this field is n, the memory used is 2 to the power of (n+1) bytes,
     * with 0 meaning no statically allocated RAM is used.
     * - 0 = 0 bytes used
     * - 1 = 4 bytes used
     * - 2 = 8 bytes used
     * ...
     * - 255 = maximum available statically allocated RAM used.
     */
    // offset 21
    uint8_t sam_usage;

    /**
     * @brief Firmware overrides this plugin wants to apply
     * 
     * Only supported by system plugins, used to indicate to the firmware that
     * defaults built into the firmware, or overriden by config, should
     * be overridden.
     *
     * For future compatibility, any unused bits must be set to 0.
     *
     * This is a bit field, with 1 indicating override.  Bit 0 = LSB
     * Bit 0 - Disable VBUS detect @sa ORA_OVERRIDE1_DISABLE_VBUS_DETECT
     */
    // offset 22
    uint8_t overrides1;

    /**
     * @brief Plugin properties
     * 
     * 
     * This is used to indicate properties of the plugin to the firmware, to
     * allow the core firmware and firmware parses to make informed decisions
     * about how to use the plugin.
     * 
     * For future compatibility, any unused bits must be set to 0.
     * 
     * This is a bit field, with 1 indicating the presence of the property.
     * Bit 0 = LSB
     * Bit 0 = supports running while USB is connected @sa ORA_PROPERTY1_SUPPORTS_USB_RUNNING
     */
    // offset 23
    uint8_t properties1;

    /**
     * @brief Reserved for future use
     *
     * This field is reserved for future use and must be set to 0.
     */
    uint8_t reserved[232];
} ora_plugin_header_t;
#define ORA_PLUGIN_HEADER_SIZE 256  // Must not change without version bump
#if !defined(TEST_BUILD)
_Static_assert(sizeof(ora_plugin_header_t) == ORA_PLUGIN_HEADER_SIZE, "ora_plugin_header_t must be 256 bytes");
#endif // !TEST_BUILD

/**
 * @brief Firmware override flag for VBUS detect
 */
#define ORA_OVERRIDE1_DISABLE_VBUS_DETECT (1 << 0)

/** 
 * @brief Plugin property flag for USB support
 */
#define ORA_PROPERTY1_SUPPORTS_USB_RUNNING (1 << 0)

/** @} */

/** @} */ // plugin_api

#endif // PLUGIN_API_H