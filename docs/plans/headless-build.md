# The machine without the panel

A build must exist that runs the machine alone: the vacuum system and its parts, the ownership
record and the two links, with no display, no keys, no buzzer, no filesystem and no interpreter in
the image. A device with that build is not a crippled one — a controller drives it exactly as it
drives the full device, and the wire contract is one machine, so a host that can drive one can drive
the other.

What this settles is whether the panel is one end of the machine or a client of it, and the tree
already answers most of it. What a controller commands is one graph — the vacuum system, the
ownership record and the links — and the panel is the rest: the display, the
keys, the buzzer, the filesystem, the settings, the interpreter and the disconnected pages. Every command from a routine, a serial
controller or a BLE central already lands in the one machine command queue, and the control task is
the only thing that touches the parts. The work is to finish that split and ship the half a
controller alone can use.

## The seam today

The code is closer to two assemblies than its one `AppContext` graph suggests. What follows is the
map the plan rests on.

**Three tasks, one machine.** Boot spawns a control task (core 1, highest priority), an interface
task (core 0) and a panel task (core 0, lowest) ([Tasks.cpp](../../libraries/VacuumCore/src/app/Tasks.cpp)).
The ordering is a safety choice, not a convenience: the control task's tick must never be delayed by
a link or by a page, and the panel runs lowest because a routine's `poll` may take its time.

**One command queue.** The machine's only write path is `machineCommandQueue`, drained by the control
task's `applyMachineCommand`, with the ops `Start`, `Stop`, `ResetFault`, `SetTarget`, `ClearTarget`,
`Reboot`. The writers are the serial `vacuum`, `control` and `device` domains, the BLE link, and the
VM's `vacuum` binding — a routine and a controller command the same machine the same way. The parts
themselves (`sensor`, `pump`, `vent`, `vacuum`) are touched only inside the control task. Reads are
direct from the reader's own task: the wire reads `vacuum` and `battery`; the panel reads through
the overlay and the VM bindings.

**One store bus.** The panel's `StoreClient` is the VM's way to flash, and a `StoreServer` applies
it: routine `Get`/`Set`, and the settings writes `SetUnit`, `SetTarget`, `SetBluetooth`, `SetJingle`.
The bus and the filesystem behind it serve the panel alone — the routines' values and the user's
preferences — which is the first sign that neither belongs to the machine.

**Shared scalars, one writer each.** `stopSeq` and `goHomeSeq` (the control task), `serialConnected`
and `bleConnected` (the interface task) — the smallest possible shared state, read dirty by the
panel.

**The graph itself splits cleanly.** Of the context's fourteen parts, the machine is `sensor`,
`pump`, `vent`, `battery`, `vacuum` and `ownership`; the panel is `buttons`, `buzzer`,
`overlay`/`display`, `routines`, `storage`, `settings` and the store bus's two ends. The battery
sits on both sides — the overlay draws it, the BLE table publishes `batt` and `device get` reports it
— but it is a machine reading that the panel displays, not a panel part, and a wired-only device
simply does not fit one.

**Storage and settings are panel, not machine.** Everything they hold serves the panel: `unit` is
what the glass prints, `target` is the depth the default routine pulls to, `jingle` is what the
jingles module plays, the routine `.ini` files are the pages' own values, and the data image — the
mark, the modules, the pages — is panel content. The two knobs that touch the machine, `bluetooth`
and `log`, are read at boot and persisted in `settings.ini`; in a build without the panel that file
does not exist, so both are build constants rather than settings. A machine-alone device has no
filesystem at all.

## Where the halves are still welded

Four couplings survive the map above, and each one is a requirement in the next section.

**The safety gestures are buttons in the control task.** `buttonsTick` classifies presses and folds
Up+Down into a chord, `watchBackHold` stops and vents after Back is held 3 s, and `watchResetHold`
reboots after Back+OK is held 5 s. They are read in the control tick at the highest priority so a
busy page cannot delay them. The keys are not a convenience; they are how the device stops, and they
are the one panel part the machine's beat reaches for today.

**The boot aborts for the panel.** `App::begin()` brings up the panel's I²C bus, calls
`display.begin()`, and `abort()`s when it fails ([App.cpp](../../libraries/VacuumCore/src/app/App.cpp)).
The panel bus bring-up and the fatal rule are panel properties currently owned by the machine's boot.

