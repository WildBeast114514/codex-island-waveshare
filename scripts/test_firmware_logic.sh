#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CXX=${CXX:-clang++}
OUTPUT=$(mktemp -t codex-island-button-test)
trap 'rm -f "$OUTPUT"' EXIT

"$CXX" -std=c++17 -Wall -Wextra -Werror \
    -I"$ROOT/firmware/main" \
    "$ROOT/firmware/main/input/button_logic.cpp" \
    "$ROOT/firmware/tests/button_logic_test.cpp" \
    -o "$OUTPUT"
"$OUTPUT"
echo "Firmware host logic tests passed"
