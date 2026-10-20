# Documentation

## Specs

The contracts the firmware and its tools must satisfy.

- [Serial protocol](./specs/serial-protocol.md) — the wire, domains, replies and errors
- [Bluetooth](./specs/bluetooth.md) — the GATT table, ownership and the command opcodes
- [Vacuum control](./specs/vacuum-control.md) — the machine: states, transitions and protections
- [Gestures](./specs/gestures.md) — the press classifier, the reserved holds and the page standards
- [Assets](./specs/assets.md) — the image file format and the tool that makes one
- [Routines](./specs/ui/routine.md) — what a routine is: its lifecycle and embedded rules
- [Bindings](./specs/ui/bindings.md) — what a routine can call: the native modules and records

## Plans

Proposals, design records and deferred work. A plan is not a commitment: it leaves when it is
implemented or dropped. Specs, guides and the project README never link to a plan — a contract has
to stand on its own, and plans may reference specs.

- [Alpha PCB scope](./plans/alpha-pcb.md) — which parts move onto the board
- [Parking lot](./plans/parking-lot.md) — deferred "what if" ideas, noted not designed

## Guides

User-facing build and parts documentation.

- [Parts list and build guide](./guides/poc-parts.md) — every part, what to buy and what it costs

See the [project README](../README.md) for wiring, build and upload flow, and safety notes.
