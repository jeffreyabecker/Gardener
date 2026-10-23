# Documentation

## Specs

The contracts the firmware and its tools must satisfy.

- [Serial protocol](./specs/serial-protocol.md) — the wire, domains, replies and errors
- [Bluetooth](./specs/bluetooth.md) — every characteristic and parameter, ownership and the command
  opcodes
- [Vacuum control](./specs/vacuum-control.md) — the machine: states, transitions and protections
- [Gestures](./specs/gestures.md) — the press classifier, the reserved holds and the scene standards
- [Assets](./specs/assets.md) — the packed mark header and the tool that makes it
- [Berry harness](./specs/berry-harness.md) — the host build that compiles, drives and draws the
  data image without a device
- [Routines](./specs/ui/routine.md) — what a routine is: its lifecycle and embedded rules
- [Bindings](./specs/ui/bindings.md) — what a routine can call: the native modules and records
- [Scenes](./specs/ui/scenes.md) — the gestures each scene binds, and what it does with them

## Backlogs

The detailed breakdown of the plan it implements: ordered, checkable items that each leave the tree
working, independent enough to be moved, reordered or dropped on their own. A backlog breaks work
down rather than choosing or designing it, and it may name its plan. The work is done one item at a
time, with a summary written under `docs/backlogs/completed/` after each epic; when the last item
lands the backlog and its plan are deleted together.

The summaries of the finished ones are in
[docs/backlogs/completed/](./backlogs/completed/) — currently
[the machine without the panel](./backlogs/completed/headless-build/), whose epics split the machine
from the panel into two libraries, made both links pluggable, and shipped a panel-less build with a
capability surface, and [the six-key panel](./backlogs/completed/six-key-panel/), whose epics read
six keys through a shift register, added the gesture classifier and bound the six-key map.

## Guides

User-facing build and parts documentation.

- [Parts list and build guide](./guides/poc-parts.md) — every part, what to buy, what it costs and
  how it is wired

See the [project README](../README.md) for the build and upload flow and the safety notes.
