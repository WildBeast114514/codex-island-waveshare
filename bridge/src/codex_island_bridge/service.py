from __future__ import annotations

import asyncio
import dataclasses
import logging
import time
from dataclasses import asdict
from pathlib import Path
from typing import Any

from .ble_transport import BleTransport
from .cache import AtomicJsonFile
from .codex_sessions import CodexSessionAggregator
from .codex_usage import CodexUsageProvider, UsageProviderError
from .config import Settings
from .models import UsageSnapshot
from .protocol import Sequence, usage_line

LOGGER = logging.getLogger(__name__)


def _snapshot_from_cache(value: Any) -> UsageSnapshot | None:
    if not isinstance(value, dict):
        return None
    try:
        daily = tuple(int(item) for item in value.get("daily_tokens", ()))
        if len(daily) != 7:
            return None
        return UsageSnapshot(
            updated_at=int(value["updated_at"]),
            five_hour_percent=value.get("five_hour_percent"),
            seven_day_percent=value.get("seven_day_percent"),
            five_hour_reset_at=value.get("five_hour_reset_at"),
            today_tokens=int(value.get("today_tokens", 0)),
            today_cost_cents=int(value.get("today_cost_cents", 0)),
            daily_tokens=daily,
            plan=value.get("plan"),
        )
    except (KeyError, TypeError, ValueError):
        return None


class UsageService:
    def __init__(self, settings: Settings) -> None:
        self.provider = CodexUsageProvider(settings.codex_auth_path)
        self.sessions = CodexSessionAggregator(
            settings.codex_sessions_dir, settings.data_dir / "session_index.json"
        )
        self.cache = AtomicJsonFile(settings.data_dir / "usage_cache.json", schema_version=1)

    def cached(self) -> UsageSnapshot | None:
        return _snapshot_from_cache(self.cache.load())

    def collect(self) -> UsageSnapshot:
        previous = self.cached()
        try:
            limits = self.provider.fetch()
        except UsageProviderError:
            if previous is None:
                raise
            limits = previous
            LOGGER.warning("Usage endpoint failed; retaining last valid limit windows")
        stats = self.sessions.collect()
        if stats.unknown_models:
            LOGGER.warning(
                "Estimated cost excludes unpriced session model(s): %s",
                ", ".join(stats.unknown_models),
            )
        snapshot = dataclasses.replace(
            limits,
            today_tokens=stats.today_tokens,
            today_cost_cents=stats.today_cost_cents,
            daily_tokens=stats.daily_tokens,
        )
        self.cache.save(asdict(snapshot))
        return snapshot


class BridgeService:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings
        self.usage = UsageService(settings)
        self.sequence = Sequence()
        self.refresh = asyncio.Event()

    async def _on_device_message(self, message: dict[str, Any]) -> None:
        if message.get("v") == 1 and message.get("k") == "refresh":
            self.refresh.set()

    async def collect_usage(self) -> UsageSnapshot:
        return await asyncio.to_thread(self.usage.collect)

    async def send_usage(self, transport: BleTransport, snapshot: UsageSnapshot) -> None:
        await transport.send_line(
            usage_line(snapshot, self.sequence.next(), now=int(time.time()))
        )

    async def once(self) -> UsageSnapshot:
        transport = BleTransport(
            address=self.settings.ble_address,
            notification_handler=self._on_device_message,
        )
        try:
            name = await transport.connect()
            LOGGER.info("Connected to %s", name)
            await transport.wait_for_hello()
            cached = self.usage.cached()
            if cached is not None:
                await self.send_usage(transport, cached)
                LOGGER.info("Pushed cached usage")
            snapshot = await self.collect_usage()
            await self.send_usage(transport, snapshot)
            LOGGER.info("Pushed current usage")
            return snapshot
        finally:
            await transport.disconnect()

    async def run(self) -> None:
        backoff = 1
        while True:
            transport = BleTransport(
                address=self.settings.ble_address,
                notification_handler=self._on_device_message,
            )
            try:
                name = await transport.connect()
                LOGGER.info("Connected to %s", name)
                await transport.wait_for_hello()
                backoff = 1
                cached = self.usage.cached()
                if cached is not None:
                    await self.send_usage(transport, cached)
                    LOGGER.info("Pushed cached usage")
                last_refresh = 0.0
                while transport.connected:
                    now = time.monotonic()
                    wait_seconds = max(
                        0.0, self.settings.usage_interval_seconds - (now - last_refresh)
                    )
                    try:
                        await asyncio.wait_for(self.refresh.wait(), timeout=wait_seconds)
                        self.refresh.clear()
                        if time.monotonic() - last_refresh < 5:
                            continue
                    except TimeoutError:
                        pass
                    snapshot = await self.collect_usage()
                    await self.send_usage(transport, snapshot)
                    last_refresh = time.monotonic()
                    LOGGER.info("Pushed current usage")
            except asyncio.CancelledError:
                await transport.disconnect()
                raise
            except Exception as error:  # noqa: BLE001 - persistent service boundary
                LOGGER.warning("Bridge connection cycle failed: %s", error)
            finally:
                await transport.disconnect()
            LOGGER.info("Reconnecting in %ss", backoff)
            await asyncio.sleep(backoff)
            backoff = min(60, backoff * 2)
