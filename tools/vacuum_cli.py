#!/usr/bin/env python3
"""Host side BLE client for the Eggplan Vacuum pump controller.

Usage:
    python tools/vacuum_cli.py scan
    python tools/vacuum_cli.py status
    python tools/vacuum_cli.py upload programs/hold_vacuum.be
    python tools/vacuum_cli.py list
    python tools/vacuum_cli.py run hold_vacuum
    python tools/vacuum_cli.py watch

The protocol is documented in docs/ble-protocol.md.
"""

from __future__ import annotations

import argparse
import asyncio
import struct
import sys
import zlib
from pathlib import Path

try:
    from bleak import BleakClient, BleakScanner
except ImportError:  # pragma: no cover - dependency hint only
    sys.exit("bleak is required: python -m pip install -r tools/requirements.txt")

SERVICE_UUID = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f00"
CMD_UUID = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f01"
RSP_UUID = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f02"
PROG_UUID = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f03"
INFO_UUID = "8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f04"

DEVICE_NAME_PREFIX = "EggplanVac"
CHUNK_MARKER = 0x01
END_MARKER = 0x02
UPLOAD_CHUNK = 160
MAX_PROGRAM_BYTES = 64 * 1024

# Replies that are followed by a body and an "eof" line.
MULTILINE_PREFIXES = ("ok file", "ok log", "ok programs")


def sanitize_name(path: str) -> str:
    name = Path(path).stem
    clean = "".join(c for c in name if c.isalnum() or c in "_-")
    if not clean or len(clean) > 31:
        raise SystemExit(f"cannot derive a valid program name from {path!r}")
    return clean


class VacuumClient:
    """Minimal client: one command channel, one notification channel."""

    def __init__(self, address: str, verbose: bool = False) -> None:
        self.address = address
        self.verbose = verbose
        self._client = BleakClient(address, timeout=20.0)
        self._queue: asyncio.Queue[str] = asyncio.Queue()
        self._buffer = bytearray()
        self._telemetry = None

    async def __aenter__(self) -> "VacuumClient":
        await self._client.connect()
        await self._client.start_notify(RSP_UUID, self._on_notify)
        return self

    async def __aexit__(self, *exc_info) -> None:
        if self._client.is_connected:
            try:
                await self._client.stop_notify(RSP_UUID)
            except Exception:
                pass
            await self._client.disconnect()

    def _on_notify(self, _characteristic, data: bytearray) -> None:
        if not data:
            return
        if data[0] == CHUNK_MARKER:
            if len(data) < 2:
                return
            if data[1] == 0:
                self._buffer.clear()
            self._buffer.extend(data[2:])
            return
        if data[0] == END_MARKER:
            text = self._buffer.decode("utf-8", "replace")
            self._buffer.clear()
            self._deliver(text)
            return
        self._deliver(bytes(data).decode("utf-8", "replace"))

    def _deliver(self, text: str) -> None:
        if self.verbose:
            print(f"<- {text}", file=sys.stderr)
        if text.startswith("tel ") and self._telemetry is not None:
            self._telemetry.put_nowait(text[4:])
            return
        self._queue.put_nowait(text)

    async def _next_reply(self, timeout: float) -> str:
        return await asyncio.wait_for(self._queue.get(), timeout)

    async def command(self, line: str, timeout: float = 15.0) -> list[str]:
        """Send one command line and collect its reply (including any body)."""
        while not self._queue.empty():
            self._queue.get_nowait()
        await self._client.write_gatt_char(
            CMD_UUID, (line.strip() + "\n").encode(), response=False
        )
        replies: list[str] = []
        while True:
            text = await self._next_reply(timeout)
            if text.startswith("tel "):
                continue
            replies.append(text)
            if text == "eof":
                break
            if text.startswith("ok ") and text.startswith(MULTILINE_PREFIXES):
                continue
            if text.startswith("ok ") or text.startswith("err "):
                break
        return replies

    async def _prog(self, frame: bytes, timeout: float = 15.0) -> str:
        while not self._queue.empty():
            self._queue.get_nowait()
        await self._client.write_gatt_char(PROG_UUID, frame, response=False)
        while True:
            text = await self._next_reply(timeout)
            if text.startswith("tel "):
                continue
            return text

    async def upload(self, path: str, name: str | None = None) -> str:
        program = name or sanitize_name(path)
        data = Path(path).read_bytes()
        if len(data) > MAX_PROGRAM_BYTES:
            raise SystemExit(f"{path} is larger than {MAX_PROGRAM_BYTES} bytes")

        reply = await self._prog(b"B" + program.encode())
        if not reply.startswith("ok"):
            return reply
        for offset in range(0, len(data), UPLOAD_CHUNK):
            chunk = data[offset : offset + UPLOAD_CHUNK]
            reply = await self._prog(b"D" + chunk)
            if not reply.startswith("ok"):
                return reply
        crc = zlib.crc32(data) & 0xFFFFFFFF
        return await self._prog(b"E" + struct.pack("<II", crc, len(data)))

    async def watch(self, seconds: float | None) -> None:
        self._telemetry = asyncio.Queue()
        await self.command("telemetry on")
        print("streaming telemetry, Ctrl+C to stop")
        loop = asyncio.get_running_loop()
        deadline = loop.time() + seconds if seconds else None
        while deadline is None or loop.time() < deadline:
            timeout = 1.0 if deadline is None else max(0.1, deadline - loop.time())
            try:
                print(await asyncio.wait_for(self._telemetry.get(), timeout))
            except asyncio.TimeoutError:
                if deadline is not None:
                    break


