from __future__ import annotations

import re
import time
from datetime import datetime
from typing import Any

import certifi
import requests
from bs4 import BeautifulSoup, Tag

from ..models import RadarModel, RadarSnapshot
from .base import NotModified, RadarProviderError, normalize_key

RADAR_URL = "https://codexradar.com/"


def _heading_timestamp(text: str, now: int) -> int:
    match = re.search(
        r"(?:(20\d{2})\s*年)?\s*(\d{1,2})\s*月\s*(\d{1,2})\s*日\s*(\d{1,2})[:：](\d{2})",
        text,
    )
    if not match:
        return now
    current = datetime.fromtimestamp(now).astimezone()
    year = int(match.group(1) or current.year)
    candidate = current.replace(
        year=year,
        month=int(match.group(2)),
        day=int(match.group(3)),
        hour=int(match.group(4)),
        minute=int(match.group(5)),
        second=0,
        microsecond=0,
    )
    if candidate.timestamp() > now + 2 * 24 * 3600 and match.group(1) is None:
        candidate = candidate.replace(year=year - 1)
    return int(candidate.timestamp())


def _name_parts(label: str) -> tuple[str, str]:
    parts = label.strip().rsplit(maxsplit=1)
    if len(parts) != 2 or not all(parts):
        raise RadarProviderError(f"Radar model label cannot be split: {label!r}")
    return parts[0], parts[1]


def _table_models(section: Tag) -> tuple[RadarModel, ...]:
    rows: dict[str, list[str]] = {}
    for row in section.find_all("tr"):
        cells = [cell.get_text(" ", strip=True) for cell in row.find_all(["th", "td"])]
        if not cells:
            continue
        for prefix in ("项目", "通过数", "IQ"):
            if cells[0].startswith(prefix):
                rows[prefix] = cells[1:]
                break
    if not rows:
        return ()
    if not all(name in rows for name in ("项目", "通过数", "IQ")):
        raise RadarProviderError("Radar table is missing a required semantic row")
    names, passes, iq_values = rows["项目"], rows["通过数"], rows["IQ"]
    if len(names) != len(passes) or len(names) != len(iq_values):
        raise RadarProviderError("Radar table row lengths do not match")
    models: list[RadarModel] = []
    for order, (label, pass_text, iq_text) in enumerate(zip(names, passes, iq_values)):
        family, effort = _name_parts(label)
        pass_match = re.fullmatch(r"\s*(\d+)\s*/\s*(\d+)\s*", pass_text)
        try:
            iq_x10 = round(float(iq_text) * 10)
        except ValueError as error:
            raise RadarProviderError("Radar table IQ is not numeric") from error
        if not pass_match or not 0 < iq_x10 <= 3000:
            raise RadarProviderError("Radar table metrics are invalid")
        passed, total = map(int, pass_match.groups())
        if passed > total or total > 255:
            raise RadarProviderError("Radar table pass count is invalid")
        models.append(
            RadarModel(
                key=normalize_key(family, effort),
                family=family,
                effort=effort,
                iq_x10=iq_x10,
                passed=passed,
                total=total,
                source_order=order,
            )
        )
    return tuple(models)


def _chip_models(section: Tag) -> tuple[RadarModel, ...]:
    pass_counts: dict[str, tuple[int, int]] = {}
    for element in section.find_all(attrs={"data-model-key": True}):
        tooltip_key = element.get("data-model-iq-tooltip-key")
        if not isinstance(tooltip_key, str) or not tooltip_key.startswith("iq|"):
            continue
        label = element.get("aria-label")
        match = re.search(r"(\d+)\s*/\s*(\d+)", label or "")
        if match:
            pass_counts[str(element["data-model-key"])] = tuple(map(int, match.groups()))

    models: list[RadarModel] = []
    seen: set[str] = set()
    for element in section.find_all(attrs={"data-model-key": True, "role": "button"}):
        data_key = str(element["data-model-key"])
        if data_key in seen:
            continue
        name_node = element.find("span")
        iq_node = element.find("strong")
        if name_node is None or iq_node is None:
            continue
        label = name_node.get_text(" ", strip=True)
        try:
            family, effort = _name_parts(label)
            iq_x10 = round(float(iq_node.get_text(strip=True)) * 10)
        except ValueError as error:
            raise RadarProviderError("Radar score chip IQ is not numeric") from error
        if not 0 < iq_x10 <= 3000:
            raise RadarProviderError("Radar score chip IQ is outside the valid range")
        passed, total = pass_counts.get(data_key, (0, 10))
        models.append(
            RadarModel(
                key=normalize_key(family, effort),
                family=family,
                effort=effort,
                iq_x10=iq_x10,
                passed=passed,
                total=total,
                source_order=len(models),
            )
        )
        seen.add(data_key)
    return tuple(models)


def parse_radar_html(html: str, *, now: int | None = None) -> RadarSnapshot:
    current = int(time.time()) if now is None else now
    soup = BeautifulSoup(html, "html.parser")
    heading = next(
        (
            node
            for node in soup.find_all(["h1", "h2", "h3"])
            if node.get_text(" ", strip=True).startswith("降智雷达")
        ),
        None,
    )
    if heading is None:
        raise RadarProviderError("Radar heading was not found")
    section = heading.find_parent("section")
    if section is None:
        raise RadarProviderError("Radar semantic section was not found")
    models = _table_models(section) or _chip_models(section)
    if len(models) < 3:
        raise RadarProviderError("Radar page yielded fewer than three models")
    return RadarSnapshot(
        updated_at=_heading_timestamp(heading.get_text(" ", strip=True), current),
        models=models,
    )


class HtmlRadarProvider:
    def __init__(
        self,
        *,
        url: str = RADAR_URL,
        session: requests.Session | None = None,
        timeout: float = 15.0,
        etag: str | None = None,
        last_modified: str | None = None,
    ) -> None:
        self.url = url
        self.session = session or requests.Session()
        self.timeout = timeout
        self.etag = etag
        self.last_modified = last_modified

    def fetch(self) -> RadarSnapshot:
        headers = {
            "Accept": "text/html,application/xhtml+xml",
            "User-Agent": "CodexIsland/0.1 (+local macOS bridge; hourly)",
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
            raise RadarProviderError(f"Radar HTML request failed: {error.__class__.__name__}") from error
        if response.status_code == 304:
            raise NotModified("Radar HTML is unchanged")
        if response.status_code != 200:
            raise RadarProviderError(f"Radar HTML returned HTTP {response.status_code}")
        self.etag = response.headers.get("ETag")
        self.last_modified = response.headers.get("Last-Modified")
        return parse_radar_html(response.text)
