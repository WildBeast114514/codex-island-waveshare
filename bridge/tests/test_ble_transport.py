import asyncio

import pytest

from codex_island_bridge.ble_transport import (
    NUS_RX_UUID,
    BleOperationTimeout,
    BleTransport,
    NotificationLineBuffer,
)


def test_notification_lines_reassemble_across_chunks() -> None:
    buffer = NotificationLineBuffer()
    assert buffer.feed(b'{"v":1,') == []
    assert buffer.feed(b'"k":"hello"}\n') == [{"v": 1, "k": "hello"}]


def test_notification_overflow_drops_only_the_bad_line() -> None:
    buffer = NotificationLineBuffer(maximum=5)
    assert buffer.feed(b"123456789\n") == []
    assert buffer.feed(b"{}\n") == [{}]


class _Characteristic:
    max_write_without_response_size = 180


class _Services:
    def get_characteristic(self, uuid: str) -> _Characteristic:
        assert uuid == NUS_RX_UUID
        return _Characteristic()


class _HungWriteClient:
    is_connected = True
    services = _Services()

    async def write_gatt_char(self, *_: object, **__: object) -> None:
        await asyncio.Event().wait()


def test_gatt_write_has_a_hard_timeout() -> None:
    async def scenario() -> None:
        transport = BleTransport(operation_timeout=0.01)
        transport._client = _HungWriteClient()
        with pytest.raises(BleOperationTimeout, match="BLE write timed out"):
            await asyncio.wait_for(transport.send_line(b"{}\n"), timeout=0.5)

    asyncio.run(scenario())


class _HungDisconnectClient:
    is_connected = True

    async def disconnect(self) -> None:
        await asyncio.Event().wait()


def test_disconnect_timeout_still_clears_transport() -> None:
    async def scenario() -> None:
        transport = BleTransport(operation_timeout=0.01)
        transport._client = _HungDisconnectClient()
        await asyncio.wait_for(transport.disconnect(), timeout=0.5)
        assert not transport.connected
        await asyncio.wait_for(transport.wait_for_disconnect(), timeout=0.1)

    asyncio.run(scenario())
