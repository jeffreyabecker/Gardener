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

- [fs serial domain](./plans/serial-protocol-fs.md) — implementation plan
- [Alpha PCB scope](./plans/alpha-pcb.md) — which parts move onto the board
- [Parking lot](./plans/parking-lot.md) — deferred work and what has to be true first

## Guides

User-facing build and parts documentation.

- [Parts list and build guide](./guides/poc-parts.md) — every part, what to buy and what it costs

See the [project README](../README.md) for wiring, build and upload flow, control model and safety
notes.