**The wire names the panel.** `device get` reports `display-size`, `unit` and `log`, and the `fs`
and `buzzer` domains serve the filesystem and the piezo — five things a machine without a panel
cannot answer. The rest of the wire surface — `state`, `kpa`, `target`, `duty`, `fault`, `batt`,
`control` — is the machine and nothing else.

**Ownership is cleared only by the panel.** The record is in RAM; a controller that drops while
owning leaves it held but stale, and only `reset()` clears it ([Ownership.h](../../libraries/VacuumCore/src/app/Ownership.h)).
The reset's only reachable gesture today is Back+OK. A machine without keys has no way to clear a
record a vanished controller left, short of power — which is why the headless device clears on loss
instead.

## What has to be true

**The machine boots and runs with none of the panel's parts.** A device with no display does not
abort for the want of one: the panel bus bring-up and the "panel must answer or abort" rule are part
of the panel build's boot, not the machine's. The machine comes up on its own — the parts,
the ownership record, the links — and serves the wire.

**The panel is a client of the machine.** It observes the machine's state and commands it through
the surface a controller uses, nothing more: the command queue for every verb, direct reads for
state. Its filesystem, its settings and the store bus stay its own; the panel may not reach a part. The consequence
is the point of the plan: a build that does not reference the panel compiles the machine alone, and
the panel stack, the fonts, the interpreter and the display driver never reach the image. The saving
must be measured rather than assumed, in removal order — bindings, then VM, then panel, then keys
— before the build is committed.

**The machine's safety does not live in the keys.** The Back hold and the Back+OK reset are the
panel's expression of a machine property, not the property itself. The property is stated without a
key: a machine with no panel may hold pressure only while a controller owns it, and losing the owner
— a dropped link, a vanished controller — releases the chamber and returns the device to unowned, so
an unattended headless device is always inert and nothing outlives the controller that started it.
The full device keeps the Back hold as its stop; the headless device's stop is ownership.

**The wire surface is one machine, and it says which machine it is.** `device get` must not report
a panel dimension on a device without one; the report names the machine and carries a capability
that says which optional assemblies are fitted, which is how a host tells the two products apart
(see below). The panel's surface follows that capability: the `fs` and `buzzer` domains and the
`display-size`/`unit`/`log` fields of `device` exist only where a panel does, so a machine-alone
device carries no `fs`, no `buzzer`, no `unit`, no `settings.ini` and no data image.

**The builds are builds, not switches.** The full device and the machine-alone are separate sketches
with separate upload paths; the full device ships a data image and the machine-alone ships none,
because it has no filesystem. The battery is a third axis inside either of them — a wired-only unit
leaves the gauge out, whichever side of the panel split it sits on — so the variants are a small
matrix, not a pair. The build script pins one FQBN and one LittleFS offset today
([arduino.py](../../scripts/arduino.py)); the machine-alone build skips the data task entirely, which
resolves the "one file to flash" item for it and leaves it open only for the full device.

## The two libraries

The split is two libraries and a one-way dependency, not a decomposition into many. The machine
library keeps everything a controller commands; the panel library keeps everything that draws,
listens and runs scripts; the panel library depends on the machine library, and the machine library
includes nothing panel-shaped. The line is the physical one: the machine keeps the parts a
controller drives and the links that drive them; the panel keeps everything that presents and
persists — display, keys, filesystem, settings, scripts — and the wire domains those need ride on
the machine's dispatcher, composed the same way the capability record is.

**The machine library** — the current `VacuumCore`, kept under that name — holds:
- `core/`: the constants, units, parsing and math.
- the machine's `hal/` parts and their impls: pressure sensor, pump, vent valve, and the battery
  gauge, which a wired-only build leaves out.
- the machine itself: `VacuumSystem`, `Ownership`, the loggers, the command queue and the control
  and interface tasks, plus the small configuration the composition root hands it — whether the
  radio runs, whether diagnostics stream, whether a battery gauge is fitted.
- the links: `SerialLink` and its domains, `BleLink` — and the capability record both read.
- the machine's boot and its composition root.

**The panel library** — a new `GardenerPanel` — holds:
- the panel's `hal/` parts and their impls: buttons, display and buzzer.
- the panel itself: `Overlay`, the two screens, `Fonts`, `Asset`, the VM (`Routines` and the eight
  bindings), the jingles, `BuzzerPlayer`, the storage stack (`Ini`, `Settings`, `Storage`), the
  store bus, the press queue and the panel task.
- the safety reader — the press classifier and the two holds, which leave the control task.
- the panel's boot and its composition root, which builds the machine root first and composes the
  panel onto it.

