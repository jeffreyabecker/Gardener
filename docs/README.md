# Documentation

## Specs

The contracts the firmware and its tools must satisfy.

- [Serial protocol](./specs/serial-protocol.md) — the wire, domains, replies and errors
- [Bluetooth](./specs/bluetooth.md) — every characteristic, parameter and command opcode
- [Routine links](./specs/routine-links.md) — the routine surface the serial line and the radio
  serve
- [Vacuum control](./specs/vacuum-control.md) — the machine: states, transitions and protections
- [Gestures](./specs/gestures.md) — the press classifier, the reserved holds and the scene standards
- [Assets](./specs/assets.md) — the packed mark header and the tool that makes it
- [Graphics](./specs/graphics.md) — the drawing library: the buffered frame, the device that
  flushes it, and the canvas that draws into an extent
- [Berry harness](./specs/berry-harness.md) — the host build that compiles, drives and draws the
  data image without a device
- [Routines](./specs/ui/routine.md) — what a routine is: its lifecycle and embedded rules
- [Bindings](./specs/ui/bindings.md) — what a routine can call: the native modules and records
- [Scenes](./specs/ui/scenes.md) — the gestures each scene binds, and what it does with them
- [Components](./specs/ui/components.md) — the drawing primitives a page composes, and the layout
  that places them

## Backlogs

The detailed breakdown of the plan it implements: ordered, checkable items that each leave the tree
working, independent enough to be moved, reordered or dropped on their own. A backlog breaks work
down rather than choosing or designing it, and it may name its plan. The work is done one item at a
time, with a summary written under `docs/backlogs/completed/` after each epic; when the last item
lands the backlog and its plan are deleted together.

- [The native panel](./backlogs/native-panel.md) — every page native: the settings list, the
  registry, the browser page, the harness and the specs the port left behind

Summaries of the finished ones are in
[docs/backlogs/completed/](./backlogs/completed/) —
[the machine without the panel](./backlogs/completed/headless-build/), whose epics split the machine
from the panel into two libraries, made both links pluggable, and shipped a panel-less build with a
capability surface; [the six-key panel](./backlogs/completed/six-key-panel/), whose epics read
six keys through a shift register, added the gesture classifier and bound the six-key map;
[safety hardening](./backlogs/completed/safety-hardening/), whose epics made the device's safe path
independent of the controller; [the component and layout framework](./backlogs/completed/components-layout/),
whose epics built the box-and-flip component, the compiled-first text and the flat layout the pages
drew with in its day; and [relative layout](./backlogs/completed/relative-layout/), whose epics
replaced that flat list with a layout that resolves each child's declared bounds, moved the three
chromes onto it, and stopped components and scenes taking their box at construction.

## Guides

User-facing build and parts documentation.

- [Parts list and build guide](./guides/poc-parts.md) — every part, what to buy, what it costs and
  how it is wired

See the [project README](../README.md) for the build and upload flow and the safety notes.
