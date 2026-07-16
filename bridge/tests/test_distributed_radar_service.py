from __future__ import annotations

from pathlib import Path

from codex_island_bridge.config import Settings
from codex_island_bridge.distributed_radar import DistributedRadarError
from codex_island_bridge.models import (
    DistributedRadarRow,
    DistributedRadarSnapshot,
)
from codex_island_bridge.service import DistributedRadarService


class Provider:
    etag = None
    last_modified = None

    def __init__(self, snapshot: DistributedRadarSnapshot | None = None) -> None:
        self.snapshot = snapshot

    def fetch(self) -> DistributedRadarSnapshot:
        if self.snapshot is None:
            raise DistributedRadarError("fixture failure")
        return self.snapshot


def settings(tmp_path: Path) -> Settings:
    return Settings(
        data_dir=tmp_path,
        codex_auth_path=tmp_path / "auth.json",
        codex_sessions_dir=tmp_path / "sessions",
    )


def snapshot(ts: int) -> DistributedRadarSnapshot:
    return DistributedRadarSnapshot(
        updated_at=ts,
        rows=(
            DistributedRadarRow(
                "future/@all", "Future", "", 101, 67, 100, True, 0
            ),
            DistributedRadarRow(
                "future/@max", "Future", "max", 120, 8, 10, False, 1
            ),
        ),
    )


def test_failure_retains_cache_and_marks_old_data_stale(tmp_path: Path) -> None:
    current = 2_000_000_000
    first = DistributedRadarService(settings(tmp_path), Provider(snapshot(current)))
    collected = first.collect(now=current)
    assert collected is not None and collected.stale is False

    failing = DistributedRadarService(settings(tmp_path), Provider())
    retained = failing.collect(now=current + 31 * 60)
    assert retained is not None
    assert retained.rows[0].model == "Future"
    assert retained.stale is True
