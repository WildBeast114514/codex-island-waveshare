#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${ESP_PORT:-}"
if [[ -z "$PORT" ]]; then
  PORT="$($ROOT/scripts/detect_port.sh)"
fi

if ! compgen -G "$ROOT/backups/factory-backup-*.bin" >/dev/null && [[ "${SKIP_FACTORY_BACKUP:-0}" != 1 ]]; then
  printf 'No factory backup found; running backup before first flash.\n' >&2
  ESP_PORT="$PORT" "$ROOT/scripts/backup_flash.sh"
fi

if [[ -z "${IDF_PATH:-}" && -f "$ROOT/reference/esp-idf-v5.5.4/export.sh" ]]; then
  # shellcheck disable=SC1091
  source "$ROOT/reference/esp-idf-v5.5.4/export.sh" >/dev/null
fi

idf.py -C "$ROOT/firmware" -p "$PORT" flash monitor
