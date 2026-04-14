# C5 Firmware

The C5 firmware is a customized `esp-hosted` co-processor firmware for ESP32-C5. It is configured as an SDIO slave and is intended to provide the wireless networking link used by the ESP32-P4 side of this project.

## Purpose

This firmware turns the ESP32-C5 into a networking companion for the P4 controller. The repository keeps the upstream `esp-hosted` structure, then layers board-specific configuration and memory constraints on top of it.

## Key Customizations

- Targeted for an ESP32-C5 module similar to WTP4C5-S1
- Tuned for a no-PSRAM environment
- Configured for SDIO transport
- Trimmed to preserve SRAM for networking buffers and runtime stability

## Project Layout

| Path | Responsibility |
| --- | --- |
| `main/esp_hosted_coprocessor.c` | Main co-processor startup flow |
| `main/` | Transport, Wi-Fi, control, and support modules |
| `dependencies.lock` | Locked ESP-IDF component versions for reproducible restores |
| `sdkconfig` | Active project configuration that should be preserved |
| `partitions.esp32c5.csv` | Active flash partition layout for the C5 target |

## Build and Flash

Use the checked-in project configuration and build for ESP32-C5:

```bash
idf.py set-target esp32c5
idf.py build
idf.py flash monitor
```

## Boot-Mode Note For External Download Adapters

Some C5 boards wired through an external downloader can be flashed successfully but still fall back to:

```text
boot:0x28 (DOWNLOAD(UART0/USB))
waiting for download
```

When that happens, the problem is not yet in the application. The chip is still selecting download mode at reset time.

The validated workaround for this repository is:

1. Start flashing with `BOOT` wired so the chip can enter download mode.
2. After flashing has already entered the write phase, move `BOOT` to `3.3V`.
3. Let `idf.py flash monitor` finish normally.
4. The final auto-reset will then sample `BOOT` high and continue with `SPI_FAST_FLASH_BOOT`.

This behavior is board-specific. The detailed Chinese bring-up note is in [`docs/guides/c5-flashing-boot-mode.md`](../docs/guides/c5-flashing-boot-mode.md).

## Configuration Cautions

This project depends on the checked-in `sdkconfig`, `sdkconfig.defaults`, and `sdkconfig.defaults.esp32c5`. Do not casually reset or regenerate the configuration.

Important constraints:

- Preserve the no-PSRAM memory profile
- Preserve the SDIO transport selection
- Keep the board-specific flash and partition assumptions intact

If these settings drift, initialization failures such as memory allocation errors can appear during boot.

## Integration Notes

- The P4 side depends on this firmware to provide the wireless link over SDIO.
- `managed_components/` is a generated restore directory and can be recreated from `idf_component.yml` plus `dependencies.lock`.
- When changing transport, memory, or Wi-Fi behavior, validate the combined P4+C5 system rather than only this firmware in isolation.

## When You Need to Change This Firmware

Typical reasons to update this project include:

- adapting to a different ESP32-C5 module variant
- tuning memory usage for a tighter runtime profile
- changing transport-related settings
- patching the co-processor behavior used by the P4 host side

When making those changes, document them carefully and keep the board assumptions explicit.
