# Documentation

## Specs

The contracts the firmware and its tools must satisfy.

The specs written before the panel port are in
[docs/archive/specs-and-plans.zip](./archive/specs-and-plans.zip) — out of date, and read by
nothing when implementing. A spec comes back by being rewritten against the tree and landing in
`docs/specs/` again, where the next one written also goes.

## Backlogs

The detailed breakdown of the plan it implements: ordered, checkable items that each leave the tree
working, independent enough to be moved, reordered or dropped on their own. A backlog breaks work
down rather than choosing or designing it, and it may name its plan. The work is done one item at a
time, with a summary written after each epic and committed on its own; when the last item lands the
backlog and its plan are deleted together, and the commits are what is left of them.

- [The native panel](./backlogs/native-panel.md) — every page native: the settings list, the
  registry, the browser page, the harness and the specs the port left behind

## Guides

User-facing build and parts documentation.

- [Parts list and build guide](./guides/poc-parts.md) — every part, what to buy, what it costs and
  how it is wired

See the [project README](../README.md) for the build and upload flow and the safety notes.
