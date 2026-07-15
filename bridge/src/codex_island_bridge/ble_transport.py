from __future__ import annotations

import asyncio
import json
from collections.abc import Awaitable, Callable
from dataclasses import dataclass
from typing import Any

NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
DEVICE_NAME_PREFIX = "Codex Island-"


@dataclass(frozen=True, slots=True)
class DiscoveredDevice:
    name: str
    address: str
    rssi: int | None


class NotificationLineBuffer:
    def __init__(self, maximum: int = 2048) -> None:
        self._maximum = maximum
        self._buffer = bytearray()
        self._dropping = False

    def feed(self, data: bytes) -> list[dict[str, Any]]:
        messages: list[dict[str, Any]] = []
        for byte in data:
            if byte == 10:
                if not self._dropping and self._buffer:
                    try:
                        value = json.loads(self._buffer.decode("utf-8"))
                        if isinstance(value, dict):
                            messages.append(value)
                    except (UnicodeDecodeError, json.JSONDecodeError):
                        pass
                self._buffer.clear()
                self._dropping = False
            elif not self._dropping:
                if len(self._buffer) >= self._maximum:
                    self._buffer.clear()
                    self._dropping = True
                else:
                    self._buffer.append(byte)
        return messages


async def discover_devices(timeout: float = 8.0) -> list[DiscoveredDevice]:
    from bleak import BleakScanner

    seen = await BleakScanner.discover(timeout=timeout, return_adv=True)
    result: list[DiscoveredDevice] = []
    for _, (device, advertisement) in seen.items():
        name = device.name or advertisement.local_name or ""
        uuids = {uuid.lower() for uuid in (advertisement.service_uuids or [])}
        if name.startswith(DEVICE_NAME_PREFIX) or NUS_SERVICE_UUID in uuids:
            result.append(
                DiscoveredDevice(
                    name=name or "(unnamed NUS device)",
                    address=device.address,
                    rssi=getattr(advertisement, "rssi", None),
                )
            )
    return sorted(result, key=lambda item: item.name)


class BleTransport:
    def __init__(
        self,
        *,
        address: str | None = None,
        notification_handler: Callable[[dict[str, Any]], Awaitable[None] | None] | None = None,
    ) -> None:
        self.address = address
        self.notification_handler = notification_handler
        self._client: Any = None
        self._lines = NotificationLineBuffer()
        self._hello = asyncio.Event()
        self._disconnected = asyncio.Event()

    @property
    def connected(self) -> bool:
        return bool(self._client and self._client.is_connected)

    async def _find(self, timeout: float) -> Any:
        from bleak import BleakScanner

        if self.address:
            device = await BleakScanner.find_device_by_address(self.address, timeout=timeout)
        else:
            target_uuid = NUS_SERVICE_UUID.lower()

            def matches(device: Any, advertisement: Any) -> bool:
                name = device.name or advertisement.local_name or ""
                uuids = {uuid.lower() for uuid in (advertisement.service_uuids or [])}
                return name.startswith(DEVICE_NAME_PREFIX) or target_uuid in uuids

            device = await BleakScanner.find_device_by_filter(matches, timeout=timeout)
        if device is None:
            raise TimeoutError("Codex Island BLE device not found")
        return device

    async def connect(self, timeout: float = 20.0) -> str:
        from bleak import BleakClient

        device = await self._find(timeout)
        self._disconnected.clear()
        self._hello.clear()
        self._client = BleakClient(
            device,
            disconnected_callback=lambda _: self._disconnected.set(),
            services=[NUS_SERVICE_UUID],
            timeout=timeout,
        )
        await self._client.connect()

        async def dispatch(message: dict[str, Any]) -> None:
            if message.get("v") == 1 and message.get("k") == "hello":
                self._hello.set()
            if self.notification_handler is not None:
                result = self.notification_handler(message)
                if asyncio.iscoroutine(result):
                    await result

        def notification(_: Any, data: bytearray) -> None:
            for message in self._lines.feed(bytes(data)):
                asyncio.create_task(dispatch(message))

        await self._client.start_notify(NUS_TX_UUID, notification)
        return device.name or device.address

    async def wait_for_hello(self, timeout: float = 5.0) -> None:
        await asyncio.wait_for(self._hello.wait(), timeout=timeout)

    async def send_line(self, payload: bytes) -> None:
        if not self.connected:
            raise ConnectionError("BLE transport is not connected")
        if not payload.endswith(b"\n"):
            raise ValueError("payload must end with a newline")
        characteristic = self._client.services.get_characteristic(NUS_RX_UUID)
        maximum = min(
            180,
            int(getattr(characteristic, "max_write_without_response_size", 180) or 180),
        )
        for offset in range(0, len(payload), maximum):
            await self._client.write_gatt_char(
                NUS_RX_UUID, payload[offset : offset + maximum], response=True
            )

    async def disconnect(self) -> None:
        if self._client is not None and self._client.is_connected:
            await self._client.disconnect()
        self._client = None

    async def wait_for_disconnect(self) -> None:
        await self._disconnected.wait()

    async def __aenter__(self) -> "BleTransport":
        await self.connect()
        return self

    async def __aexit__(self, *_: object) -> None:
        await self.disconnect()
