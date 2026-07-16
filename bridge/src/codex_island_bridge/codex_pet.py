from __future__ import annotations

import json
import logging
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any

from .models import PetActivity, PetSnapshot

LOGGER = logging.getLogger(__name__)
TRANSITION_TYPES: dict[str, PetActivity] = {
    "task_started": "running",
    "task_complete": "review",
    "turn_aborted": "failed",
}
WAITING_FUNCTIONS = {
    "request_user_input",
    "request_permissions",
    "request_permission",
    "ask_user",
}
STATE_PRIORITY: dict[PetActivity, int] = {
    "idle": 0,
    "review": 1,
    "running": 2,
    "failed": 3,
    "waiting": 4,
}


@dataclass(frozen=True, slots=True)
class _FileSnapshot:
    size: int
    mtime_ns: int
    modified_at: int
    event_time: int
    state: PetActivity
    active: bool


def _event_timestamp(value: Any, fallback: int) -> int:
    if not isinstance(value, str):
        return fallback
    try:
        return int(datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp())
    except ValueError:
        return fallback


class CodexPetProvider:
    """Infer the aggregate Codex task state from local JSONL lifecycle events."""

    def __init__(
        self,
        sessions_dir: Path,
        *,
        discovery_seconds: float = 5.0,
        recent_files: int = 12,
        tail_bytes: int = 768 * 1024,
        active_grace_seconds: int = 120,
        review_seconds: int = 300,
        failed_seconds: int = 300,
    ) -> None:
        self.sessions_dir = sessions_dir
        self.discovery_seconds = discovery_seconds
        self.recent_files = recent_files
        self.tail_bytes = tail_bytes
        self.active_grace_seconds = active_grace_seconds
        self.review_seconds = review_seconds
        self.failed_seconds = failed_seconds
        self._paths: tuple[Path, ...] = ()
        self._next_discovery = 0.0
        self._cache: dict[Path, _FileSnapshot] = {}

    def _discover(self) -> None:
        candidates: list[tuple[int, Path]] = []
        try:
            for path in self.sessions_dir.glob("**/rollout-*.jsonl"):
                try:
                    candidates.append((path.stat().st_mtime_ns, path))
                except OSError:
                    continue
        except OSError as error:
            LOGGER.warning("Cannot scan Codex sessions for pet state: %s", error)
        candidates.sort(reverse=True)
        self._paths = tuple(path for _, path in candidates[: self.recent_files])
        retained = set(self._paths)
        self._cache = {
            path: snapshot
            for path, snapshot in self._cache.items()
            if path in retained
        }

    def _read_tail(self, path: Path, now: int) -> _FileSnapshot:
        stat = path.stat()
        cached = self._cache.get(path)
        if (
            cached is not None
            and cached.size == stat.st_size
            and cached.mtime_ns == stat.st_mtime_ns
        ):
            return cached

        start = max(0, stat.st_size - self.tail_bytes)
        with path.open("rb") as handle:
            handle.seek(start)
            if start:
                handle.readline()
            lines = handle.readlines()

        transition_state: PetActivity | None = None
        transition_time = 0
        latest_time = int(stat.st_mtime)
        pending_waits: dict[str, int] = {}
        for raw_line in lines:
            try:
                event = json.loads(raw_line)
            except (json.JSONDecodeError, UnicodeDecodeError, ValueError):
                continue
            event_time = _event_timestamp(event.get("timestamp"), latest_time)
            latest_time = max(latest_time, event_time)
            payload = event.get("payload") or {}
            event_type = event.get("type")
            payload_type = payload.get("type")
            if event_type == "event_msg" and payload_type in TRANSITION_TYPES:
                transition_state = TRANSITION_TYPES[payload_type]
                transition_time = event_time
            elif event_type == "response_item" and payload_type == "function_call":
                name = str(payload.get("name") or "").rsplit(".", 1)[-1]
                call_id = payload.get("call_id")
                if name in WAITING_FUNCTIONS and isinstance(call_id, str):
                    pending_waits[call_id] = event_time
            elif (
                event_type == "response_item"
                and payload_type == "function_call_output"
            ):
                call_id = payload.get("call_id")
                if isinstance(call_id, str):
                    pending_waits.pop(call_id, None)

        if pending_waits:
            state: PetActivity = "waiting"
            event_time = max(pending_waits.values())
            active = True
        elif transition_state is not None:
            state = transition_state
            event_time = transition_time
            active = transition_state == "running"
        elif now - int(stat.st_mtime) <= self.active_grace_seconds:
            state = "running"
            event_time = latest_time
            active = True
        else:
            state = "idle"
            event_time = latest_time
            active = False

        snapshot = _FileSnapshot(
            size=stat.st_size,
            mtime_ns=stat.st_mtime_ns,
            modified_at=int(stat.st_mtime),
            event_time=event_time,
            state=state,
            active=active,
        )
        self._cache[path] = snapshot
        return snapshot

    def collect(self, *, now: int | None = None) -> PetSnapshot:
        current = int(time.time()) if now is None else now
        monotonic = time.monotonic()
        if monotonic >= self._next_discovery or not self._paths:
            self._discover()
            self._next_discovery = monotonic + self.discovery_seconds

        snapshots: list[_FileSnapshot] = []
        for path in self._paths:
            try:
                snapshots.append(self._read_tail(path, current))
            except OSError as error:
                LOGGER.debug("Skipping unreadable Codex pet session %s: %s", path, error)
        if not snapshots:
            return PetSnapshot(updated_at=current)

        effective: list[_FileSnapshot] = []
        for snapshot in snapshots:
            age = max(0, current - snapshot.event_time)
            if snapshot.state == "review" and age > self.review_seconds:
                continue
            if snapshot.state == "failed" and age > self.failed_seconds:
                continue
            if snapshot.state == "running" and (
                not snapshot.active
                or current - snapshot.modified_at > self.active_grace_seconds
            ):
                continue
            effective.append(snapshot)
        if not effective:
            return PetSnapshot(
                updated_at=max(item.event_time for item in snapshots),
                state="idle",
                active_tasks=0,
            )

        selected = max(
            effective,
            key=lambda item: (STATE_PRIORITY[item.state], item.event_time),
        )
        active_tasks = min(
            255,
            sum(item.state in {"running", "waiting"} for item in effective),
        )
        return PetSnapshot(
            updated_at=max(item.event_time for item in effective),
            state=selected.state,
            active_tasks=active_tasks,
        )
