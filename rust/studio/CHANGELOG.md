# Changelog

## v0.1.11 - 2026-02-3

- Add some keyboard shortcuts to One ROM Studio:
  - r: Rescan devices
  - f: Flash firmware
- Added 231024 support.

## v0.1.9 - 2026-01-22

### Fixed

- Fixed #90 in v0.6.0 when older (pre v0.1.8) versions of One ROM Studio are used to build firmware images with more than one ROM set, One ROM will not boot on any ROM set other than ROM set 0.
- 2732 ROM type serving was broken - the top 2K replicated the bottom 2K.  Fixed (#103).  This included fixing the testing, which had also not caught this issue.

## v0.1.8 - 2026-01-14

### Added

- Support for low level config of firmware at runtime using JSON files #87.

## v0.1.7 - 2026-01-03

### Added

- #77 - Support for serving multi-ROM sets using Fire PIO algorithm.

## v0.1.6 - 2026-01-01

### Added

- 231024 ROM support in JSON config files.
- fire-24-c
- ice-24-i

## v0.1.5 - 2025-12-12

### Added

- fire-28-a

## v0.1.4 - 2025-12-09

### Added

- Support for local files to onerom-fw and onerom-studio.

### Fixed

- Set Windows PE file and product versions to match Cargo version.

## v0.1.3 - 2025-11-24

### Fixed

- Panic (crash) when analyzing a Fire with a debug probe.  Moved to fork of probe-rs with fix for panic.
- Allow multi-rom sets to be built
- Statically link with vcruntime

## v0.1.2 - 2025-11-09

- Built with rustc 1.91
- Move to probe-rs 0.30
- Added ability to load ROM config JSON files from disk in Create view
- Added online manifest to access latest URLs, with local cache file backup, and defaults as further backup
- Added app version update check and download link

## v0.1.1 - 2025-10-30

- Built with rustc 1.90
- Mac and Windows releases now signed.
- Mac app now uses the One ROM liquid glass icon.
- Moved to libusb-less DFU implementation using `dfu-rs` and `nusb` crates.
- Moved to manual rescanning to detect probes and USB devices.
- Added network connectivity icon.
- Single universal macOS dmg installer instead of separate Intel and Apple Silicon versions.
- Added ability to load ROM config JSON files from disk in Create view.
