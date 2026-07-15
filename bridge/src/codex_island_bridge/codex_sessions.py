from __future__ import annotations

import json
import logging
import re
from dataclasses import dataclass
from datetime import date, datetime, timedelta
from pathlib import Path
from typing import Any

from .cache import AtomicJsonFile

LOGGER = logging.getLogger(__name__)

# USD per million tokens: input, output, cached input. These are estimates,
# centralized so private/renamed Codex model slugs can be configured without
# changing the scanner. Current public rates are from the official model pages
# at developers.openai.com/api/docs/models; older entries preserve cc-island's
# MIT-licensed bridge behavior.
DEFAULT_PRICING: dict[str, tuple[float, float, float]] = {
    "gpt-5.6-sol": (5.0, 30.0, 0.50),
    "gpt-5.6": (5.0, 30.0, 0.50),
    "gpt-5.6-terra": (2.5, 15.0, 0.25),
    "gpt-5.6-luna": (1.0, 6.0, 0.10),
    "gpt-5.5": (5.0, 30.0, 0.50),
    "gpt-5.4": (2.5, 15.0, 0.25),
    "gpt-5.4-mini": (0.75, 4.5, 0.075),
    "gpt-5.2": (1.75, 14.0, 0.175),
    "gpt-5.3-codex": (1.75, 14.0, 0.175),
    "gpt-5-codex": (1.25, 10.0, 0.125),
}

LONG_CONTEXT_MODELS = {
    "gpt-5.6-sol",
    "gpt-5.6",
    "gpt-5.6-terra",
    "gpt-5.6-luna",
    "gpt-5.5",
    "gpt-5.4",
}


@dataclass(frozen=True, slots=True)
class SessionStats:
    today_tokens: int
    today_cost_cents: int
    daily_tokens: tuple[int, ...]
    unknown_models: tuple[str, ...] = ()


def _local_date(timestamp: Any) -> date | None:
    if not isinstance(timestamp, str):
        return None
    try:
        parsed = datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
    except ValueError:
        return None
    return parsed.astimezone().date()


def _canonical_model(raw: str) -> str:
    return re.sub(r"-\d{8}$", "", raw)


def estimate_cost_microusd(
    model: str,
    input_tokens: int,
    cached_input_tokens: int,
    output_tokens: int,
    pricing: dict[str, tuple[float, float, float]] | None = None,
) -> int | None:
    canonical = _canonical_model(model)
    rates = (pricing or DEFAULT_PRICING).get(canonical)
    if rates is None:
        return None
    noncached = max(0, input_tokens - cached_input_tokens)
    input_multiplier = 2.0 if canonical in LONG_CONTEXT_MODELS and input_tokens > 272_000 else 1.0
    output_multiplier = 1.5 if input_multiplier > 1.0 else 1.0
    usd = (
        noncached * rates[0] * input_multiplier
        + output_tokens * rates[1] * output_multiplier
        + cached_input_tokens * rates[2] * input_multiplier
    ) / 1_000_000
    return round(usd * 1_000_000)


