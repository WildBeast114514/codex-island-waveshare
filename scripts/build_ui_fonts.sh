#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

VERSION="2.304"
ARCHIVE="assets/fonts/.cache/JetBrainsMono-${VERSION}.zip"
ARCHIVE_URL="https://github.com/JetBrains/JetBrainsMono/releases/download/v${VERSION}/JetBrainsMono-${VERSION}.zip"
ARCHIVE_SHA256="6f6376c6ed2960ea8a963cd7387ec9d76e3f629125bc33d1fdcd7eb7012f7bbf"
CACHE_DIR="assets/fonts/.cache"
OUTPUT_DIR="firmware/main/ui/fonts"
CONVERTER_VERSION="1.5.3"
if [[ -n "${LV_FONT_CONV:-}" ]]; then
  CONVERTER=("$LV_FONT_CONV")
else
  CONVERTER=(npx --yes --prefer-offline "lv_font_conv@${CONVERTER_VERSION}")
fi

mkdir -p "$CACHE_DIR" "$OUTPUT_DIR"
if [[ ! -f "$ARCHIVE" ]] || [[ "$(shasum -a 256 "$ARCHIVE" | awk '{print $1}')" != "$ARCHIVE_SHA256" ]]; then
  temporary="${ARCHIVE}.download"
  rm -f "$temporary"
  curl -fsSL --connect-timeout 10 --max-time 90 "$ARCHIVE_URL" -o "$temporary"
  actual="$(shasum -a 256 "$temporary" | awk '{print $1}')"
  if [[ "$actual" != "$ARCHIVE_SHA256" ]]; then
    rm -f "$temporary"
    printf 'JetBrains Mono SHA-256 mismatch: expected %s, got %s\n' \
      "$ARCHIVE_SHA256" "$actual" >&2
    exit 1
  fi
  mv "$temporary" "$ARCHIVE"
fi

unzip -p "$ARCHIVE" fonts/ttf/JetBrainsMono-Medium.ttf \
  > "$CACHE_DIR/JetBrainsMono-Medium.ttf"
unzip -p "$ARCHIVE" fonts/ttf/JetBrainsMono-SemiBold.ttf \
  > "$CACHE_DIR/JetBrainsMono-SemiBold.ttf"

generate_font() {
  local source="$1"
  local size="$2"
  local name="$3"
  "${CONVERTER[@]}" \
    --font "$source" \
    -r 0x20-0x7E \
    --size "$size" \
    --bpp 4 \
    --format lvgl \
    --no-compress \
    --no-kerning \
    --lv-include lvgl.h \
    --lv-font-name "$name" \
    -o "$OUTPUT_DIR/${name}.c"
}

generate_font "$CACHE_DIR/JetBrainsMono-Medium.ttf" 14 jetbrains_mono_14
generate_font "$CACHE_DIR/JetBrainsMono-Medium.ttf" 20 jetbrains_mono_20
generate_font "$CACHE_DIR/JetBrainsMono-SemiBold.ttf" 24 jetbrains_mono_24

printf 'Generated JetBrains Mono ASCII subsets: 14, 20 and 24 px\n'
