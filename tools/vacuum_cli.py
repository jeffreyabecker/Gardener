#!/usr/bin/env python3
"""Host side serial controller for the Eggplan Vacuum pump.

Usage:
    python tools/vacuum_cli.py --list
    python tools/vacuum_cli.py info
    python tools/vacuum_cli.py status
    python tools/vacuum_cli.py set target 40
    python tools/vacuum_cli.py start
    python tools/vacuum_cli.py set hold 572
    python tools/vacuum_cli.py watch
    python tools/vacuum_cli.py stop

The protocol is documented in docs/serial-protocol.md. The device is a serial peripheral:
every command produces exactly one reply line, lines starting with '[' are diagnostics
rather than replies, and a readout is polled rather than pushed. The device keeps no clock, so
the hold's duration is this program's business: time it here and send `stop` when it is done.
"""

from __future__ import annotations

import argparse
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover - dependency hint only
    sys.exit("pyserial is required: python -m pip install -r tools/requirements.txt")

BAUD = 115200
DEFAULT_TIMEOUT = 5.0


class Controller:
    """One command channel: write a line, read the reply line."""

    def __init__(self, port: str, baud: int = BAUD, echo: bool = False) -> None:
        self.port = port
        self.echo = echo
        self._serial = serial.Serial(port, baud, timeout=0.2)
        time.sleep(0.1)  # let the port settle before the first command

    def __enter__(self) -> "Controller":
        return self

    def __exit__(self, *exc_info) -> None:
        self._serial.close()

    def send(self, line: str) -> None:
        if self.echo:
            print(f"-> {line}", file=sys.stderr)
        self._serial.write((line.strip() + "\n").encode("ascii", "replace"))
        self._serial.flush()

    def read_reply(self, timeout: float = DEFAULT_TIMEOUT) -> str:
        """The next line that is a reply rather than a diagnostic record."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            raw = self._serial.readline()
            text = raw.decode("utf-8", "replace").strip()
            if not text:
                continue
            if text[0] == "[":
                print(text, file=sys.stderr)
                continue
            if self.echo:
                print(f"<- {text}", file=sys.stderr)
            return text
        raise TimeoutError(f"no reply from {self.port}")

    def command(self, line: str, timeout: float = DEFAULT_TIMEOUT) -> str:
        self.send(line)
        return self.read_reply(timeout)


def find_port(requested: str | None) -> str:
    if requested:
        return requested
    ports = list(list_ports.comports())
    if not ports:
        raise SystemExit("no serial ports found; pass --port")
    # Prefer a USB device: the ESP32-C3 native USB shows up as one, and a machine usually
    # has a few unrelated COM ports.
    usb = [p for p in ports if p.device and "USB" in (p.description or "").upper()]
    if len(usb) == 1:
        print(f"using {usb[0].device} ({usb[0].description})", file=sys.stderr)
        return usb[0].device
    if len(ports) == 1:
        print(f"using {ports[0].device} ({ports[0].description})", file=sys.stderr)
        return ports[0].device
    listing = "\n".join(f"  {p.device}  {p.description or ''}" for p in ports)
    raise SystemExit(f"several ports, pass --port:\n{listing}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", help="serial port, e.g. COM5 or /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=BAUD, help=f"baud rate (default {BAUD})")
    parser.add_argument("--list", action="store_true", help="list serial ports and exit")
    parser.add_argument("--echo", action="store_true", help="log commands and replies")

    sub = parser.add_subparsers(dest="command")
    for name, help_text in (
        ("info", "device identity, including the firmware version"),
        ("status", "live status"),
        ("start", "close the valve and pull to the target"),
        ("stop", "stop the pump and vent the chamber"),
        ("reset", "clear the latched fault and vent"),
        ("reboot", "restart the device, then boot safe"),
        ("help", "the device's own command list"),
        ("ambient", "capture the current reading as the ambient reference"),
    ):
        sub.add_parser(name, help=help_text)

    setter = sub.add_parser("set", help="change one setting")
    setter.add_argument("field", choices=("target", "hold", "ambient", "log"))
    setter.add_argument("value", help="kPa, seconds, on|off")

    watch = sub.add_parser("watch", help="poll status and print it")
    watch.add_argument("--period", type=float, default=0.25, help="seconds between polls")
    watch.add_argument("--seconds", type=float, default=None, help="stop after this long")

    raw = sub.add_parser("raw", help="send a command line verbatim")
    raw.add_argument("line", nargs="+")
    return parser


def watch(controller: Controller, period: float, seconds: float | None) -> int:
    print(f"polling every {period:.2f}s, Ctrl+C to stop", file=sys.stderr)
    deadline = time.monotonic() + seconds if seconds else None
    try:
        while deadline is None or time.monotonic() < deadline:
            started = time.monotonic()
            print(f"{time.strftime('%H:%M:%S')} {controller.command('status')}")
            remaining = period - (time.monotonic() - started)
            if remaining > 0:
                time.sleep(remaining)
    except KeyboardInterrupt:
        return 130
    return 0


def main() -> int:
    args = build_parser().parse_args()

    if args.list:
        for port in list_ports.comports():
            print(f"{port.device}  {port.description or ''}")
        return 0

    if not args.command:
        build_parser().print_help()
        return 2

    with Controller(find_port(args.port), args.baud, args.echo) as controller:
        if args.command == "watch":
            return watch(controller, args.period, args.seconds)

        if args.command == "set":
            line = f"set {args.field} {args.value}"
        elif args.command == "raw":
            line = " ".join(args.line)
        else:
            line = args.command

        reply = controller.command(line)
        print(reply)
    return 0 if reply.startswith("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())
