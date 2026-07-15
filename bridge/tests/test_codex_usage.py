from __future__ import annotations

from pathlib import Path

from codex_island_bridge.codex_usage import CodexUsageProvider


class Response:
    status_code = 200

    def json(self):
        return {
            "plan_type": "plus",
            "rate_limit": {
                "primary_window": {
                    "used_percent": 42.4,
                    "limit_window_seconds": 18_000,
                    "reset_at": 1_500,
                },
                "secondary_window": {
                    "used_percent": 31.2,
                    "limit_window_seconds": 604_800,
                },
            },
        }


class Session:
    def get(self, *args, **kwargs):
        assert kwargs["headers"]["Authorization"].startswith("Bearer ")
        return Response()


def test_usage_response_is_normalized(tmp_path: Path) -> None:
    auth = tmp_path / "auth.json"
    auth.write_text('{"tokens":{"access_token":"secret","account_id":"acct"}}')
    result = CodexUsageProvider(auth, session=Session()).fetch(now=1_000)
    assert result.five_hour_percent == 42
    assert result.seven_day_percent == 31
    assert result.five_hour_reset_at == 1_500


class WeeklyOnlyResponse(Response):
    def json(self):
        return {
            "plan_type": "plus",
            "rate_limit": {
                "primary_window": {
                    "used_percent": 13,
                    "limit_window_seconds": 604_800,
                    "reset_at": 999_999,
                },
                "secondary_window": None,
            },
        }


class WeeklyOnlySession(Session):
    def get(self, *args, **kwargs):
        return WeeklyOnlyResponse()


def test_weekly_primary_is_not_mislabeled_as_five_hour(tmp_path: Path) -> None:
    auth = tmp_path / "auth.json"
    auth.write_text('{"tokens":{"access_token":"secret"}}')
    result = CodexUsageProvider(auth, session=WeeklyOnlySession()).fetch(now=1_000)
    assert result.five_hour_percent is None
    assert result.five_hour_reset_at is None
    assert result.seven_day_percent == 13
