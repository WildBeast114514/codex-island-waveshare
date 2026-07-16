from __future__ import annotations

from dataclasses import dataclass, field
from typing import Literal


@dataclass(frozen=True, slots=True)
class UsageSnapshot:
    updated_at: int
    five_hour_percent: int | None = None
    seven_day_percent: int | None = None
    five_hour_reset_at: int | None = None
    today_tokens: int = 0
    today_cost_cents: int = 0
    daily_tokens: tuple[int, ...] = (0, 0, 0, 0, 0, 0, 0)
    plan: str | None = None


@dataclass(frozen=True, slots=True)
class RadarModel:
    """A dynamic Radar row; names are data, never a firmware enum."""

    key: str
    family: str
    effort: str
    iq_x10: int
    passed: int
    total: int
    source_order: int = 0


@dataclass(frozen=True, slots=True)
class RadarSnapshot:
    updated_at: int
    models: tuple[RadarModel, ...] = field(default_factory=tuple)
    stale: bool = False
    trend_iq_x10: tuple[int, ...] = field(default_factory=tuple)


PetActivity = Literal["idle", "running", "waiting", "review", "failed"]


@dataclass(frozen=True, slots=True)
class PetSnapshot:
    updated_at: int
    state: PetActivity = "idle"
    active_tasks: int = 0