async def find_device(address: str | None, name: str | None) -> str:
    if address:
        return address
    print("scanning for an EggplanVac device (10 s)...")
    devices = await BleakScanner.discover(timeout=10.0)
    wanted = name or DEVICE_NAME_PREFIX
    for device in devices:
        if device.name and device.name.startswith(wanted):
            print(f"found {device.name} at {device.address}")
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
    sub.add_parser("info", help="device information")
    sub.add_parser("status", help="current pressure/pump status")
    sub.add_parser("list", help="stored programs")
    for name in ("ping", "help", "reset", "reboot"):
        sub.add_parser(name)

    cat = sub.add_parser("cat", help="print a stored program")
    cat.add_argument("program")
    cat.add_argument("--offset", type=int, default=0)
    cat.add_argument("--length", type=int, default=0)

    remove = sub.add_parser("rm", help="delete a stored program")
    remove.add_argument("program")

    upload = sub.add_parser("upload", help="upload a program")
    upload.add_argument("path")
    upload.add_argument("--as", dest="name", help="override the program name")

    run = sub.add_parser("run", help="run a stored program")
    run.add_argument("program")
    run.add_argument("args", nargs="*")

    sub.add_parser("stop", help="stop program and pump")
    sub.add_parser("watch", help="stream telemetry").add_argument(
        "--seconds", type=float, default=None
    )

    value_help = {
        "pump": "0-100 duty percent, or off",
        "target": "hold target in kPa, or off",
        "ambient": "ambient reference in kPa, or omit to capture",
        "autostart": "program to run on boot, or none",
        "telemetry": "on, off, or a period in ms",
        "eval": "Berry expression (needs the Berry runtime)",
        "log": "number of log lines to return",
    }
    for name in ("pump", "target", "ambient", "autostart", "telemetry", "eval", "log"):
        child = sub.add_parser(name, help=value_help[name])
        child.add_argument("value", nargs="?", help=value_help[name])
    return parser


async def run(args: argparse.Namespace) -> int:
    if args.command == "scan":
        devices = await BleakScanner.discover(timeout=5.0)
        for device in devices:
            print(f"{device.address}  {device.name or ''}")
        return 0

    address = await find_device(args.address, args.name)
    async with VacuumClient(address, args.verbose) as client:
        if args.command == "watch":
            await client.watch(args.seconds)
            return 0
        if args.command == "upload":
            reply = await client.upload(args.path, args.name)
            print(reply)
            return 0 if reply.startswith("ok") else 1

        if args.command == "cat":
            line = f"cat {args.program} {args.offset} {args.length}"
        elif args.command == "rm":
            line = f"rm {args.program}"
        elif args.command == "run":
            line = "run " + " ".join([args.program] + args.args)
        elif args.command in ("pump", "target", "ambient", "autostart", "telemetry", "eval", "log"):
            line = args.command + (f" {args.value}" if args.value else "")
        else:
            line = args.command

        replies = await client.command(line)
        for reply in replies:
            print(reply)
        return 0 if replies and replies[-1].startswith("ok") else 1


def main() -> int:
    args = build_parser().parse_args()
    try:
        return asyncio.run(run(args))
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
