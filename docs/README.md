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

<< DO NOT EVER LIST BACKLOG OR PLAN FILES IN THIS DOCUMENT >>
Backlogs and plans are transient documents which will be updated and deleted constantly 
listing them anywhere is forbidden

## Guides

User-facing build, parts and tool documentation.

- [Parts list and build guide](./guides/poc-parts.md) — every part, what to buy, what it costs and
  how it is wired
- [Panel walkthroughs](./guides/walkthroughs.md) — a command file that drives the panel on the host
  and writes it as a GIF

## Reference

The controllers the panel is driven through, mapped for someone writing or debugging a driver: bus
address, control bytes, the command table and its reset values, and the sequence a 128x64 module
takes.

- [SH1106 I2C map](./reference/sh1106-i2c.md) — the controller on the 1.3" panel: page addressing,
  the 132-column RAM and its 2-column offset, and the init sequence the driver sends
- [SSD1306 I2C map](./reference/ssd1306-i2c.md) — the other 128x64 controller: three addressing
  modes, the charge pump, hardware scrolling, and how it differs from the SH1106
- [Telling the two panels apart](./reference/panel-identification.md) — why nothing read from the
  bus identifies either part, and the probes that do, with what each result means

See the [project README](../README.md) for the build and upload flow and the safety notes.
