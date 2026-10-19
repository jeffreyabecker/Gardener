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
not a rewrite-on-sight target: the `char`-buffer command parsing in `app/serial/SerialLink.cpp` is an
earlier decision that this guidance supersedes for new work, not a task to schedule. The
hand-rolled text builder that used to sit beside it (`core/Text.h`) is gone, so `String` is now
the only way text is formatted in this firmware.

## Keep it simple

Simplicity here is not a taste, it is what the firmware is already built to: a handful of files,
a couple of thousand lines, one loop. It stays that way only if new work extends that shape
instead of building beside it. When a design is larger than the thing it does, the extra is the
bug, and every part of it should have a reason a reviewer can point at.

**KISS, and write it for an Arduino programmer.** The reader knows `setup()` and `loop()`, not a
pattern catalogue. Write the straight-line version they can follow from the top of the function to
the bottom. Don't add a helper function, wrapper, callback or layer of indirection unless you were
told to: each one is another name to chase before the code says what it does. Two repeated lines
are cheaper than a function nobody asked for, and a longer file is fine when it reads plainly.
Work out what the code has to do and write that — don't dither about the design. The `hal/`
boundary described next is a decision already made, not a precedent for the next abstraction.

**One class where one class works, and one file per device.** [VacuumSystem](../libraries/VacuumCore/src/app/VacuumSystem.h)
is the vacuum system *and* the machine, because the chamber's state and the run's state turned out to be
one state: don't split it into a vacuum system and a controller. A device is a different thing — a pin,
a bus and a PWM channel are a boundary — so a part is an abstract class in `hal/` with its
implementation in `hal/impl/`, and the vacuum system takes it by reference even though the vacuum
system is its only caller. Don't fold
a driver into the class that drives it: the register writes and the control law are the two things
here least likely to change together.

Those interfaces are not waiting for a second implementation. They are there because a vendor
header is allowed only where a part is named — `<Adafruit_SH110X.h>` in the composition root, where
the panel is picked — and because the vacuum system reads the same whether the part behind it is a
sensor or nothing at all. An interface with one implementation earns its place exactly when it is a
foreign boundary like that; anywhere else it is a second implementation that does not exist yet.

**Reads go through the owner.** New state is a field on the module that owns it, exposed by an
accessor; `buildAppContext()` builds and owns the parts and hands out the `AppContext` interfaces,
and the serial domains read them when they format a reply, so there is no snapshot or second copy
to keep in step.

**No speculative generality.** One caller means no interface. One implementation means no template,
no `Config.h` flag, no `mode` field for a behaviour nothing uses. Add the second case when the
second case arrives.

**Delete, don't deprecate.** Unused code goes to git — not behind `#if 0`, not in a `legacy/`
folder, not behind a flag defaulted off. The Berry runtime and the host client were both deleted
outright and left to the history ([README](../README.md) Roadmap); do the same.

**A plan is a requirement, not a design.** [docs/plans/](../docs/plans) and
[docs/specs/](../docs/specs) describe work that is proposed, agreed or already done. Read them for
what has to be true, then answer it with the smallest mechanism the current single loop allows.
Reaching for a task, a lock, a queue or a watchdog is a question to ask, not a thing to build.

**A parking-lot entry is a note, not a plan.** [docs/plans/parking-lot.md](../docs/plans/parking-lot.md)
holds unrealized "what if" ideas. One or two sentences say what the idea is and what is blocking
it; the design waits for whoever picks it up. Detail added today is detail the idea has not earned.

**Same behaviour, less surface** is the trade to look for. `PressureSensor` is the current shape
of it: one `kpa()` reading, with the ambient reference and the values derived from it left to the
driver that can actually know them.

**Committing means commit.** When you are told to commit, stage what is in the tree and write the
message: one commit, the change as it stands, in the style the history already uses. Don't re-run
what was verified when the change was made, don't split one request into a tidier series of commits,
and never revert files to rebuild an intermediate state so that a commit looks smaller. Verification
belongs to the change, not to the commit.

This is guidance for new code, and for code you are already changing. Existing structure is not a
rewrite-on-sight target, and an unrequested refactor is its own decision with its own measurement.

## Where to look

- [README.md](../README.md) — wiring, build and upload flow, control model, safety notes.
- [docs/specs/serial-protocol.md](../docs/specs/serial-protocol.md) — the interface the firmware serves.
- [docs/guides/poc-parts.md](../docs/guides/poc-parts.md) — the parts a build is assembled from, with prices.
