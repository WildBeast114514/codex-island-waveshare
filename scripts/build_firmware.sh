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

PET_MANIFEST="${CODEX_PET_MANIFEST:-$ROOT/assets/pets/mambo.json}"
PET_OUTPUT="$ROOT/firmware/generated/pet_asset.bin"
PET_PYTHON="${PET_PYTHON:-$ROOT/.venv/bin/python}"
if [[ ! -x "$PET_PYTHON" ]]; then
  PET_PYTHON="${PYTHON:-python3}"
fi
"$PET_PYTHON" "$ROOT/scripts/build_pet_asset.py" \
  --manifest "$PET_MANIFEST" \
  --output "$PET_OUTPUT"

if [[ ! -f "$ROOT/firmware/sdkconfig" ]] || ! grep -q '^CONFIG_IDF_TARGET="esp32s3"$' "$ROOT/firmware/sdkconfig"; then
  idf.py -C "$ROOT/firmware" set-target esp32s3
fi
idf.py -C "$ROOT/firmware" build
