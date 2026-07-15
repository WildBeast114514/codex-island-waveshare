from __future__ import annotations

from ..models import RadarSnapshot
from ..protocol import mock_snapshots


class MockRadarProvider:
    etag: str | None = None
    last_modified: str | None = None

    def fetch(self) -> RadarSnapshot:
        return mock_snapshots()[1]
