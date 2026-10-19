# Documentation

## Specs

The contracts the firmware and its tools must satisfy.

- [Serial protocol](./specs/serial-protocol.md) — the wire, domains, replies and errors
- [Gestures](./specs/gestures.md) — the key contract and the press matrix
- [Assets](./specs/assets.md) — the image file format and the tool that makes one
- [Routines](./specs/routines.md) — the Berry routine API and its embedded rules

## Plans

Proposals, design records and deferred work. A plan is not a commitment: it leaves when it is
implemented or dropped.

- [Alpha PCB scope](./plans/alpha-pcb.md) — which parts move onto the board
- [BLE control](./plans/ble-control.md) — design for a BLE GATT contract: characteristics, rates, safety, concurrency and the phased plan
- [Link control](./plans/link-control.md) — connect, assume control, command, release; the panel follows ownership
- [Parking lot](./plans/parking-lot.md) — deferred "what if" ideas, noted not designed

## Guides

User-facing build and parts documentation.

- [Parts list and build guide](./guides/poc-parts.md) — every part, what to buy and what it costs

See the [project README](../README.md) for wiring, build and upload flow, control model and safety
notes.
