# Codex Island Bridge

The bridge keeps Codex credentials and all HTTP access on the Mac. The ESP32
receives only compact, newline-delimited usage, Codex Radar, Distributed Radar,
and pet-state snapshots over Nordic UART Service BLE.

It reads the signed-in Codex account from `~/.codex/auth.json` and incrementally
scans `~/.codex/sessions/**/rollout-*.jsonl`. Credentials never enter the cache,
logs, protocol payload, or device. Usage windows are identified by the API's
reported duration; if the account does not expose a five-hour window, the bridge
sends `p5: null` instead of mislabeling a weekly window.

```bash
python3 -m venv .venv
.venv/bin/pip install -e './bridge[test]'
.venv/bin/codex-island-bridge print
.venv/bin/codex-island-bridge pet-status
.venv/bin/codex-island-bridge devices
.venv/bin/codex-island-bridge distributed-test
.venv/bin/codex-island-bridge once
```

Today cost is an estimate based on the centralized public model price table.
Unknown/private model slugs are reported by name and excluded rather than
silently priced as another model.

Radar never falls back to mock data in normal operation. Configure either an
authorized JSON endpoint with `CODEX_RADAR_API_URL` (and optionally
`CODEX_RADAR_API_TOKEN`) or explicitly enable the low-frequency public-page
parser with `CODEX_RADAR_ALLOW_HTML=1`. The latter is rate-limited to one
request per 30 minutes and normally runs hourly. `CODEX_RADAR_PRIMARY_KEY` can
select the trend model (for example `sol/max`); when a model is renamed or
removed, trend collection automatically uses the highest IQ in each sample.

Distributed Radar reads the public live table used by
<https://deng.codexradar.com/> every five minutes, on the same schedule as
usage. It omits the pooled GPT-family average, calculates each model aggregate
and effort tier as `round(passed / total * 150)`, and preserves the source
order. Model and effort labels are data, so Sol/Terra/Luna and future names do
not require a Bridge or firmware release. Up to 32 rows fit the compact BLE
snapshot. `updated` is the successful Mac fetch time; a failed refresh retains
the atomic cache and marks it stale after 30 minutes. Set
`CODEX_DISTRIBUTED_RADAR_URL` to override the endpoint.

The pet provider reads only lifecycle metadata from recent local Codex session
JSONL files. It checks every two seconds and sends a BLE update only when the
aggregate state or active-task count changes. Asset selection is a firmware
build concern; the Bridge emits generic `idle`, `running`, `waiting`, `review`,
and `failed` states and contains no Mambo-specific logic.
