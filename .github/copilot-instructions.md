# Project Guidelines

## Build

`powershell -File scripts/arduino.ps1 -Task build` — compiles the sketch with the project
local library. There is no host test suite, and nothing checks the layer rule, so a stray
`hal/` → `app/` include is a review catch rather than a build failure.

## Conventions

**Don't reinvent the wheel.** This is an Arduino/ESP32 project, and the core already
provides the utilities this firmware needs. Prefer them to hand-rolled equivalents.

- **String handling**: use Arduino `String` and its methods — `substring`, `trim`,
  `indexOf`, `lastIndexOf`, `toInt`, `toFloat`, `toLowerCase`, `startsWith`, `+` — for
  parsing command lines, building replies and formatting values. It is good enough, it is
  what the rest of the ecosystem uses, and a hand-written tokeniser or fixed `char` buffer
  buys very little on an ESP32-S3 with hundreds of kilobytes of free heap.
- The same applies to other core facilities: reach for the platform's own helper rather
  than writing a bespoke one.
- Hand-roll only with a reason you can point at — a hard real-time path, a proven hot loop,
  a measured memory ceiling — and say what the reason is in a comment.

This applies to new code and to code you are already changing. Existing hand-rolled code is
not a rewrite-on-sight target: the `char`-buffer command parsing in `app/SerialLink.cpp` is an
earlier decision that this guidance supersedes for new work, not a task to schedule. The
hand-rolled text builder that used to sit beside it (`core/Text.h`) is gone, so `String` is now
the only way text is formatted in this firmware.

## Keep it simple

Simplicity here is not a taste, it is what the firmware is already built to: a handful of files,
a couple of thousand lines, one loop. It stays that way only if new work extends that shape
instead of building beside it. When a design is larger than the thing it does, the extra is the
bug, and every part of it should have a reason a reviewer can point at.

**One class where one class works.** [VacuumSystem](../libraries/VacuumCore/src/hal/VacuumSystem.h)
is the plant, the machine and the three devices it drives — the sensor, the pump and the valve —
because the chamber's state and the run's state turned out to be one state, and each of those
devices has exactly one user. Don't split it into a plant, a controller and a driver per device, and
don't add a base class, an interface or a policy object for a second implementation that does not
exist.

**A value beats a mechanism.** New state is a field on
[PlantState](../libraries/VacuumCore/src/core/Snapshot.h) that the loop fills each pass — not a
getter per fact, not a callback, not an observer, not an event queue. The reads are
`VacuumSystem::state()` and the one snapshot; if a change wants a new public getter for something,
carry it in the snapshot instead.

**No speculative generality.** One caller means no interface. One implementation means no template,
no `Config.h` flag, no `mode` field for a behaviour nothing uses. Add the second case when the
second case arrives.

**Delete, don't deprecate.** Unused code goes to git — not behind `#if 0`, not in a `legacy/`
folder, not behind a flag defaulted off. The Berry runtime and the host client were both deleted
outright and left to the history ([README](../README.md) Roadmap); do the same.

**A plan is a requirement, not a design.** [docs/berry-on-device-control.md](../docs/berry-on-device-control.md)
and [docs/specs/](../docs/specs) describe a scripting build with a VM on the second core, three
tasks, a heartbeat watchdog and task restart. Read them for what has to be true, then answer it with
the smallest mechanism the current single loop allows. Reaching for a task, a lock, a queue or a
watchdog is a question to ask, not a thing to build.

**Same behaviour, less surface** is the trade to look for. `VacuumSystem::state()` and
`PlantState` are the current shape of it: fifteen one-by-one getters became one read of one plain
value, with the wire bytes unchanged.

**Committing means commit.** When you are told to commit, stage what is in the tree and write the
message: one commit, the change as it stands, in the style the history already uses. Don't re-run
what was verified when the change was made, don't split one request into a tidier series of commits,
and never revert files to rebuild an intermediate state so that a commit looks smaller. Verification
belongs to the change, not to the commit.

This is guidance for new code, and for code you are already changing. Existing structure is not a
rewrite-on-sight target, and an unrequested refactor is its own decision with its own measurement.

## Where to look

- [README.md](../README.md) — wiring, build and upload flow, control model, safety notes.
- [docs/serial-protocol.md](../docs/serial-protocol.md) — the interface the firmware serves.
- [docs/bom.md](../docs/bom.md) — the parts the firmware's limits and pin map assume.
