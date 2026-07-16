from __future__ import annotations

import re
import time
from collections import OrderedDict
from typing import Any, Protocol

import certifi
import requests

from .models import DistributedRadarRow, DistributedRadarSnapshot

DISTRIBUTED_RADAR_URL = "https://api.codexradar.com/api/v1/table"
MAX_DISTRIBUTED_ROWS = 32


class DistributedRadarError(RuntimeError):
    pass


class DistributedRadarProviderProtocol(Protocol):
    etag: str | None
    last_modified: str | None

    def fetch(self) -> DistributedRadarSnapshot:
        ...


def _utf8_limit(value: str, maximum: int) -> str:
    encoded = value.strip().encode("utf-8")
    if len(encoded) <= maximum:
        return encoded.decode("utf-8")
    return encoded[:maximum].decode("utf-8", errors="ignore").rstrip()


def display_model_name(model: str) -> str:
    """Make current and future model slugs readable without a name enum."""

    value = model.strip()
    versioned = re.fullmatch(r"gpt-(\d+(?:\.\d+)?)-(.+)", value, re.IGNORECASE)
    if versioned:
        suffix = re.sub(r"[-_.]+", " ", versioned.group(2)).strip()
        return _utf8_limit(suffix.title(), 31)
    bare = re.fullmatch(r"gpt-(\d+(?:\.\d+)?)", value, re.IGNORECASE)
    if bare:
        return _utf8_limit(f"GPT-{bare.group(1)}", 31)
    readable = re.sub(r"[-_.]+", " ", value).strip()
    return _utf8_limit(readable.title() if readable.islower() else readable, 31)


def _metric(cell: Any) -> tuple[int, int] | None:
    if not isinstance(cell, dict) or cell.get("rate") is None:
        return None
    passed = cell.get("p")
    total = cell.get("n")
    if (
        isinstance(passed, bool)
        or isinstance(total, bool)
        or not isinstance(passed, int)
        or not isinstance(total, int)
        or not 0 <= passed <= total <= 65_535
    ):
        raise DistributedRadarError("distributed Radar cell metrics are invalid")
    return passed, total


def _iq(passed: int, total: int) -> int | None:
    if total == 0:
        return None
    return min(150, max(0, (passed * 150 + total // 2) // total))


def parse_distributed_table(
    payload: Any, *, now: int | None = None
) -> DistributedRadarSnapshot:
    current = int(time.time()) if now is None else now
    if not isinstance(payload, dict) or payload.get("schema") != 1:
        raise DistributedRadarError("distributed Radar schema is unsupported")
    combos = payload.get("combos")
    tasks = payload.get("tasks")
    cells = payload.get("cells")
    if not isinstance(combos, list) or not isinstance(tasks, list) or not isinstance(cells, dict):
        raise DistributedRadarError("distributed Radar table shape is invalid")

    groups: OrderedDict[str, list[str]] = OrderedDict()
    seen_combos: set[tuple[str, str]] = set()
    for combo in combos:
        if not isinstance(combo, dict):
            raise DistributedRadarError("distributed Radar combo is not an object")
        model = combo.get("model")
        effort = combo.get("effort")
        if (
            not isinstance(model, str)
            or not model.strip()
            or not isinstance(effort, str)
            or not effort.strip()
        ):
            raise DistributedRadarError("distributed Radar combo labels are invalid")
        normalized = (model.strip(), effort.strip())
        if normalized in seen_combos:
            continue
        seen_combos.add(normalized)
        groups.setdefault(normalized[0], []).append(normalized[1])

    task_ids: list[str] = []
    for task in tasks:
        if not isinstance(task, dict) or not isinstance(task.get("id"), str):
            raise DistributedRadarError("distributed Radar task has no id")
        task_ids.append(task["id"])
    if not groups or not task_ids:
        raise DistributedRadarError("distributed Radar returned no model grid")

    metrics: dict[tuple[str, str], list[int]] = {
        (model, effort): [0, 0]
        for model, efforts in groups.items()
        for effort in efforts
    }
    for task_id in task_ids:
        for model, efforts in groups.items():
            for effort in efforts:
                metric = _metric(cells.get(f"{task_id}|{model}|{effort}"))
                if metric is None:
                    continue
                totals = metrics[(model, effort)]
                totals[0] += metric[0]
                totals[1] += metric[1]
                if totals[1] > 65_535:
                    raise DistributedRadarError("distributed Radar sample count is too large")

    rows: list[DistributedRadarRow] = []
    for model, efforts in groups.items():
        display = display_model_name(model)
        model_passed = sum(metrics[(model, effort)][0] for effort in efforts)
        model_total = sum(metrics[(model, effort)][1] for effort in efforts)
        rows.append(
            DistributedRadarRow(
                key=f"{model}/@all",
                model=display,
                effort="",
                iq=_iq(model_passed, model_total),
                passed=model_passed,
                total=model_total,
                aggregate=True,
                source_order=len(rows),
            )
        )
        for effort in efforts:
            passed, total = metrics[(model, effort)]
            rows.append(
                DistributedRadarRow(
                    key=f"{model}/@{effort}",
                    model=display,
                    effort=_utf8_limit(effort, 23),
                    iq=_iq(passed, total),
                    passed=passed,
                    total=total,
                    aggregate=False,
                    source_order=len(rows),
                )
            )

    # The web page's first GPT-family average is intentionally absent: it is
    # a client-side roll-up, while these rows contain each model and effort.
    return DistributedRadarSnapshot(updated_at=current, rows=tuple(rows[:MAX_DISTRIBUTED_ROWS]))


class DistributedRadarProvider:
    def __init__(
        self,
        url: str = DISTRIBUTED_RADAR_URL,
        *,
        session: requests.Session | None = None,
        timeout: float = 30.0,
        etag: str | None = None,
        last_modified: str | None = None,
    ) -> None:
        self.url = url
        self.session = session or requests.Session()
        self.timeout = timeout
        self.etag = etag
        self.last_modified = last_modified

    def fetch(self) -> DistributedRadarSnapshot:
        headers = {
            "Accept": "application/json",
            "User-Agent": "CodexIsland/0.3 (+local macOS bridge; 5-minute sync)",
        }
        if self.etag:
            headers["If-None-Match"] = self.etag
        if self.last_modified:
            headers["If-Modified-Since"] = self.last_modified
        try:
            response = self.session.get(
                self.url, headers=headers, timeout=self.timeout, verify=certifi.where()
            )
        except requests.RequestException as error:
            raise DistributedRadarError(
                f"distributed Radar request failed: {error.__class__.__name__}"
            ) from error
        if response.status_code != 200:
            raise DistributedRadarError(
                f"distributed Radar returned HTTP {response.status_code}"
            )
        try:
            payload = response.json()
        except requests.JSONDecodeError as error:
            raise DistributedRadarError("distributed Radar returned invalid JSON") from error
        snapshot = parse_distributed_table(payload)
        self.etag = response.headers.get("ETag")
        self.last_modified = response.headers.get("Last-Modified")
        return snapshot
