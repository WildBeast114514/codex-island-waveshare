#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -z "${IDF_PATH:-}" ]]; then
  if [[ -f "$ROOT/reference/esp-idf-v5.5.4/export.sh" ]]; then
    # shellcheck disable=SC1091
    source "$ROOT/reference/esp-idf-v5.5.4/export.sh" >/dev/null
  else
    printf 'ESP-IDF is not active and reference/esp-idf-v5.5.4/export.sh is absent.\n' >&2
    exit 2
  fi
fi

idf.py -C "$ROOT/firmware" set-target esp32s3
idf.py -C "$ROOT/firmware" build

