from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path

from platformdirs import user_data_path


def _flag(name: str, default: bool = False) -> bool:
    raw = os.environ.get(name)
    if raw is None:
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on"}


@dataclass(frozen=True, slots=True)
class Settings:
    data_dir: Path
    codex_auth_path: Path
    codex_sessions_dir: Path
    usage_interval_seconds: int = 300
    radar_interval_seconds: int = 3600
    pet_interval_seconds: int = 2
    heartbeat_interval_seconds: int = 60
    ble_io_timeout_seconds: int = 10
    radar_api_url: str | None = None
    radar_api_token: str | None = None
    radar_allow_html: bool = False
    radar_primary_key: str | None = None
    ble_address: str | None = None

    @classmethod
    def from_environment(cls) -> "Settings":
        home = Path.home()
        return cls(
            data_dir=Path(
                os.environ.get(
                    "CODEX_ISLAND_DATA_DIR",
                    user_data_path("CodexIsland", ensure_exists=True),
                )
            ),
            codex_auth_path=Path(
                os.environ.get("CODEX_AUTH_PATH", home / ".codex" / "auth.json")
            ),
            codex_sessions_dir=Path(
                os.environ.get("CODEX_SESSIONS_DIR", home / ".codex" / "sessions")
            ),
            usage_interval_seconds=max(30, int(os.environ.get("CODEX_USAGE_INTERVAL", "300"))),
            radar_interval_seconds=max(1800, int(os.environ.get("CODEX_RADAR_INTERVAL", "3600"))),
            pet_interval_seconds=max(
                1, int(os.environ.get("CODEX_PET_INTERVAL", "2"))
            ),
            heartbeat_interval_seconds=max(
                15, int(os.environ.get("CODEX_HEARTBEAT_INTERVAL", "60"))
            ),
            ble_io_timeout_seconds=max(
                5, int(os.environ.get("CODEX_BLE_IO_TIMEOUT", "10"))
            ),
            radar_api_url=os.environ.get("CODEX_RADAR_API_URL") or None,
            radar_api_token=os.environ.get("CODEX_RADAR_API_TOKEN") or None,
            radar_allow_html=_flag("CODEX_RADAR_ALLOW_HTML"),
            radar_primary_key=os.environ.get("CODEX_RADAR_PRIMARY_KEY") or None,
            ble_address=os.environ.get("CODEX_ISLAND_BLE_ADDRESS") or None,
        )
