This directory vendors the repo-owned `common` subset required by `firmware-c5`.

Source baseline:
- ESP-Hosted common headers and sources needed by the C5 SDIO coprocessor build

Scope:
- Keep only the files referenced by `firmware-c5` CMake and includes
- Do not point C5 at `firmware-p4/managed_components` or other cross-project paths
