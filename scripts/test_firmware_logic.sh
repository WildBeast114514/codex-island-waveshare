#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CXX=${CXX:-clang++}
OUTPUT_DIR=$(mktemp -d -t codex-island-firmware-tests)
trap 'rm -rf "$OUTPUT_DIR"' EXIT

"$CXX" -std=c++17 -Wall -Wextra -Werror \
    -I"$ROOT/firmware/main" \
    "$ROOT/firmware/main/input/button_logic.cpp" \
    "$ROOT/firmware/tests/button_logic_test.cpp" \
    -o "$OUTPUT_DIR/button_logic_test"
"$OUTPUT_DIR/button_logic_test"

"$CXX" -std=c++17 -Wall -Wextra -Werror \
    -I"$ROOT/firmware/main" \
    "$ROOT/firmware/tests/link_watchdog_test.cpp" \
    -o "$OUTPUT_DIR/link_watchdog_test"
"$OUTPUT_DIR/link_watchdog_test"
echo "Firmware host logic tests passed"