class CodexSessionAggregator:
    def __init__(
        self,
        sessions_dir: Path,
        cache_path: Path,
        *,
        pricing: dict[str, tuple[float, float, float]] | None = None,
    ) -> None:
        self.sessions_dir = sessions_dir
        self.cache = AtomicJsonFile(cache_path, schema_version=2)
        self.pricing = pricing or DEFAULT_PRICING

    def _initial_state(self) -> dict[str, Any]:
        loaded = self.cache.load(default={})
        if not isinstance(loaded, dict):
            loaded = {}
        loaded.setdefault("files", {})
        return loaded

    def _scan_file(self, path: Path, previous: dict[str, Any] | None) -> dict[str, Any]:
        stat = path.stat()
        same_file = bool(
            previous
            and previous.get("inode") == stat.st_ino
            and isinstance(previous.get("offset"), int)
            and 0 <= previous["offset"] <= stat.st_size
        )
        offset = previous["offset"] if same_file else 0
        model = previous.get("model") if same_file else None
        days = dict(previous.get("days") or {}) if same_file else {}
        unknown = set(previous.get("unknown_models") or ()) if same_file else set()

        with path.open("rb") as handle:
            handle.seek(offset)
            while raw_line := handle.readline():
                try:
                    event = json.loads(raw_line)
                except (json.JSONDecodeError, UnicodeDecodeError, ValueError):
                    LOGGER.warning("Skipping malformed session line in %s", path.name)
                    continue
                event_type = event.get("type")
                payload = event.get("payload") or {}
                if event_type == "turn_context":
                    candidate = payload.get("model")
                    if isinstance(candidate, str) and candidate:
                        model = candidate
                    continue
                if event_type != "event_msg" or payload.get("type") != "token_count":
                    continue
                usage = (payload.get("info") or {}).get("last_token_usage") or {}
                day = _local_date(event.get("timestamp"))
                if day is None or not isinstance(usage, dict):
                    continue
                input_tokens = max(0, int(usage.get("input_tokens") or 0))
                cached_tokens = max(0, int(usage.get("cached_input_tokens") or 0))
                output_tokens = max(0, int(usage.get("output_tokens") or 0))
                tokens = max(0, input_tokens - cached_tokens) + cached_tokens + output_tokens
                if tokens == 0:
                    continue
                model_name = model or "unknown"
                cost = estimate_cost_microusd(
                    model_name,
                    input_tokens,
                    cached_tokens,
                    output_tokens,
                    self.pricing,
                )
                if cost is None:
                    unknown.add(model_name)
                    cost = 0
                key = day.isoformat()
                bucket = days.setdefault(key, {"tokens": 0, "cost_microusd": 0})
                bucket["tokens"] += tokens
                bucket["cost_microusd"] += cost
            next_offset = handle.tell()
        return {
            "inode": stat.st_ino,
            "size": stat.st_size,
            "mtime_ns": stat.st_mtime_ns,
            "offset": next_offset,
            "model": model,
            "days": days,
            "unknown_models": sorted(unknown),
        }

    def collect(self, *, today: date | None = None) -> SessionStats:
        current_day = date.today() if today is None else today
        first_day = current_day - timedelta(days=6)
        state = self._initial_state()
        file_state: dict[str, Any] = state["files"]
        for path in sorted(self.sessions_dir.glob("**/rollout-*.jsonl")):
            try:
                previous = file_state.get(str(path))
                stat = path.stat()
                if (
                    previous
                    and previous.get("inode") == stat.st_ino
                    and previous.get("size") == stat.st_size
                    and previous.get("mtime_ns") == stat.st_mtime_ns
                ):
                    continue
                file_state[str(path)] = self._scan_file(path, previous)
            except OSError as error:
                LOGGER.warning("Skipping unreadable session file %s: %s", path.name, error)

        totals = {day.isoformat(): {"tokens": 0, "cost_microusd": 0} for day in (
            first_day + timedelta(days=index) for index in range(7)
        )}
        unknown_models: set[str] = set()
        for entry in file_state.values():
            unknown_models.update(entry.get("unknown_models") or ())
            for key, bucket in (entry.get("days") or {}).items():
                if key in totals:
                    totals[key]["tokens"] += int(bucket.get("tokens") or 0)
                    totals[key]["cost_microusd"] += int(bucket.get("cost_microusd") or 0)

        # Keep only eight days of per-file contributions while retaining file
        # offsets/model context for true incremental reads.
        oldest_kept = (first_day - timedelta(days=1)).isoformat()
        for entry in file_state.values():
            entry["days"] = {
                key: value for key, value in (entry.get("days") or {}).items() if key >= oldest_kept
            }
        self.cache.save(state)

        ordered = [totals[(first_day + timedelta(days=index)).isoformat()] for index in range(7)]
        today_bucket = ordered[-1]
        return SessionStats(
            today_tokens=today_bucket["tokens"],
            today_cost_cents=round(today_bucket["cost_microusd"] / 10_000),
            daily_tokens=tuple(bucket["tokens"] for bucket in ordered),
            unknown_models=tuple(sorted(model for model in unknown_models if model != "unknown")),
        )
