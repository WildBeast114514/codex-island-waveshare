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
from .models import RadarModel, RadarSnapshot, UsageSnapshot
from .protocol import Sequence, radar_line, usage_line
from .radar import (
    AuthorizedApiRadarProvider,
    HtmlRadarProvider,
    NotModified,
    RadarProvider,
    RadarProviderError,
)
from .radar_history import RadarHistory

LOGGER = logging.getLogger(__name__)
RADAR_STALE_SECONDS = 18 * 60 * 60
RADAR_MIN_REQUEST_SECONDS = 30 * 60


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


def _radar_from_dict(value: Any) -> RadarSnapshot | None:
    if not isinstance(value, dict) or not isinstance(value.get("models"), list):
        return None
    try:
        models = tuple(
            RadarModel(
                key=str(model["key"]),
                family=str(model["family"]),
                effort=str(model["effort"]),
                iq_x10=int(model["iq_x10"]),
                passed=int(model["passed"]),
                total=int(model["total"]),
                source_order=int(model.get("source_order", order)),
            )
            for order, model in enumerate(value["models"])
        )
        if not models:
            return None
        return RadarSnapshot(
            updated_at=int(value["updated_at"]),
            models=models,
            stale=bool(value.get("stale", False)),
            trend_iq_x10=tuple(int(item) for item in value.get("trend_iq_x10", ())),
        )
    except (KeyError, TypeError, ValueError):
        return None


class RadarService:
    def __init__(self, settings: Settings, provider: RadarProvider | None = None) -> None:
        self.cache = AtomicJsonFile(settings.data_dir / "radar_cache.json", schema_version=1)
        self.history = RadarHistory(
            AtomicJsonFile(settings.data_dir / "radar_history.json", schema_version=1),
            preferred_key=settings.radar_primary_key,
        )
        if provider is not None:
            self.provider = provider
        elif settings.radar_api_url:
            self.provider = AuthorizedApiRadarProvider(
                settings.radar_api_url, token=settings.radar_api_token
            )
        elif settings.radar_allow_html:
            self.provider = HtmlRadarProvider()
        else:
            self.provider = None

    def _record(self) -> dict[str, Any]:
        value = self.cache.load(default={})
        return value if isinstance(value, dict) else {}

    @staticmethod
    def _with_stale(snapshot: RadarSnapshot, now: int, trend: tuple[int, ...]) -> RadarSnapshot:
        return dataclasses.replace(
            snapshot,
            stale=now - snapshot.updated_at > RADAR_STALE_SECONDS,
            trend_iq_x10=trend,
        )

    def cached(self, *, now: int | None = None) -> RadarSnapshot | None:
        current = int(time.time()) if now is None else now
        snapshot = _radar_from_dict(self._record().get("snapshot"))
        if snapshot is None:
            return None
        return self._with_stale(snapshot, current, self.history.trend())

    def _save_record(self, record: dict[str, Any]) -> None:
        self.cache.save(record)

    def collect(self, *, now: int | None = None, force: bool = False) -> RadarSnapshot | None:
        current = int(time.time()) if now is None else now
        record = self._record()
        cached = _radar_from_dict(record.get("snapshot"))
        if self.provider is None:
            return (
                self._with_stale(cached, current, self.history.trend())
                if cached is not None
                else None
            )
        last_attempt = int(record.get("last_attempt_at", 0))
        next_retry = int(record.get("next_retry_at", 0))
        if not force and (
            current - last_attempt < RADAR_MIN_REQUEST_SECONDS or current < next_retry
        ):
            return (
                self._with_stale(cached, current, self.history.trend())
                if cached is not None
                else None
            )

        self.provider.etag = record.get("etag")
        self.provider.last_modified = record.get("last_modified")
        record["last_attempt_at"] = current
        try:
            snapshot = self.provider.fetch()
        except NotModified:
            record["failures"] = 0
            record["next_retry_at"] = 0
            self._save_record(record)
            return (
                self._with_stale(cached, current, self.history.trend())
                if cached is not None
                else None
            )
        except RadarProviderError:
            failures = int(record.get("failures", 0)) + 1
            record["failures"] = failures
            record["next_retry_at"] = current + min(3600, 60 * (2 ** min(failures - 1, 6)))
            self._save_record(record)
            if cached is not None:
                LOGGER.warning("Radar refresh failed; retaining the last valid cache")
                return self._with_stale(cached, current, self.history.trend())
            raise

        self.history.record(snapshot)
        result = self._with_stale(snapshot, current, self.history.trend())
        record.update(
            {
                "snapshot": asdict(result),
                "etag": self.provider.etag,
                "last_modified": self.provider.last_modified,
                "last_success_at": current,
                "failures": 0,
                "next_retry_at": 0,
            }
        )
        self._save_record(record)
        return result


