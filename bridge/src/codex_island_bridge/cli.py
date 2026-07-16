from __future__ import annotations

import argparse
import asyncio
import json
import logging
import sys

from .ble_transport import BleTransport, discover_devices
from .codex_pet import CodexPetProvider
from .config import Settings
from .protocol import (
    Sequence,
    distributed_radar_line,
    mock_snapshots,
    pet_line,
    radar_line,
    usage_line,
)
from .service import (
    BridgeService,
    DistributedRadarService,
    RadarService,
    UsageService,
)


async def _devices(timeout: float) -> int:
    devices = await discover_devices(timeout)
    if not devices:
        print("No Codex Island BLE device found")
        return 1
    for device in devices:
        strength = "?" if device.rssi is None else str(device.rssi)
        print(f"{device.name}\t{device.address}\tRSSI {strength}")
    return 0


async def _mock_push(address: str | None) -> int:
    sequence = Sequence()
    usage, radar = mock_snapshots()
    transport = BleTransport(address=address)
    try:
        name = await transport.connect()
        print(f"Connected to {name}")
        await transport.wait_for_hello()
        print("Received hello")
        await transport.send_line(usage_line(usage, sequence.next()))
        await transport.send_line(radar_line(radar, sequence.next()))
        print("Pushed mock usage and radar")
        return 0
    finally:
        await transport.disconnect()


def _print_mock() -> int:
    sequence = Sequence()
    usage, radar = mock_snapshots()
    for payload in (usage_line(usage, sequence.next()), radar_line(radar, sequence.next())):
        print(json.dumps(json.loads(payload), ensure_ascii=False, indent=2))
    return 0


def _print_real(settings: Settings) -> int:
    snapshot = UsageService(settings).collect()
    payload = usage_line(snapshot, 1)
    print(json.dumps(json.loads(payload), ensure_ascii=False, indent=2))
    return 0


def _radar_test(settings: Settings) -> int:
    snapshot = RadarService(settings).collect(force=True)
    if snapshot is None:
        print(
            "Radar is not configured; set CODEX_RADAR_API_URL or CODEX_RADAR_ALLOW_HTML=1",
            file=sys.stderr,
        )
        return 2
    print(json.dumps(json.loads(radar_line(snapshot, 1)), ensure_ascii=False, indent=2))
    return 0


def _pet_status(settings: Settings) -> int:
    snapshot = CodexPetProvider(settings.codex_sessions_dir).collect()
    print(json.dumps(json.loads(pet_line(snapshot, 1)), ensure_ascii=False, indent=2))
    return 0


def _distributed_test(settings: Settings) -> int:
    snapshot = DistributedRadarService(settings).collect()
    if snapshot is None:
        print("Distributed Radar is unavailable and no cache exists", file=sys.stderr)
        return 2
    print(
        json.dumps(
            json.loads(distributed_radar_line(snapshot, 1)),
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0


async def _once(settings: Settings) -> int:
    await BridgeService(settings).once()
    return 0


async def _run(settings: Settings) -> int:
    await BridgeService(settings).run()
    return 0


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(prog="codex-island-bridge")
    commands = root.add_subparsers(dest="command", required=True)
    devices = commands.add_parser("devices", help="scan for Codex Island BLE peripherals")
    devices.add_argument("--timeout", type=float, default=8.0)
    mock = commands.add_parser("mock-push", help="push deterministic test data over BLE")
    mock.add_argument("--address")
    commands.add_parser("print-mock", help="print deterministic protocol messages")
    commands.add_parser("print", help="collect and print real Codex usage")
    commands.add_parser("once", help="connect, push current data, and exit")
    commands.add_parser("run", help="run the reconnecting background bridge")
    commands.add_parser("radar-test", help="print the deterministic Radar protocol fixture")
    commands.add_parser(
        "distributed-test", help="collect and print live Distributed Radar IQ"
    )
    commands.add_parser("pet-status", help="print the inferred local Codex pet state")
    return root


def main(argv: list[str] | None = None) -> None:
    args = parser().parse_args(argv)
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )
    settings = Settings.from_environment()
    try:
        if args.command == "devices":
            code = asyncio.run(_devices(args.timeout))
        elif args.command == "mock-push":
            code = asyncio.run(_mock_push(args.address))
        elif args.command == "print":
            code = _print_real(settings)
        elif args.command == "once":
            code = asyncio.run(_once(settings))
        elif args.command == "run":
            code = asyncio.run(_run(settings))
        elif args.command == "radar-test":
            code = _radar_test(settings)
        elif args.command == "distributed-test":
            code = _distributed_test(settings)
        elif args.command == "pet-status":
            code = _pet_status(settings)
        else:
            code = _print_mock()
    except (TimeoutError, ConnectionError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        code = 2
    raise SystemExit(code)
