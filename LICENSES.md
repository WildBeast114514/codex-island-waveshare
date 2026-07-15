# Third-party licenses and provenance

This project contains an adapted Waveshare example and refers to, but does not vendor, the other source trees listed below. Exact commits are in `reference/LOCKFILE.md`.

| Material | Project paths | Upstream | License | Adaptation |
|---|---|---|---|---|
| ESP-IDF LVGL example | `firmware/CMakeLists.txt`, `firmware/partitions.csv`, `firmware/sdkconfig.defaults`, initial `firmware/main/*` | Waveshare ESP32-S3-Touch-AMOLED-1.75C | Apache-2.0 | Converted from demo entry point into the Codex dashboard while retaining the BSP setup. |
| BLE NUS design and Codex provider ideas | `firmware/main/transport/*`, `bridge/src/codex_island_bridge/*` | alexjc-tech/cc-island | MIT | Reimplemented for a 2048-byte newline JSON protocol and Codex-only service. |
| 1.75C PMU/button behavior | `firmware/main/power/*`, `firmware/main/input/*` | vthinkxie/claude-desktop-buddy-esp32 | MIT | Used as behavioral reference only; no low-resolution canvas code is used. |
| LVGL | managed ESP-IDF component | LVGL | MIT | Unmodified dependency, fixed at 9.5.0. |
| ESP-IDF | external toolchain/components | Espressif | Apache-2.0 | Unmodified dependency, fixed at 5.5.4. |

Codex and OpenAI names are used only to identify the service. This is an independent project and is not endorsed by OpenAI. No OpenAI logo is included.