class BridgeService:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings
        self.usage = UsageService(settings)
        self.radar = RadarService(settings)
        self.sequence = Sequence()
        self.refresh = asyncio.Event()

    async def _on_device_message(self, message: dict[str, Any]) -> None:
        if message.get("v") == 1 and message.get("k") == "refresh":
            self.refresh.set()

    async def _wait_for_trigger(self, transport: BleTransport, timeout: float) -> str:
        refresh_wait = asyncio.create_task(self.refresh.wait())
        disconnect_wait = asyncio.create_task(transport.wait_for_disconnect())
        tasks = {refresh_wait, disconnect_wait}
        try:
            done, _ = await asyncio.wait(
                tasks, timeout=timeout, return_when=asyncio.FIRST_COMPLETED
            )
            if disconnect_wait in done:
                return "disconnect"
            if refresh_wait in done:
                self.refresh.clear()
                return "refresh"
            return "timeout"
        finally:
            for task in tasks:
                if not task.done():
                    task.cancel()
            await asyncio.gather(*tasks, return_exceptions=True)

    async def collect_usage(self) -> UsageSnapshot:
        return await asyncio.to_thread(self.usage.collect)

    async def send_usage(self, transport: BleTransport, snapshot: UsageSnapshot) -> None:
        await transport.send_line(
            usage_line(snapshot, self.sequence.next(), now=int(time.time()))
        )

    async def collect_radar(self, *, force: bool = False) -> RadarSnapshot | None:
        return await asyncio.to_thread(self.radar.collect, force=force)

    async def send_radar(self, transport: BleTransport, snapshot: RadarSnapshot) -> None:
        await transport.send_line(radar_line(snapshot, self.sequence.next()))

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
            cached_radar = self.radar.cached()
            if cached_radar is not None:
                await self.send_radar(transport, cached_radar)
                LOGGER.info("Pushed cached Radar")
            snapshot = await self.collect_usage()
            await self.send_usage(transport, snapshot)
            LOGGER.info("Pushed current usage")
            radar = await self.collect_radar()
            if radar is not None:
                await self.send_radar(transport, radar)
                LOGGER.info("Pushed current Radar")
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
                cached_radar = self.radar.cached()
                if cached_radar is not None:
                    await self.send_radar(transport, cached_radar)
                    LOGGER.info("Pushed cached Radar")
                last_usage_refresh = 0.0
                last_radar_refresh = 0.0
                while transport.connected:
                    now = time.monotonic()
                    usage_due = max(
                        0.0,
                        self.settings.usage_interval_seconds - (now - last_usage_refresh),
                    )
                    radar_due = max(
                        0.0,
                        self.settings.radar_interval_seconds - (now - last_radar_refresh),
                    )
                    trigger = await self._wait_for_trigger(
                        transport, min(usage_due, radar_due)
                    )
                    if trigger == "disconnect":
                        LOGGER.info("Peripheral disconnected")
                        break
                    if trigger == "refresh":
                        if time.monotonic() - min(last_usage_refresh, last_radar_refresh) < 5:
                            continue
                        usage_due = radar_due = 0
                    now = time.monotonic()
                    if usage_due <= 0 or now - last_usage_refresh >= self.settings.usage_interval_seconds:
                        snapshot = await self.collect_usage()
                        await self.send_usage(transport, snapshot)
                        last_usage_refresh = now
                        LOGGER.info("Pushed current usage")
                    if radar_due <= 0 or now - last_radar_refresh >= self.settings.radar_interval_seconds:
                        radar = await self.collect_radar()
                        if radar is not None:
                            await self.send_radar(transport, radar)
                            LOGGER.info("Pushed current Radar")
                        last_radar_refresh = now
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
