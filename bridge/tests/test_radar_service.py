from __future__ import annotations

from pathlib import Path

from codex_island_bridge.config import Settings
from codex_island_bridge.models import RadarModel, RadarSnapshot
from codex_island_bridge.radar import RadarProviderError
from codex_island_bridge.service import RadarService


class Provider:
    etag = "first"
    last_modified = None

    def __init__(self, snapshot: RadarSnapshot | None = None) -> None:
        self.snapshot = snapshot

    def fetch(self) -> RadarSnapshot:
        if self.snapshot is None:
            raise RadarProviderError("fixture failure")
        return self.snapshot


def settings(tmp_path: Path) -> Settings:
    return Settings(
        data_dir=tmp_path,
        codex_auth_path=tmp_path / "auth.json",
        codex_sessions_dir=tmp_path / "sessions",
    )


def snapshot(ts: int) -> RadarSnapshot:
    return RadarSnapshot(
        updated_at=ts,
        models=tuple(
            RadarModel(f"new/{index}", f"New{index}", "max", 900 + index, 6, 10, index)
            for index in range(3)
        ),
    )


def test_failure_retains_cache_and_stale_is_computed(tmp_path: Path) -> None:
    current = 2_000_000_000
    first = RadarService(settings(tmp_path), Provider(snapshot(current)))
    assert first.collect(now=current, force=True).stale is False

    failing = RadarService(settings(tmp_path), Provider())
    retained = failing.collect(now=current + 19 * 3600, force=True)
    assert retained is not None
    assert retained.models[0].family == "New0"
    assert retained.stale is True


def test_history_falls_back_when_preferred_model_is_renamed(tmp_path: Path) -> None:
    current = 2_000_000_000
    service = RadarService(settings(tmp_path), Provider(snapshot(current)))
    collected = service.collect(now=current, force=True)
    assert collected is not None
    assert collected.trend_iq_x10 == (902,)
