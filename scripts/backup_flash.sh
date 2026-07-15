#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${ESP_PORT:-}"
if [[ -z "$PORT" ]]; then
  PORT="$($ROOT/scripts/detect_port.sh)"
fi

if [[ -z "${IDF_PATH:-}" && -f "$ROOT/reference/esp-idf-v5.5.4/export.sh" ]]; then
  # shellcheck disable=SC1091
  source "$ROOT/reference/esp-idf-v5.5.4/export.sh" >/dev/null
fi

PYTHON="${IDF_PYTHON_ENV_PATH:+$IDF_PYTHON_ENV_PATH/bin/python}"
PYTHON="${PYTHON:-python3}"
mkdir -p "$ROOT/backups"

"$PYTHON" -m esptool --chip esp32s3 -p "$PORT" chip_id
flash_info="$($PYTHON -m esptool --chip esp32s3 -p "$PORT" flash_id 2>&1)"
printf '%s\n' "$flash_info"

case "$flash_info" in
  *"Detected flash size: 32MB"*) size_hex=0x2000000; suffix=32mb ;;
  *"Detected flash size: 16MB"*) size_hex=0x1000000; suffix=16mb ;;
  *)
    printf 'Unsupported or undetected flash capacity; refusing to guess backup length.\n' >&2
    exit 4
    ;;
esac

image="$ROOT/backups/factory-backup-$suffix.bin"
"$PYTHON" -m esptool --chip esp32s3 -p "$PORT" read_flash 0x0 "$size_hex" "$image"
shasum -a 256 "$image" > "$ROOT/backups/factory-backup.sha256"
printf 'Factory flash backup: %s\n' "$image"
printf 'Digest: %s\n' "$ROOT/backups/factory-backup.sha256"

