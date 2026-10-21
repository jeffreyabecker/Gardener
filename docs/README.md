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

Proposals: what has to be true when the work is done. A plan is not a commitment — it leaves when it
is implemented or dropped, and its requirement is carried into a spec. Specs, guides, backlogs and
the project README never link to a plan; a contract has to stand on its own, and plans may reference
specs.

- [Alpha PCB scope](./plans/alpha-pcb.md) — which parts move onto the board
- [Parking lot](./plans/parking-lot.md) — deferred "what if" ideas, noted not designed

## Ideas

A direction being weighed before it has earned a requirement: the options, what each would cost and
the constraint that set each aside. An idea document commits to nothing, may end in "no", and is
deleted once a plan or a spec carries what it established. Its only entry point is this index —
specs, guides, backlogs, code and the project README never link to one.

- [Page machine](./ideas/page-machine.md) — whether a page should own a machine whose states hold
  `event => (callback, next phase)` records as closures, with the page firing the events

## Backlogs

A fine-grained tasklist for work that has been decided: ordered, checkable items that each leave the
tree working. A backlog breaks work down rather than choosing or designing it, and it is deleted when
its last item lands.

## Guides

User-facing build and parts documentation.

- [Parts list and build guide](./guides/poc-parts.md) — every part, what to buy, what it costs and
  how it is wired

See the [project README](../README.md) for the build and upload flow and the safety notes.
