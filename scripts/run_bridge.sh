#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export CODEX_RADAR_ALLOW_HTML=${CODEX_RADAR_ALLOW_HTML:-1}
exec "$ROOT/.venv/bin/codex-island-bridge" run
