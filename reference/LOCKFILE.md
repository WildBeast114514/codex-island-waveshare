# Reference lockfile

Resolved on 2026-07-15. The source trees are intentionally excluded from Git.

| Purpose | Repository | Commit |
|---|---|---|
| Waveshare BSP, LVGL and PMU examples | `https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C` | `0349e331567b3fb33752abc70c9d1ce12f7418e5` |
| Codex usage/session/BLE reference | `https://github.com/alexjc-tech/cc-island` | `f749756be8bfcaad65f5dbc7dc592f498db145e8` |
| 1.75C board behavior reference | `https://github.com/vthinkxie/claude-desktop-buddy-esp32` | `61a0ce9f7410ed87de2032f226e511c9e59abbfe` |
| ESP-IDF 5.5.4 | `https://github.com/espressif/esp-idf` | `735507283d5b2f9fb363a1901172dbd9e847945d` |
| LVGL 9.5.0 | `https://github.com/lvgl/lvgl` | `85aa60d18b3d5e5588d7b247abf90198f07c8a63` |

The default Mambo sprite sheet is locked separately by content hash because it
is distributed as a CDN asset rather than a Git repository:

| Asset | URL | SHA-256 |
|---|---|---|
| Mambo WebP sprite sheet | `https://cdn.codex-pet.com/pets/mambo/spritesheet.webp` | `bf40a1330a1151457516cc51ca76cb10097920cae07becc21d5882c8d84a1977` |

The baseline firmware was copied from `examples/esp-idf/02_lvgl_demo_v9` at the locked Waveshare commit. A clean host build completed with ESP-IDF 5.5.4 on Apple Silicon before project-specific changes.
