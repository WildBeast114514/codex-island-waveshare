from __future__ import annotations

import json
import re

import pytest

from codex_island_bridge.models import (
    PetSnapshot,
    RadarModel,
    RadarSnapshot,
    UsageSnapshot,
)
from codex_island_bridge.protocol import (
    MAX_LINE_BYTES,
    ProtocolError,
    Sequence,
    heartbeat_line,
    pet_line,
    radar_line,
    usage_line,
)


def decoded(line: bytes) -> dict:
    assert line.endswith(b"\n")
    assert len(line) < MAX_LINE_BYTES
    return json.loads(line)


def test_usage_protocol_and_reset_seconds() -> None:
    snapshot = UsageSnapshot(
        updated_at=1_000,
        five_hour_percent=42,
        seven_day_percent=31,
        five_hour_reset_at=1_500,
        today_tokens=486_200,
        today_cost_cents=231,
        daily_tokens=(1, 2, 3, 4, 5, 6, 7),
    )
    message = decoded(usage_line(snapshot, 9, now=1_100))
    assert message == {
        "v": 1,
        "k": "usage",
        "seq": 9,
        "ts": 1_000,
        "p5": 42,
        "p7": 31,
        "reset_s": 400,
        "tok": 486_200,
        "cost_c": 231,
        "daily": [1, 2, 3, 4, 5, 6, 7],
    }


def test_missing_five_hour_is_explicitly_null() -> None:
    message = decoded(usage_line(UsageSnapshot(updated_at=1_000), 1, now=1_100))
    assert message["p5"] is None
    assert message["reset_s"] is None


def test_radar_names_are_dynamic_and_ties_keep_source_order() -> None:
    snapshot = RadarSnapshot(
        updated_at=1_000,
        models=(
            RadarModel("future/b", "FutureName", "b", 1200, 8, 10, 2),
            RadarModel("future/a", "FutureName", "a", 1200, 9, 10, 1),
            RadarModel("next/max", "NextGeneration", "max", 1500, 10, 10, 3),
        ),
    )
    message = decoded(radar_line(snapshot, 2))
    assert re.fullmatch(r"\d{2}-\d{2} \d{2}:\d{2}", message["updated"])
    assert [row[:2] for row in message["models"]] == [
        ["NextGeneration", "max"],
        ["FutureName", "a"],
        ["FutureName", "b"],
    ]


def test_sequence_wraps_without_emitting_zero() -> None:
    sequence = Sequence(0x7FFFFFFF)
    assert sequence.next() == 1


def test_heartbeat_has_no_data_timestamp_side_effects() -> None:
    assert decoded(heartbeat_line(7, now=1_234)) == {
        "v": 1,
        "k": "heartbeat",
        "seq": 7,
        "ts": 1_234,
    }


def test_pet_state_protocol_is_dynamic_and_compact() -> None:
    assert decoded(
        pet_line(
            PetSnapshot(updated_at=1_234, state="waiting", active_tasks=2),
            8,
        )
    ) == {
        "v": 1,
        "k": "pet",
        "seq": 8,
        "ts": 1_234,
        "state": "waiting",
        "active": 2,
    }


def test_unknown_pet_state_is_rejected() -> None:
    snapshot = PetSnapshot(updated_at=1_234, state="dancing", active_tasks=0)  # type: ignore[arg-type]
    with pytest.raises(ProtocolError, match="unknown pet state"):
        pet_line(snapshot, 1)
