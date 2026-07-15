from __future__ import annotations

import json
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any

import certifi
import requests

from .models import UsageSnapshot

USAGE_URL = "https://chatgpt.com/backend-api/wham/usage"


@dataclass(frozen=True, slots=True)
class UsageProviderError(RuntimeError):
    code: str
    message: str

    def __str__(self) -> str:
        return self.message


def _reset_epoch(value: Any) -> int | None:
    if isinstance(value, (int, float)):
        return int(value)
    if isinstance(value, str):
        try:
            return int(datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp())
        except ValueError:
            return None
    return None


def _percentage(window: Any) -> int | None:
    if not isinstance(window, dict) or not isinstance(window.get("used_percent"), (int, float)):
        return None
    return max(0, min(100, round(float(window["used_percent"]))))


FIVE_HOURS_SECONDS = 5 * 60 * 60
SEVEN_DAYS_SECONDS = 7 * 24 * 60 * 60


def _window_duration(window: Any) -> int | None:
    if not isinstance(window, dict):
        return None
    value = window.get("limit_window_seconds")
    return int(value) if isinstance(value, (int, float)) and value > 0 else None


def _classified_windows(rate_limit: Any) -> tuple[dict[str, Any] | None, dict[str, Any] | None]:
    """Find the 5-hour and 7-day windows by duration, not API field position.

    The service has used ``primary_window`` for different periods over time.  In
    particular, some current accounts expose only a 604800-second primary
    window.  Treating primary as 5h would silently put the weekly value in the
    wrong ring, so duration metadata is authoritative.
    """

    if not isinstance(rate_limit, dict):
        return None, None
    five_hour: dict[str, Any] | None = None
    seven_day: dict[str, Any] | None = None
    unclassified: list[dict[str, Any]] = []
    for name in ("primary_window", "secondary_window"):
        window = rate_limit.get(name)
        if not isinstance(window, dict) or _percentage(window) is None:
            continue
        duration = _window_duration(window)
        if duration is not None and abs(duration - FIVE_HOURS_SECONDS) <= 60:
            five_hour = window
        elif duration is not None and abs(duration - SEVEN_DAYS_SECONDS) <= 60:
            seven_day = window
        else:
            unclassified.append(window)

    # Older responses did not include duration metadata and consistently used
    # primary=5h, secondary=7d.  Preserve compatibility only for that shape.
    if five_hour is None and seven_day is None and unclassified:
        primary = rate_limit.get("primary_window")
        secondary = rate_limit.get("secondary_window")
        if isinstance(primary, dict):
            five_hour = primary
        if isinstance(secondary, dict):
            seven_day = secondary
    return five_hour, seven_day


class CodexUsageProvider:
    def __init__(
        self,
        auth_path: Path,
        *,
        session: requests.Session | None = None,
        timeout: float = 15.0,
    ) -> None:
        self.auth_path = auth_path
        self.session = session or requests.Session()
        self.timeout = timeout
        self.last_valid: UsageSnapshot | None = None

    def _credentials(self) -> tuple[str, str | None]:
        try:
            with self.auth_path.open("r", encoding="utf-8") as handle:
                tokens = (json.load(handle).get("tokens") or {})
        except (OSError, json.JSONDecodeError, AttributeError) as error:
            raise UsageProviderError("auth_missing", "Codex auth is unavailable; run codex login") from error
        token = tokens.get("access_token")
        if not isinstance(token, str) or not token:
            raise UsageProviderError("auth_missing", "Codex access token is unavailable; run codex login")
        account = tokens.get("account_id")
        return token, account if isinstance(account, str) and account else None

    def fetch(self, *, now: int | None = None) -> UsageSnapshot:
        token, account = self._credentials()
        headers = {
            "Authorization": f"Bearer {token}",
            "Accept": "application/json",
            "User-Agent": "CodexIsland/0.1 (+local macOS bridge)",
        }
        if account:
            headers["ChatGPT-Account-Id"] = account
        try:
            response = self.session.get(
                USAGE_URL,
                headers=headers,
                timeout=self.timeout,
                verify=certifi.where(),
            )
        except requests.RequestException as error:
            raise UsageProviderError("transport", f"Codex usage request failed: {error.__class__.__name__}") from error
        if response.status_code == 401:
            raise UsageProviderError("auth_expired", "Codex authentication expired; run codex login")
        if response.status_code != 200:
            raise UsageProviderError("http", f"Codex usage returned HTTP {response.status_code}")
        try:
            payload = response.json()
        except requests.JSONDecodeError as error:
            raise UsageProviderError("invalid_json", "Codex usage returned invalid JSON") from error
        if not isinstance(payload, dict):
            raise UsageProviderError("invalid_shape", "Codex usage response is not an object")

        rate_limit = payload.get("rate_limit") or {}
        five_hour, seven_day = _classified_windows(rate_limit)
        current = int(time.time()) if now is None else now
        snapshot = UsageSnapshot(
            updated_at=current,
            five_hour_percent=_percentage(five_hour),
            seven_day_percent=_percentage(seven_day),
            five_hour_reset_at=_reset_epoch(five_hour.get("reset_at"))
            if isinstance(five_hour, dict)
            else None,
            plan=payload.get("plan_type") if isinstance(payload.get("plan_type"), str) else None,
        )
        if snapshot.five_hour_percent is None and snapshot.seven_day_percent is None:
            raise UsageProviderError("missing_windows", "Codex usage did not report a 5h or 7d window")
        self.last_valid = snapshot
        return snapshot
