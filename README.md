# One ROM

**[One ROM](https://onerom.org) - One ROM To Rule Them All**

The most flexible ROM replacement for your retro computer.  It is highly configurable and low-cost.  Get them fabbed at JLC for under $5 each in a quantity of 10 or more.

Based on a Raspberry Pi RP2350 (One ROM Fire 🔥) or STM32F4 (One ROM Ice ❄️).

One ROM is available as 24 pin and 28 pin versions, emulating nearly all systems' 24 and 28 pin ROMs, including any possible chip select configurations with a single hardware variants, and can serve different images to multiple ROM sockets simultaneously.

Replaces failed ROMs in Commodore 64s, VIC-20s, PETs, Ataris, BBC, TIs, disk drives and many other types of systems, like pinball machines and music synthesizers.

ROM types replaced:
- 24 pin: 2704, 2708, 2716, 2732, 2316, 2332, 2364
- 28 pin: 2764, 27128, 27256, 27512, 23128, 23256, 23512, 231024

As of the latest firmware, One ROM also supports RAM chips of the correct footprint, including the 6116 and 2016 2K SRAM chips.

One ROM was formerly known as Software Defined Retro ROM (SDRR).

## Quick Start

See https://onerom.org/start/

## Key Features

💰 Based on sub-$2 microcontrollers.  **[Choose from Raspberry Pi (Fire 🔥) or STM (Ice ❄️) flavours](docs/MCU-SELECTION.md)**.

⚡ **[Fast](docs/STM32-SELECTION.md)** enough for PETs, VIC-20s, C64s, 1541s, IEEE drives, etc. Support faster systems with higher end microcontroller.

📐 **[Same footprint](docs/images/sdrr-24-pin-side.png)** as original ROMs - doesn't overhang the socket like other solutions.

🔌 **[USB Version](https://onerom.org/prog/)** - no programmer required, program from your browser.

🚀 **[Quick programming](docs/PROGRAMMING.md)** - build and flash in <10 seconds.

🛠️ **[Reflash in situ](docs/PROGRAMMING.md)** - no need to remove the ROM from the host when reprogramming.

⚙️ **[Software configurable](docs/CONFIGURATION.md)** chip select lines - no hardware jumpers required.

💾 Stores up to **[16 ROM images](docs/STM32-SELECTION.md)** of different sizes and chip select configurations.  Image selectable via jumpers.

📦 **[Replace multiple ROMs with one ROM](docs/MULTI-ROM-SETS.md)** a single One ROM can replace up to 3 original ROMs e.g. all of C64 kernel, BASIC, character set.

🔀 **[Dynamic bank switching](docs/MULTI-ROM-SETS.md)** - switch between ROM images on the fly, e.g. different char ROMs.

💻 **[Hackable at runtime](docs/RUNTIME-ACCESS.md)** - change the ROM images being served and access telemetry from the ROM at runtime using **[Airfrog](https://piers.rocks/u/airfrog)** or other SWD probes.

🧩 **[Images combined automatically](docs/CONFIGURATION.md)** - no need to manually build up your own larger PROM image containing multiple retro ROMs.

🏭 **[Two layer PCB](sdrr-pcb/README.md)**, component on single-side, limited BOM for low manufacturing cost/complexity.

🎯 Supports multiple **[STM32F4xxR](docs/STM32-SELECTION.md)** variants: F401, F411, F405, F446 (others can be added).

🔓 **[Open source](LICENSE.md)** software and hardware.

## Introduction

The video below provides an introduction to One ROM:

[![Video Title](https://img.youtube.com/vi/Jhe4LF5LrZ8/maxresdefault.jpg)](https://youtu.be/Jhe4LF5LrZ8)

## Hardware

This is the Fire 24-pin version, hardware revison A.  See [sdrr-pcb](sdrr-pcb/README.md) for the hardware designs and documentation.

<div style="display: flex; justify-content: center; gap: 20px;">
  <img src="docs/images/one-rom-24-pin-rp2350-rev-a-side.png" alt="One ROM RP2350 24 pin rev A side on" width="400">
</div>

## Documentation

| Topic | Description |
|-------|-------------|
| [Getting Started](docs/GETTING-STARTED.md) | Detailed instructions for flashing your One ROM |
| [Frequently Asked Questions](docs/FAQ.md) | Answers to common questions about One ROM. |
| [Installation](INSTALL.md) | Installation of dependencies. |
| [Programming](docs/PROGRAMMING.md) | How to program One ROM. |
| [Available Configurations](config/README.md#available-configurations) | Various pre-collated ROM collection configurations. |
| [MCU Selection](docs/MCU-SELECTION.md) | How to select the appropriate MCU for your application. |
| [Image Selection](docs/IMAGE-SELECTION.md) | How to tell One ROM which of the installed ROM images to serve. |
| [Image Sets](docs/MULTI-ROM-SETS.md) | How to use a single One ROM to support multiple ROMs simultaneously or to dynamically switch between images. |
| [Configuration](docs/CONFIGURATION.md) | One ROM configuration options. |
| [Runtime Access/Control](docs/RUNTIME-ACCESS.md) | Access and control the One ROM while it is serving ROMs. |
| [Compatibility](docs/COMPATIBILITY.md) | Compatibility with various retro systems. |
| [Logging](docs/LOGGING.md) | How to enable and use One ROM logging. |
| [Technical Summary](docs/TECHNICAL-SUMMARY.md) | Overview of the One ROM architecture, operation and design decisions. |
| [Technical Details](docs/TECHNICAL-DETAILS.md) | Technical details of the One ROM firmware and hardware. |
| [ROMs Glorious ROMs](docs/ROMS-GLORIOUS-ROMS.md) | Everything you ever wanted to know about 23/27 series ROMs but were afraid to ask. |
| [Custom Hardware](docs/CUSTOM-HARDWARE.md) | Guide on designing custom hardware for One ROM. |
| [Build System](docs/BUILD-SYSTEM.md) | How the One ROM build system works. |
| [Voltage Levels](docs/VOLTAGE-LEVELS.md) | How the One ROM supports the required logic voltage levels. |
| [Pi Pico Programmer](docs/PI-PICO-PROGRAMMER.md) | How to use a $5 Raspberry Pi Pico as a programmer for One ROM. |
| [Manufacturing Test](docs/MANUFACTURING-TEST.md) | Information on testing One ROM during manufacturing. |
| [STM32 Clones](docs/STM32-CLONES.md) | Information using STM32 clones with One ROM. |
| [Future Enhancements](docs/FUTURE-ENHANCEMENTS.md) | Possible future enhancements under consideration. |
| [License](LICENSE.md) | One ROM software and hardware licenses. |

## Debugging

The best place to start with debugging is [`Logging`](docs/LOGGING.md).  This will help you see what the One ROM is doing, and why it may not be working as expected.

If you have an [Airfrog](https://piers.rocks/u/airfrog), you can use it to inspect the One ROM firmware and its current state.

If you'd like a hand, raise an issue on the [GitHub issues page](https://github.com/piersfinlayson/software-defined-retro-rom/issues).  Please provide:

- the MCU you are using and the PCB type/revision
- the retro system you are trying to One ROM with
- any logging from One ROM
- ideally, the firmware image, if you are happy to share.

If you have built the firmware yourself, rather than using a pre-built release image, also provide:

- your build configuration
- the output from `make info-detail`.

## License

See [LICENSE](LICENSE.md) for software and hardware licensing information.
