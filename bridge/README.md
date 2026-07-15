# Codex Island Bridge

The bridge keeps Codex credentials and all HTTP access on the Mac. The ESP32
receives only compact, newline-delimited usage and radar snapshots over Nordic
UART Service BLE.

It reads the signed-in Codex account from `~/.codex/auth.json` and incrementally
scans `~/.codex/sessions/**/rollout-*.jsonl`. Credentials never enter the cache,
logs, protocol payload, or device. Usage windows are identified by the API's
reported duration; if the account does not expose a five-hour window, the bridge
sends `p5: null` instead of mislabeling a weekly window.

```bash
python3 -m venv .venv
.venv/bin/pip install -e './bridge[test]'
.venv/bin/codex-island-bridge print
.venv/bin/codex-island-bridge devices
.venv/bin/codex-island-bridge once
```

Today cost is an estimate based on the centralized public model price table.
Unknown/private model slugs are reported by name and excluded rather than
silently priced as another model.
