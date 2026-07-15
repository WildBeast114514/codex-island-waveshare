from __future__ import annotations

import json
import time
from dataclasses import dataclass
from datetime import datetime
from typing import Any

from .models import RadarSnapshot, UsageSnapshot

PROTOCOL_VERSION = 1
MAX_LINE_BYTES = 2048


class ProtocolError(ValueError):
    pass


@dataclass(slots=True)
class Sequence:
    value: int = 0

    def next(self) -> int:
        self.value = (self.value + 1) & 0x7FFFFFFF
        if self.value == 0:
            self.value = 1
        return self.value


def _line(message: dict[str, Any]) -> bytes:
    encoded = (json.dumps(message, ensure_ascii=False, separators=(",", ":")) + "\n").encode(
        "utf-8"
    )
    if len(encoded) > MAX_LINE_BYTES:
        raise ProtocolError(f"BLE protocol line is {len(encoded)} bytes; max is {MAX_LINE_BYTES}")
    return encoded


def usage_line(snapshot: UsageSnapshot, seq: int, *, now: int | None = None) -> bytes:
    current = int(time.time()) if now is None else now
    if snapshot.five_hour_percent is None or snapshot.five_hour_reset_at is None:
        reset_seconds: int | None = None
    else:
        reset_seconds = max(0, snapshot.five_hour_reset_at - current)
    return _line(
        {
            "v": PROTOCOL_VERSION,
            "k": "usage",
            "seq": seq,
            "ts": snapshot.updated_at,
            "p5": snapshot.five_hour_percent,
            "p7": snapshot.seven_day_percent,
            "reset_s": reset_seconds,
            "tok": snapshot.today_tokens,
            "cost_c": snapshot.today_cost_cents,
            "daily": list(snapshot.daily_tokens),
        }
    )


def radar_line(snapshot: RadarSnapshot, seq: int) -> bytes:
    ordered = sorted(snapshot.models, key=lambda model: (-model.iq_x10, model.source_order))
    updated = datetime.fromtimestamp(snapshot.updated_at).astimezone().strftime("%m-%d %H:%M")
    return _line(
        {
            "v": PROTOCOL_VERSION,
            "k": "radar",
            "seq": seq,
            "ts": snapshot.updated_at,
            "updated": updated,
            "stale": snapshot.stale,
            "models": [
                [model.family, model.effort, model.iq_x10, model.passed, model.total]
                for model in ordered
            ],
            "trend": list(snapshot.trend_iq_x10[-12:]),
        }
    )


def mock_snapshots(now: int | None = None) -> tuple[UsageSnapshot, RadarSnapshot]:
    from .models import RadarModel, RadarSnapshot

    current = int(time.time()) if now is None else now
    usage = UsageSnapshot(
        updated_at=current,
        five_hour_percent=42,
        seven_day_percent=31,
        five_hour_reset_at=current + 4980,
        today_tokens=486_200,
        today_cost_cents=231,
        daily_tokens=(15_200, 83_000, 47_000, 92_000, 66_000, 101_000, 486_200),
    )
    radar = RadarSnapshot(
        updated_at=current,
        models=(
            RadarModel("sol/max", "Sol", "max", 1200, 8, 10, 0),
            RadarModel("sol/xhigh", "Sol", "xhigh", 1500, 10, 10, 1),
            RadarModel("terra/max", "Terra", "max", 1050, 7, 10, 2),
            RadarModel("luna/max", "Luna", "max", 900, 6, 10, 3),
        ),
        stale=False,
        trend_iq_x10=(900, 1050, 1050, 1200),
    )
    return usage, radar