**What stays outside both.** The vendored `berry` interpreter and the Adafruit drivers (`SH110X`,
`BusIO`, `GFX`) remain third-party libraries. The panel library is the only thing that links them,
which is how a machine-alone build comes up without them; the machine library links only the ESP32
core and NimBLE.

**How the tasks work.** The machine library owns the control and interface tasks and the command
queue. The panel library owns the panel task and starts it from its own boot, so the machine's task
spawn no longer takes the two screens. The control beat keeps its order — drain commands, tick the
parts, service the vacuum — and ends in one slot the build fills or leaves empty: a
**local-stop source**, polled on the control beat so the stop keeps its priority. The panel owns the
keys and writes two readers for them: the safety reader it installs into that slot, and the page
reader it drains on its own beat. The slot's contract is a verdict — a stop, a reset — applied by
the machine, so the machine never names a key and the stop is never delayed by a page. A headless
build installs no reader at all.

Everything else is already the shape it should be: the interface task polls both links, the panel
task drains the press queue, switches screens by ownership, updates the overlay, runs the VM, ticks
the buzzer and owns the store bus end to end, and the two libraries meet only at the command queue
and that one slot.

The split is not pushed further. No wire library, no storage library, no `hal` library: the only
boundary with a second implementation to serve is the one the build variant draws, and a third
library would be a boundary waiting for a caller that does not exist. The battery is optional the
same way the panel and buzzer are, but it stays in the machine library — it is a machine reading a
wired-only device happens not to need, not a second assembly.

## The capability surface

A host has to know what it can command, and today it half-knows: `batt_v` and `batt_pct` report
`-` with no gauge, `sensor` says `1` or `0`, and the BLE `batt` publishes `0xFF` — honest answers,
but scattered and runtime-shaped, not one place a host can ask. What a host cannot ask is whether the panel exists, because today it
always does. The headless build makes that a real question, and it is answered once, machine-side,
and served identically by both links.

**One source, two links.** Capabilities are composed at build time: the composition root knows which
assemblies it built and records them in the machine, and the serial link and the BLE link read the
same record, so a host gets the same answer whichever transport it uses. A capability is a fact
about the device, not about the link.

**What is reported.** The set stays to the questions a host actually decides, and the per-field
answers that already exist are left alone rather than folded in:
- whether the panel is fitted — the one new fact the headless build introduces, carrying the keys,
  the pages, the VM, the filesystem, the settings and the buzzer with it,
- whether a battery is fitted — a wired-only device has no gauge, and its `batt_v`/`batt_pct` and
  BLE `batt` follow that bit,
- and the safety posture — whether the device can be left to stop itself: the headless device cannot
  be pressurized while unowned and releases when its owner leaves, which is what a controller must
  know before it owns a run it might abandon.

The sensor keeps its present `1|0` answer rather than joining the set: whether a reading can be
trusted is a runtime fact, not a build fact, and the pressure sensor is never optional.

**What it replaces.** `device get` stops reporting `display-size` and reports the capability in its
stead, and its `unit`/`log` fields and the `fs` and `buzzer` domains appear only where a panel does;
on a panel-less device the field simply says so, and a host knows up front instead of learning it
from an `err unknown_domain`. The BLE table gains one read-only characteristic alongside the
per-field absents it already has. The rule both links enforce is the same: a command for a part that
is not fitted is refused, and a host can know that before it sends it.

## The trade being accepted

The device's own stop is a key today, and the firmware has no timeout by design: a controller that
drops while owning leaves the run going and the record held, and nothing counts the hold down
([vacuum control](../specs/vacuum-control.md)). A machine without keys cannot keep that rule, so the
headless device answers it with ownership itself: it may hold pressure only while a controller owns
it, and losing the owner — a dropped link, a vanished controller — releases the chamber and frees
the device for the next controller. That is the timeout the firmware refuses today, adopted for
exactly the one shape that needs it, and it resolves the stale record too: a loss clears what a drop
used to leave held, so there is no Back+OK to replace and no wire clear-command to invent.

The full device keeps its present semantics and its key; the headless device's stop is ownership.
The difference is real, and it is exactly what the capability's safety posture reports, so a
controller knows before it owns a run whether the device will stop itself if the controller does not.

## Ruled out

A compile flag that hides the panel from the one sketch is not the answer: it puts two products in
one file and a partial context in front of every wire caller, and it removes nothing the second
build does not. A runtime setting is not the answer either: nothing that runs after boot can take
code out of an image. The requirement is the seam, not the switch; which file and which type carry
it is the implementer's call.
