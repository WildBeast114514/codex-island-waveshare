from __future__ import annotations

import asyncio

from codex_island_bridge.service import BridgeService


class FakeTransport:
    def __init__(self) -> None:
        self.disconnected = asyncio.Event()

    async def wait_for_disconnect(self) -> None:
        await self.disconnected.wait()


def test_disconnect_interrupts_refresh_timer() -> None:
    async def scenario() -> None:
        service = object.__new__(BridgeService)
        service.refresh = asyncio.Event()
        transport = FakeTransport()
        waiting = asyncio.create_task(service._wait_for_trigger(transport, 60))
        await asyncio.sleep(0)
        transport.disconnected.set()
        assert await asyncio.wait_for(waiting, timeout=1) == "disconnect"

    asyncio.run(scenario())


def test_refresh_interrupts_timer_and_clears_event() -> None:
    async def scenario() -> None:
        service = object.__new__(BridgeService)
        service.refresh = asyncio.Event()
        transport = FakeTransport()
        service.refresh.set()
        assert await service._wait_for_trigger(transport, 60) == "refresh"
        assert not service.refresh.is_set()

    asyncio.run(scenario())
