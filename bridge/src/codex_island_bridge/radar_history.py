from __future__ import annotations

from datetime import datetime, timedelta
from typing import Any

from .cache import AtomicJsonFile
from .models import RadarSnapshot
from .radar.base import normalize_key


class RadarHistory:
    def __init__(self, cache: AtomicJsonFile, preferred_key: str | None = None) -> None:
        self.cache = cache
        self.preferred_key = preferred_key or normalize_key("Sol", "max")

    def _samples(self) -> list[dict[str, Any]]:
        loaded = self.cache.load(default=[])
        return loaded if isinstance(loaded, list) else []

    @staticmethod
    def _mapping(snapshot: RadarSnapshot) -> dict[str, int]:
        return {model.key: model.iq_x10 for model in snapshot.models}

    def record(self, snapshot: RadarSnapshot) -> None:
        samples = self._samples()
        mapping = self._mapping(snapshot)
        if not samples or samples[-1].get("models") != mapping:
            samples.append({"ts": snapshot.updated_at, "models": mapping})
        cutoff = int((datetime.now().astimezone() - timedelta(days=90)).timestamp())
        samples = [sample for sample in samples if int(sample.get("ts", 0)) >= cutoff][-500:]
        self.cache.save(samples)

    def trend(self, limit: int = 12) -> tuple[int, ...]:
        values: list[int] = []
        for sample in self._samples():
            models = sample.get("models")
            if not isinstance(models, dict) or not models:
                continue
            value = models.get(self.preferred_key)
            if not isinstance(value, int):
                numeric = [item for item in models.values() if isinstance(item, int)]
                value = max(numeric) if numeric else None
            if isinstance(value, int):
                values.append(value)
        return tuple(values[-limit:])
