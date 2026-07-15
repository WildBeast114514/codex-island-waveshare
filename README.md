# Codex Island for Waveshare 1.75C

Native 466x466 LVGL 9 status display for the battery-equipped Waveshare ESP32-S3-Touch-AMOLED-1.75C, backed by a local macOS Codex bridge.

## Hardware baseline status

The connected board was probed on `/dev/cu.usbmodem101` before the first project flash. The results are:

- ESP32-S3 QFN56 revision v0.2, MAC `28:84:85:55:57:0c`;
- 8MB embedded OPI PSRAM;
- 16MB physical flash (`manufacturer 0x20`, `device 0x4018`);
- ESP-IDF v5.5.4 and esptool.py v4.12.0.

The full 16MB factory image is kept locally at `backups/factory-backup-16mb.bin` (ignored by Git). Its SHA-256 is recorded in `backups/factory-backup.sha256`. Re-run the probe and backup with:

```bash
export ESP_PORT="$(scripts/detect_port.sh)"
python -m esptool --chip esp32s3 -p "$ESP_PORT" chip_id
python -m esptool --chip esp32s3 -p "$ESP_PORT" flash_id
scripts/backup_flash.sh
```

`backup_flash.sh` chooses 16MB or 32MB from the detected flash size and writes an untracked image plus SHA-256 digest under `backups/`. Set `ESP_BAUD` to override its default 921600 baud read speed.

## Factory recovery

The preferred recovery artifact is the full flash image created before the first project flash. It can be restored with:

```bash
shasum -a 256 -c backups/factory-backup.sha256
python -m esptool --chip esp32s3 -p "$ESP_PORT" write_flash 0x0 backups/factory-backup-16mb.bin
```

If a full backup cannot be completed, Waveshare also publishes board firmware in the upstream [`Firmware/`](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/tree/main/Firmware) directory. Match the image to the exact 1.75C board revision and follow its flashing notes. Do not substitute flash capacity based on older community repositories; use `flash_id` from the connected board.

## Firmware build

Install ESP-IDF 5.5.4 for the `esp32s3` target. This checkout also supports the ignored local toolchain at `reference/esp-idf-v5.5.4`.

```bash
scripts/build_firmware.sh
```

The final bridge, LaunchAgent and product verification instructions are documented in later implementation phases.

This independent project is not affiliated with or endorsed by OpenAI or Waveshare.
