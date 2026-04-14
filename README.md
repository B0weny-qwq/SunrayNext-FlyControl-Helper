# SunrayNext-FlyControl-Helper

SunrayNext-FlyControl-Helper is a dual-chip wireless flight-control helper platform built around the ESP32-P4 and ESP32-C5. The repository combines a high-level MAVLink bridge on the P4 side with a customized wireless co-processor firmware on the C5 side.

For Chinese documentation, see [README_cn.md](./README_cn.md).
For the structured development guides, see [docs/README.md](./docs/README.md).

## Overview

The project is split into two firmware targets that work together:

| Component | Target | Role |
| --- | --- | --- |
| [`firmware-p4`](./firmware-p4/) | ESP32-P4 | Main controller, Wi-Fi/UART bridge, console commands, MAVLink forwarding |
| [`firmware-c5`](./firmware-c5/) | ESP32-C5 | Customized `esp-hosted` SDIO slave firmware that provides the wireless link for the P4 |

The repository also includes hardware manufacturing files and physical board assets:

| Directory | Purpose |
| --- | --- |
| [`pcb-design`](./pcb-design/) | PCB manufacturing outputs, BOM, pick-and-place files, and design sources |
| [`hardware-assets`](./hardware-assets/) | Board photos, renders, and mechanical export files |

## Architecture

The P4 firmware receives MAVLink or simulator data over Wi-Fi, then forwards it to a flight controller over UART. The C5 firmware acts as a dedicated wireless networking companion and exposes the Wi-Fi link to the P4 through SDIO.

Typical data flow:

```text
PC / simulator / GCS
        |
     UDP / TCP
        |
   ESP32-P4 bridge
        |
       UART
        |
 Flight controller

ESP32-C5 provides the wireless link for the P4 over SDIO.
```

## Repository Layout

```text
.
├── firmware-p4/
├── firmware-c5/
├── pcb-design/
├── hardware-assets/
├── README.md
└── README_cn.md
```

## Prerequisites

- ESP-IDF v5.3 or newer
- Toolchain support for ESP32-P4 and ESP32-C5
- A serial flashing workflow for both boards
- A compatible ESP32-C5 module and ESP32-P4 board for end-to-end validation

## Quick Start

### 1. Clone the repository

```bash
git clone <repo-url>
cd SunrayNext-FlyControl-Helper
```

### 2. Build the P4 firmware

```bash
cd firmware-p4
idf.py build
```

### 3. Build the C5 firmware

```bash
cd ../firmware-c5
idf.py set-target esp32c5
idf.py build
```

## P4 Firmware Workflow

The P4 project is the application-facing side of the system. It initializes Wi-Fi, sets up the MAVLink bridge, and exposes runtime configuration through an ESP-IDF console.

Key capabilities:

- Wi-Fi station connection management
- UDP/TCP network transport for MAVLink traffic
- UART forwarding to one or more flight-control ports
- Runtime command-line configuration for network and UART behavior

Common workflow:

```bash
cd firmware-p4
idf.py flash monitor
```

Typical console usage:

```text
wifi_set <ssid> <password>
net_type udp
net_target <pc_ip> 8888
uart_en 1 1
```

See [`firmware-p4/README.md`](./firmware-p4/README.md) for the full command set and module breakdown.

## C5 Firmware Workflow

The C5 project is a customized `esp-hosted` firmware configured for SDIO slave operation. It is tailored for a WTP4C5-S1 style module and optimized for a no-PSRAM environment.

Key constraints:

- Keep the provided `sdkconfig` as the baseline
- Preserve the no-PSRAM memory configuration
- Use the SDIO transport configuration expected by the P4 side

Build and flash:

```bash
cd firmware-c5
idf.py set-target esp32c5
idf.py build flash monitor
```

Some C5 boards used with external download adapters do not return to application boot automatically after flashing. If the serial log stays at `waiting for download`, keep the board in download mode only long enough to start flashing, then move `BOOT` to `3.3V` before the final auto-reset so the next reset enters `SPI_FAST_FLASH_BOOT`. The full bring-up note is documented in [docs/guides/c5-flashing-boot-mode.md](./docs/guides/c5-flashing-boot-mode.md).

See [`firmware-c5/README.md`](./firmware-c5/README.md) for integration notes and build cautions.

## Hardware Assets

The hardware-related directories are kept separate from the firmware projects:

- [`pcb-design`](./pcb-design/) contains production and assembly artifacts
- [`hardware-assets`](./hardware-assets/) contains photos and exported board assets

Internal file names in these directories may still contain Chinese text. This repository update only standardizes the top-level directory names.

## Notes

- The C5 firmware includes vendored managed components. Treat them as upstream-managed unless you intentionally need to patch them.
- Do not reset or casually regenerate the C5 `sdkconfig`; the current configuration reflects board-specific memory and transport constraints.
- The README set is intentionally English-first at the root, with a dedicated Chinese mirror in [`README_cn.md`](./README_cn.md).
