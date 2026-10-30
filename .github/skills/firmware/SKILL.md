---
name: firmware
description: 'Design conventions for the Gardener ESP32 firmware. Use when writing, changing, refactoring or reviewing code under libraries/ or firmware/: Arduino String and core utilities, the core/ layer rule, one class where one class works, one file per device, no speculative generality, and deleting rather than deprecating. Naming and formatting are in the cpp skill.'
---

# Firmware

## When to use

- Adding or changing anything under `libraries/Gardener/` or `firmware/gardener/`
- Deciding where a new type, a driver or an interface belongs
- Reviewing a firmware change

How the code is written — naming, formatting and the type rules — is the
[cpp skill](../cpp/SKILL.md)'s, and is not repeated here.

## Don't reinvent the wheel

This is an Arduino/ESP32 project, and the core already provides the utilities this firmware needs.
Prefer them to hand-rolled equivalents.

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
not a rewrite-on-sight target: the `char`-buffer command parsing in `serial/SerialLink.cpp` is an
earlier decision that this guidance supersedes for new work, not a task to schedule. The
hand-rolled text builder that used to sit beside it (`core/Text.h`) is gone, so `String` is now
the only way text is formatted in this firmware.

## Keep it simple

Simplicity here is not a taste, it is what the firmware is already built to: an Arduino/ESP32-S3
sketch that hands its parts to a few tasks and lets `loop()` delete itself. It stays that way only if
new work extends that shape instead of building beside it. When a design is larger than the thing it
does, the extra is the bug, and every part of it should have a reason a reviewer can point at.

**KISS, and write it for an Arduino programmer.** The reader knows `setup()` and `loop()`, not a
pattern catalogue. Write the straight-line version they can follow from the top of the function to
the bottom. Don't add a helper function, wrapper, callback or layer of indirection unless you were
told to: each one is another name to chase before the code says what it does. Two repeated lines
are cheaper than a function nobody asked for, and a longer file is fine when it reads plainly.
Work out what the code has to do and write that — don't dither about the design. The part boundary
described next is a decision already made, not a precedent for the next abstraction.

**One class where one class works, and one file per device.**
`libraries/Gardener/src/machine/VacuumSystem.h` is the vacuum system *and* the machine, because the
chamber's state and the run's state turned out to be one state: don't split it into a vacuum system
and a controller. A device is a different thing — a pin, a bus and a PWM channel are a boundary — so
a part is an abstract class in `libraries/Gardener/src/core/` with its implementation in
`libraries/Runtime/src/hal/impl/`, and the vacuum system takes it by reference even though the vacuum
system is its only caller. Don't fold a driver into the class that drives it: the register writes
and the control law are the two things here least likely to change together.

Those interfaces are not waiting for a second implementation. They are there because a vendor
header is allowed only where a part is named — `<NimBLEDevice.h>` in `libraries/Runtime/src/ble/`'s
`BleLink.h`, where the radio is the part — and because the vacuum system reads the same whether the
part behind it is a sensor or nothing at all. An interface with one implementation earns its place
exactly when it is a foreign boundary like that; anywhere else it is a second implementation that
does not exist yet.

**Reads go through the owner.** New state is a field on the module that owns it, exposed by an
accessor; the composition builds the parts and the objects it builds expose what they own, and the
serial domains read the owner when they format a reply. The sketch's `PanelRoot` is that owner for
the device: it holds the machine, the panel's parts and the interpreter, and answers each by
accessor, so one object still serves both halves of the graph.

**One exception, and it is the panel's view of the device.** `core/SystemState.h` is one object the
composition owns: a reference is injected into every task that owns a fact, and each writes its own
slice where the fact changes — the control tick the run and the chamber, the parts tick the pack,
the interface loop the two links, the device's document the unit and the stored target. A reader
copies the object and reads the copy, so one draw shows one instant, and accepts that the copy is up
to the writer's period old: 50 ms for the machine, 10 ms for the pack, 5 ms for the links. A slice
whose writer stops running stops moving. That staleness is the price of not having a fold with five
parameters and no owner, and it is paid knowingly — which means the state is never read as though it
were the instant's.

**No speculative generality.** One caller means no interface. One implementation means no template,
no compile-time flag, no `mode` field for a behaviour nothing uses. Add the second case when the
second case arrives.

**Delete, don't deprecate.** Unused code goes to git — not behind `#if 0`, not in a `legacy/`
folder, not behind a flag defaulted off. The Berry runtime and the host client were both deleted
outright and left to the history; do the same.

**Answer a requirement with the smallest mechanism.** Take what has to be true from the item that
states it, then answer it with the smallest mechanism the tasks already running allow. A second task,
a lock, another queue or a watchdog is a question to ask before it is a thing to build.

**Same behaviour, less surface** is the trade to look for. `PressureSensor` is the current shape
of it: one `pressureKpa()` reading, with the ambient reference and the values derived from it left to the
driver that can actually know them.

## The Berry boundary

The panel's pages are written against models the interpreter compiles. Each of those models — the
scene, the stack, the registry, the settings kinds, the components — is a C++ class in the library,
and the Berry side is a wrapper that presents it to a page. The wrapper is a `be_pushclass` table in
a file in `libraries/Gardener/src/routines/bindings/`, beside the modules, whose callbacks unpack
their arguments, call the class and push its answer; the C++ object belongs to the instance the way
`Config`'s does, a comobj in the instance's own member, freed with it.

The wrapper is thin, and the boundary runs one way:

- Nothing in the C++ class knows about `bvm`. Box arithmetic, text formatting, the settings rules
  and the stack's state are the class's; unpacking arguments and pushing an answer is the
  wrapper's. A callback that computes a layout or formats a value has put the model in the wrong
  place.
- A page still extends the wrapped class and overrides what differs. Where the class has to call
  back into the page — a scene's `draw_page`, `handle` and `tick`, a layout drawing its children —
  the wrapper is what turns that call into a call on the page's own method by name. It is the only
  place that knows both sides of the boundary, which is why it stays small.
- A value the page hands over stays Berry's: the instance's own member, or a value the wrapper
  keeps where the collector can see it. Copying it into a C type and back loses what the page meant
  by it.

A model is never written twice. Keeping the C++ class and the behaviour it replaces in a `.be` file
is the one thing this rule forbids: the two drift apart, and the image pays the compile cost of
both.

This is guidance for new code, and for code you are already changing. Existing structure is not a
rewrite-on-sight target, and an unrequested refactor is its own decision with its own measurement.
