#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${ESP_PORT:-}" ]]; then
  if [[ -c "$ESP_PORT" ]]; then
    printf '%s\n' "$ESP_PORT"
    exit 0
  fi
  printf 'ESP_PORT does not name a character device: %s\n' "$ESP_PORT" >&2
  exit 2
fi

shopt -s nullglob
candidates=(
  /dev/cu.usbmodem*
  /dev/cu.wchusbserial*
  /dev/cu.SLAB_USBtoUART*
  /dev/cu.usbserial*
)

if (( ${#candidates[@]} == 0 )); then
  printf 'No ESP-compatible serial port found. Connect the board over a data-capable USB-C cable.\n' >&2
  exit 2
fi

if (( ${#candidates[@]} > 1 )); then
  printf 'Multiple candidate serial ports found; set ESP_PORT explicitly:\n' >&2
  printf '  %s\n' "${candidates[@]}" >&2
  exit 3
fi

printf '%s\n' "${candidates[0]}"

