from __future__ import annotations

import json
from datetime import date, datetime
from pathlib import Path

from codex_island_bridge.codex_sessions import CodexSessionAggregator, estimate_cost_microusd


def event(timestamp: str, input_tokens: int, cached: int, output: int) -> str:
    return json.dumps(
        {
            "timestamp": timestamp,
            "type": "event_msg",
            "payload": {
                "type": "token_count",
                "info": {
                    "last_token_usage": {
                        "input_tokens": input_tokens,
                        "cached_input_tokens": cached,
                        "output_tokens": output,
                    }
                },
            },
        }
    )


def test_cost_conversion_is_in_microusd() -> None:
    assert estimate_cost_microusd("gpt-5-codex", 1_000_000, 0, 1_000_000) == 11_250_000


def test_local_day_buckets_bad_lines_and_incremental_scan(tmp_path: Path) -> None:
    sessions = tmp_path / "sessions"
    sessions.mkdir()
    rollout = sessions / "rollout-test.jsonl"
    local_noon = datetime(2026, 7, 15, 12, 0).astimezone().isoformat()
    rollout.write_text(
        "\n".join(
            [
                json.dumps({"type": "turn_context", "payload": {"model": "gpt-5-codex"}}),
                "not json",
                event(local_noon, 100, 40, 20),
            ]
        )
        + "\n"
    )
    aggregator = CodexSessionAggregator(sessions, tmp_path / "index.json")
    first = aggregator.collect(today=date(2026, 7, 15))
    assert first.today_tokens == 120
    assert first.daily_tokens[-1] == 120
    assert first.today_cost_cents >= 0

    with rollout.open("a") as handle:
        handle.write(event(local_noon, 50, 0, 10) + "\n")
    second = aggregator.collect(today=date(2026, 7, 15))
    assert second.today_tokens == 180
