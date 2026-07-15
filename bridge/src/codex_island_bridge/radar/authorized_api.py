from __future__ import annotations

import time
from typing import Any

import certifi
import requests

from ..models import RadarModel, RadarSnapshot
from .base import NotModified, RadarProviderError, normalize_key, parse_timestamp


class AuthorizedApiRadarProvider:
    def __init__(
        self,
        url: str,
        *,
        token: str | None = None,
        session: requests.Session | None = None,
        timeout: float = 15.0,
        etag: str | None = None,
        last_modified: str | None = None,
    ) -> None:
        self.url = url
        self.token = token
        self.session = session or requests.Session()
        self.timeout = timeout
        self.etag = etag
        self.last_modified = last_modified

    def _model(self, item: Any, order: int) -> RadarModel:
        if not isinstance(item, dict):
            raise RadarProviderError("Radar API model is not an object")
        family = item.get("family")
        effort = item.get("effort")
        if not isinstance(family, str) or not family.strip():
            raise RadarProviderError("Radar API model has no family")
        if not isinstance(effort, str) or not effort.strip():
            raise RadarProviderError("Radar API model has no effort")
        raw_iq = item.get("iq_x10")
        if not isinstance(raw_iq, (int, float)):
            iq = item.get("iq")
            raw_iq = round(float(iq) * 10) if isinstance(iq, (int, float)) else None
        passed = item.get("passed")
        total = item.get("total")
        if (
            not isinstance(raw_iq, (int, float))
            or not 0 < int(raw_iq) <= 3000
            or not isinstance(passed, int)
            or not isinstance(total, int)
            or not 0 <= passed <= total <= 255
        ):
            raise RadarProviderError("Radar API model metrics are invalid")
        key = item.get("key")
        return RadarModel(
            key=key if isinstance(key, str) and key else normalize_key(family, effort),
            family=family.strip(),
            effort=effort.strip(),
            iq_x10=int(raw_iq),
            passed=passed,
            total=total,
            source_order=order,
        )

    def fetch(self) -> RadarSnapshot:
        headers = {
            "Accept": "application/json",
            "User-Agent": "CodexIsland/0.1 (+local macOS bridge)",
        }
        if self.token:
            headers["Authorization"] = f"Bearer {self.token}"
        if self.etag:
            headers["If-None-Match"] = self.etag
        if self.last_modified:
            headers["If-Modified-Since"] = self.last_modified
        try:
            response = self.session.get(
                self.url, headers=headers, timeout=self.timeout, verify=certifi.where()
            )
        except requests.RequestException as error:
            raise RadarProviderError(f"Radar API request failed: {error.__class__.__name__}") from error
        if response.status_code == 304:
            raise NotModified("Radar API data is unchanged")
        if response.status_code != 200:
            raise RadarProviderError(f"Radar API returned HTTP {response.status_code}")
        try:
            payload = response.json()
        except requests.JSONDecodeError as error:
            raise RadarProviderError("Radar API returned invalid JSON") from error
        if not isinstance(payload, dict) or not isinstance(payload.get("models"), list):
            raise RadarProviderError("Radar API response has no model list")
        models = tuple(self._model(item, index) for index, item in enumerate(payload["models"]))
        if len(models) < 3:
            raise RadarProviderError("Radar API returned fewer than three models")
        self.etag = response.headers.get("ETag")
        self.last_modified = response.headers.get("Last-Modified")
        now = int(time.time())
        return RadarSnapshot(
            updated_at=parse_timestamp(payload.get("updated_at"), now), models=models
        )
