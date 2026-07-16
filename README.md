# Codex Island for Waveshare 1.75C

Native 466x466 LVGL 9 status display for the Waveshare ESP32-S3-Touch-AMOLED-1.75C, backed by a local macOS Codex bridge. It supports both the USB-only and battery-equipped board variants.

The three touch pages show Codex usage, dynamic CodexRadar model IQ, and link/power/trend status. Credentials and network requests stay on the Mac; the ESP32 receives only compact derived snapshots over BLE Nordic UART Service.

## Hardware baseline status

The connected board was probed on `/dev/cu.usbmodem101` before the first project flash. The results are:

- ESP32-S3 QFN56 revision v0.2, MAC `28:84:85:55:57:0c`;
- 8MB embedded OPI PSRAM;
- 16MB physical flash (`manufacturer 0x20`, `device 0x4018`);
- ESP-IDF v5.5.4 and esptool.py v4.12.0.

This particular tested unit has no battery fitted. AXP2101 was detected and
reported `USB present`, `battery absent`, which is displayed as USB power rather
than a misleading empty battery. Battery percentage, charging, dimming, low
battery color and 120-second screen-off behavior remain enabled for battery
variants.

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
scripts/test_firmware_logic.sh
```

Flash and monitor the automatically detected serial port with:

```bash
export ESP_PORT="$(scripts/detect_port.sh)"
scripts/flash_firmware.sh
```

Set `MONITOR=1` when running the flash script from an interactive terminal to
continue into the serial monitor.

The firmware starts with the last valid Usage/Radar snapshot from NVS, then
advertises as `Codex Island-XXXX`. It never stores Codex credentials. The
tested unit advertises as `Codex Island-570E`.

## macOS Bridge

Codex must already be signed in (`~/.codex/auth.json`). Install the isolated
Python environment and inspect real local data:

```bash
scripts/bootstrap_macos.sh
.venv/bin/codex-island-bridge print
.venv/bin/codex-island-bridge devices
CODEX_RADAR_ALLOW_HTML=1 .venv/bin/codex-island-bridge radar-test
.venv/bin/codex-island-bridge once
.venv/bin/pytest -q bridge/tests
```

The background Bridge collects and pushes Codex usage every 300 seconds and
Radar every 3600 seconds. It also sends a data-neutral BLE heartbeat every 60
seconds; heartbeats do not change the Status page's `Last sync` time. If an
apparently connected central sends no application traffic for 180 seconds, the
firmware terminates the stale link and resumes advertising. This recovers the
half-open CoreBluetooth state that can otherwise remain after a Mac sleeps.
The intervals can be overridden with `CODEX_USAGE_INTERVAL`,
`CODEX_RADAR_INTERVAL`, and `CODEX_HEARTBEAT_INTERVAL`. CoreBluetooth scan,
connect, subscription, write, and disconnect operations also have hard
deadlines (`CODEX_BLE_IO_TIMEOUT`, 10 seconds by default). A timed-out GATT
operation exits the Bridge so the `KeepAlive` LaunchAgent restarts it with a
clean CoreBluetooth process instead of remaining falsely `running` after wake.

The usage endpoint's windows are classified by `limit_window_seconds`, not by
their `primary`/`secondary` position. If the service does not expose a 5-hour
window, the left ring says `N/A / limit not reported`; a weekly primary window
is still shown correctly in the 7-day ring. Today token and cost are computed
incrementally from local session logs. Cost is an estimate; unknown/private
model slugs are excluded and logged without exposing any credential.

Radar Provider priority is authorized JSON (`CODEX_RADAR_API_URL`), explicitly
enabled HTML (`CODEX_RADAR_ALLOW_HTML=1`), then no data. Mock is never used as a
silent production fallback. Family/effort labels are data, so current names such
as Sol/Terra/Luna may change without a firmware update. Trend selection can be
overridden with `CODEX_RADAR_PRIMARY_KEY`; a missing/renamed key falls back to
the highest IQ in each historical sample.

## Login autostart

Install and immediately start the user LaunchAgent:

```bash
scripts/install_launch_agent.sh
launchctl print "gui/$(id -u)/com.local.codex-island-bridge"
```

Logs are under `~/Library/Logs/CodexIsland/`. Atomic caches are under
`~/Library/Application Support/CodexIsland/`. The installer also places a
non-editable Python runtime there because macOS prevents background agents from
reading projects under Desktop without a separate Files and Folders grant.
Grant Bluetooth permission to the Python process if macOS asks. Uninstall with:

```bash
launchctl bootout "gui/$(id -u)/com.local.codex-island-bridge"
rm ~/Library/LaunchAgents/com.local.codex-island-bridge.plist
rm -rf ~/Library/Application\ Support/CodexIsland/bridge-runtime
```

## Controls and power policy

- Swipe left/right or tap a page dot to switch Usage, Radar and Status.
- Scroll up/down inside Radar to browse every IQ model received (up to 24);
  horizontal swipes still switch pages. The header shows the source data's
  Mac-local update time.
- BOOT short press advances a page; double press requests immediate refresh.
- PWR short press toggles display brightness; PMIC long-press behavior is left intact.
- USB: 35% brightness, page rotation every 15 seconds after 60 seconds without interaction, and a 1–2 pixel shift every five minutes.
- Battery: dim after 30 seconds and turn the AMOLED off after 120 seconds; touch or a button wakes it.

## Data and cache behavior

Bridge JSON writes use temporary files, `fsync`, and atomic rename. Radar HTML
is limited to one request per 30 minutes (normally hourly), uses conditional
HTTP headers when available, and becomes stale after 18 hours. Failed network
or parse attempts retain the last valid snapshot. ESP32 NVS writes only when
Usage, Radar, page, or brightness content changes.

Third-party sources and licenses are documented in `LICENSES.md` and locked in
`reference/LOCKFILE.md`.

This independent project is not affiliated with or endorsed by OpenAI or Waveshare.
