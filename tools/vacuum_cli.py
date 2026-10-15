#!/usr/bin/env python3
"""Host side BLE client for the Eggplan Vacuum pump controller.

Usage:
    python tools/vacuum_cli.py scan
    python tools/vacuum_cli.py info
    python tools/vacuum_cli.py status
    python tools/vacuum_cli.py set target 40
    python tools/vacuum_cli.py set timer 600
    python tools/vacuum_cli.py start
    python tools/vacuum_cli.py watch
    python tools/vacuum_cli.py stop

The protocol is documented in docs/ble-protocol.md. The device is a BLE peripheral: it
never initiates, every state it has is either a command it was given or something it had
to do for safety, and every command produces exactly one reply line.
"""

from __future__ import annotations

import argparse
import asyncio
import sys
import time

try:
    from bleak import BleakClient, BleakScanner
except ImportError:  # pragma: no cover - dependency hint only
    sys.exit("bleak is required: python -m pip install -r tools/requirements.txt")

SERVICE_UUID = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f00"
CMD_UUID = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f01"
RSP_UUID = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f02"
STATE_UUID = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f03"
INFO_UUID = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f04"

DEVICE_NAME_PREFIX = "EggplanVac"
CHUNK_MARKER = 0x01
END_MARKER = 0x02


class VacuumClient:
    """One command channel, one reply channel, one telemetry channel."""

    def __init__(self, address: str, verbose: bool = False) -> None:
        self.address = address
        self.verbose = verbose
        self._client = BleakClient(address, timeout=20.0)
        self._queue: asyncio.Queue[str] = asyncio.Queue()
        self._state_queue: asyncio.Queue[str] | None = None
        self._buffers: dict[str, bytearray] = {}

    async def __aenter__(self) -> "VacuumClient":
        await self._client.connect()
        await self._client.start_notify(RSP_UUID, self._on_reply)
        return self

    async def __aexit__(self, *exc_info) -> None:
        if self._client.is_connected:
            for uuid in (RSP_UUID, STATE_UUID):
                try:
                    await self._client.stop_notify(uuid)
                except Exception:
                    pass
            await self._client.disconnect()

    # --- notifications ----------------------------------------------------

    def _reassemble(self, key: str, data: bytearray) -> str | None:
        """Undo the device's chunking, or return a whole line."""
        if not data:
            return None
        if data[0] == CHUNK_MARKER:
            if len(data) < 2:
                return None
            buffer = self._buffers.setdefault(key, bytearray())
            if data[1] == 0:
                buffer.clear()
            buffer.extend(data[2:])
            return None
        if data[0] == END_MARKER:
            buffer = self._buffers.pop(key, bytearray())
            return buffer.decode("utf-8", "replace")
        return bytes(data).decode("utf-8", "replace")

    def _on_reply(self, _characteristic, data: bytearray) -> None:
        text = self._reassemble("rsp", data)
        if text is not None:
            if self.verbose:
                print(f"<- {text}", file=sys.stderr)
            self._queue.put_nowait(text)

    def _on_state(self, _characteristic, data: bytearray) -> None:
        text = self._reassemble("state", data)
        if text is not None and self._state_queue is not None:
            self._state_queue.put_nowait(text)

    # --- commands ---------------------------------------------------------

    async def command(self, line: str, timeout: float = 15.0) -> str:
        """Send one command line and return its single reply line."""
        while not self._queue.empty():
            self._queue.get_nowait()
        await self._client.write_gatt_char(
            CMD_UUID, (line.strip() + "\n").encode(), response=False
        )
        return await asyncio.wait_for(self._queue.get(), timeout)

    async def read_info(self) -> str:
        raw = await self._client.read_gatt_char(INFO_UUID)
        return bytes(raw).decode("utf-8", "replace")

    async def watch(self, period_ms: int | None, seconds: float | None) -> None:
        self._state_queue = asyncio.Queue()
        await self._client.start_notify(STATE_UUID, self._on_state)
        if period_ms is not None:
            reply = await self.command(f"set period {period_ms}")
            if reply.startswith("err"):
                raise SystemExit(reply)
        print("streaming state, Ctrl+C to stop", file=sys.stderr)

        loop = asyncio.get_running_loop()
        deadline = loop.time() + seconds if seconds else None
        while deadline is None or loop.time() < deadline:
            timeout = 1.0 if deadline is None else max(0.1, deadline - loop.time())
            try:
                line = await asyncio.wait_for(self._state_queue.get(), timeout)
            except asyncio.TimeoutError:
                if deadline is not None:
                    break
                continue
            print(f"{time.strftime('%H:%M:%S')} {line}")


async def find_device(address: str | None, name: str | None) -> str:
    if address:
        return address
    print("scanning for an EggplanVac device (10 s)...", file=sys.stderr)
    devices = await BleakScanner.discover(timeout=10.0)
    wanted = name or DEVICE_NAME_PREFIX
    for device in devices:
        if device.name and device.name.startswith(wanted):
            print(f"found {device.name} at {device.address}", file=sys.stderr)
            return device.address
    known = ", ".join(sorted(d.name for d in devices if d.name)) or "none"
    raise SystemExit(f"no EggplanVac device found. Visible devices: {known}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--address", help="BLE address, skips scanning")
    parser.add_argument("--name", help="advertised name prefix to look for")
    parser.add_argument("-v", "--verbose", action="store_true", help="log raw replies")

    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("scan", help="list nearby BLE devices")
    sub.add_parser("info", help="read the info characteristic")
    sub.add_parser("status", help="live status")
    sub.add_parser("start", help="close the valve and pull to the target")
    sub.add_parser("stop", help="stop the pump and vent the chamber")
    sub.add_parser("reset", help="clear the latched fault and vent")
    sub.add_parser("reboot", help="restart the device, then boot safe")
    sub.add_parser("help", help="the device's own command list")
    sub.add_parser("ambient", help="capture the ambient reference now")

    setter = sub.add_parser("set", help="change one setting")
    setter.add_argument("field", choices=("target", "timer", "ambient", "period"))
    setter.add_argument("value", help="kPa, seconds, ms, or off")

    watch = sub.add_parser("watch", help="stream state notifications")
    watch.add_argument("--period", type=int, help="telemetry period in ms")
    watch.add_argument("--seconds", type=float, default=None, help="stop after this long")

    raw = sub.add_parser("raw", help="send a command line verbatim")
    raw.add_argument("line", nargs="+")
    return parser


async def run(args: argparse.Namespace) -> int:
    if args.command == "scan":
        for device in await BleakScanner.discover(timeout=5.0):
            print(f"{device.address}  {device.name or ''}")
        return 0

    address = await find_device(args.address, args.name)
    async with VacuumClient(address, args.verbose) as client:
        if args.command == "watch":
            await client.watch(args.period, args.seconds)
            return 0
        if args.command == "info":
            # The characteristic and the `info` command carry the same payload; reading
            # the characteristic is what a client does on connect.
            print(await client.read_info())
            return 0

        if args.command == "set":
            line = f"set {args.field} {args.value}"
        elif args.command == "raw":
            line = " ".join(args.line)
        else:
            line = args.command

        reply = await client.command(line)
        print(reply)
    return 0 if reply.startswith("ok") else 1


def main() -> int:
    args = build_parser().parse_args()
    try:
        return asyncio.run(run(args))
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
