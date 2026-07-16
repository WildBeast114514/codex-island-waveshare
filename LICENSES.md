# Third-party notices

This repository contains original integration and UI code plus adaptations of
the following open-source projects. Full reference source trees are deliberately
excluded from Git; exact commits are recorded in `reference/LOCKFILE.md`.

## Waveshare ESP32-S3-Touch-AMOLED-1.75C

- Source: <https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C>
- License: Apache License 2.0.
- Use: `firmware/` began from `examples/esp-idf/02_lvgl_demo_v9`; display,
  CST9217 touch and shared-I2C BSP dependencies remain managed components.
  `firmware/main/power/` adapts AXP2101 register meanings and read behavior
  documented by the upstream `01_AXP2101` example.

## cc-island

- Source: <https://github.com/alexjc-tech/cc-island>
- License: MIT.
- Use: conceptual reference for Codex authentication, session-log accounting,
  newline-delimited JSON and Nordic UART Service. The bridge was restructured
  into this project's Codex-only providers and protocol.

## claude-desktop-buddy-esp32

- Source: <https://github.com/vthinkxie/claude-desktop-buddy-esp32>
- License: MIT.
- Use: board pin/power behavior reference only. Its low-resolution canvas and
  application UI were not copied; this firmware renders directly at 466x466.

## ESP-IDF and LVGL

- ESP-IDF: <https://github.com/espressif/esp-idf>, Apache License 2.0.
- LVGL: <https://github.com/lvgl/lvgl>, MIT License.
- Use: ESP32-S3 runtime/toolchain, NimBLE, NVS, cJSON, FreeRTOS and LVGL 9.5.

## Mambo Codex pet

- Source: <https://codex-pet.com/pets/mambo>
- Sprite sheet: <https://cdn.codex-pet.com/pets/mambo/spritesheet.webp>
- Use: default demonstration asset for the generic build-time WebP pet
  converter. The source WebP is downloaded into an ignored cache after its
  SHA-256 is verified; it is not committed to this repository.
- License: no explicit redistribution license was published with the downloaded
  pet package when integrated. Replace the manifest for redistribution contexts
  that require an explicitly licensed artwork asset.

## Distributed Radar live data

- Page: <https://deng.codexradar.com/>
- Public table endpoint: <https://api.codexradar.com/api/v1/table>
- Use: the macOS Bridge reads the live model/task result grid and independently
  calculates aggregate and effort IQ values. No page code, artwork, or response
  data is bundled in this repository.

Codex and OpenAI names are used only to identify the compatible local service.
This project is independent and is not affiliated with or endorsed by OpenAI,
Waveshare, or CodexRadar.
