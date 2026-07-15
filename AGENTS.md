# Codex Island contributor notes

- The firmware target is ESP32-S3 and the production framework is ESP-IDF 5.5.4.
- Keep rendering native at 466x466. Do not introduce a lower-resolution canvas or scaled framebuffer.
- Keep credentials on the Mac bridge. The firmware accepts computed usage and radar data only.
- Preserve the Waveshare BSP display lock around every LVGL mutation made outside the LVGL task.
- Run `scripts/build_firmware.sh` and the bridge test suite before committing a phase.
- Never flash before `scripts/backup_flash.sh` has completed, unless the operator explicitly sets `SKIP_FACTORY_BACKUP=1` after confirming another recovery image.

