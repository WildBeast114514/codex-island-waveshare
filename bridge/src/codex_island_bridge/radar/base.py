from __future__ import annotations

import re
from datetime import datetime
from typing import Any, Protocol

from ..models import RadarSnapshot


class RadarProviderError(RuntimeError):
    pass


class NotModified(RadarProviderError):
    pass


class RadarProvider(Protocol):
    etag: str | None
    last_modified: str | None

    def fetch(self) -> RadarSnapshot:
        ...


def normalize_key(family: str, effort: str) -> str:
    def part(value: str) -> str:
        return re.sub(r"[^a-z0-9._-]+", "-", value.casefold()).strip("-")

    return f"{part(family)}/{part(effort)}"


def parse_timestamp(value: Any, fallback: int) -> int:
    if isinstance(value, (int, float)):
        return int(value)
    if isinstance(value, str):
        try:
            return int(datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp())
        except ValueError:
            pass
    return fallback
