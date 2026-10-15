# Project Guidelines

## Build

`powershell -File scripts/arduino.ps1 -Task build` — compiles the sketch with the project
local library, and runs `scripts/check-layers.ps1` first, so a layer violation fails the
build rather than a later review. There is no host test suite.

## Conventions

**Don't reinvent the wheel.** This is an Arduino/ESP32 project, and the core already
provides the utilities this firmware needs. Prefer them to hand-rolled equivalents.

- **String handling**: use Arduino `String` and its methods — `substring`, `trim`,
  `indexOf`, `lastIndexOf`, `toInt`, `toFloat`, `toLowerCase`, `startsWith`, `+` — for
  parsing command lines, building replies and formatting values. It is good enough, it is
  what the rest of the ecosystem uses, and a hand-written tokeniser or fixed `char` buffer
  buys very little on an ESP32-C3 with hundreds of kilobytes of free heap.
- The same applies to other core facilities: reach for the platform's own helper rather
  than writing a bespoke one.
- Hand-roll only with a reason you can point at — a hard real-time path, a proven hot loop,
  a measured memory ceiling — and say what the reason is in a comment.

This applies to new code and to code you are already changing. Existing hand-rolled code is
not a rewrite-on-sight target: `core/Text.h` and the `char`-buffer command parsing are
earlier decisions that this guidance supersedes for new work, not a task to schedule.

## Where to look

- [README.md](../README.md) — wiring, build and upload flow, control model, safety notes.
- [docs/serial-protocol.md](../docs/serial-protocol.md) — the interface the firmware serves.
- `scripts/check-layers.ps1` — the three layer dependency rule, enforced at build time.
