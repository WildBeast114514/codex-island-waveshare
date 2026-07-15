#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
USER_HOME=$(dscl . -read "/Users/$(id -un)" NFSHomeDirectory | awk '{print $2}')
LABEL=com.local.codex-island-bridge
DOMAIN="gui/$(id -u)"
DEST="$USER_HOME/Library/LaunchAgents/$LABEL.plist"
LOG_DIR="$USER_HOME/Library/Logs/CodexIsland"
DATA_DIR="$USER_HOME/Library/Application Support/CodexIsland"
RUNTIME="$DATA_DIR/bridge-runtime"
PYTHON=${PYTHON:-python3}

"$ROOT/scripts/bootstrap_macos.sh"
mkdir -p "$USER_HOME/Library/LaunchAgents" "$LOG_DIR" "$DATA_DIR"

# A login LaunchAgent cannot read projects under Desktop/Documents unless it is
# separately granted Files and Folders permission. Install a non-editable copy
# under Application Support so login autostart works without that permission.
if [ ! -x "$RUNTIME/bin/python" ]; then
    "$PYTHON" -m venv "$RUNTIME"
fi
"$RUNTIME/bin/python" -m pip install --upgrade pip
"$RUNTIME/bin/python" -m pip install --upgrade --force-reinstall "$ROOT/bridge"

sed -e "s|__RUNTIME__|$RUNTIME|g" \
    -e "s|__DATA_DIR__|$DATA_DIR|g" \
    -e "s|__HOME__|$USER_HOME|g" \
    "$ROOT/launchd/$LABEL.plist.template" > "$DEST"
plutil -lint "$DEST"
launchctl bootout "$DOMAIN/$LABEL" >/dev/null 2>&1 || true
launchctl bootstrap "$DOMAIN" "$DEST"
launchctl enable "$DOMAIN/$LABEL"
launchctl kickstart -k "$DOMAIN/$LABEL"
echo "Installed and started $DOMAIN/$LABEL"
echo "Runtime: $RUNTIME"
echo "Logs: $LOG_DIR"
