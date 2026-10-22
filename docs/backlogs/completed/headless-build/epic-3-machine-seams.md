# Epic 3 — The machine's seams

The machine stops reading settings, keys and the buzzer directly, and its boot
stops owning the panel's bring-up. Each seam landed while everything still
lives in one library, so the full device behaves identically after the epic.

## What changed

- **Configuration.** `MachineConfig` joins the `AppContext` graph — radio,
  diagnostics, battery gauge. `BleLink::begin()` reads the radio knob from it,
  the boot reads the diagnostics knob for the logger, and the full build's
  root fills both from `settings.ini` before either is read. The machine no
  longer names `Settings`.
- **Local-stop slot.** `StopSource` reports a verdict — stop or reset — and
  the machine applies it on the control beat, re-asserting the stop each tick
  and latching the `stopSeq` bump to one per hold. `ButtonStopSource` carries
  the two holds (Back 3 s, Back+OK 5 s) behind the slot. A build with no
  local stop installs nothing.
- **Panel bring-up.** `panelBoot()` in the composition root brings the panel
  bus up and aborts when the panel does not answer. The machine's boot calls
  it only when the capability says a panel is fitted, so a machine-alone build
  boots with no display and no abort.
- **Buzzer tick.** The control beat ticks whatever `TickService` the
  composition installs; the full build installs `BuzzerTick`. The machine's
  beat names no part.

## One correction to the plan

Task 3.7 said to move the press classifier to the panel beat. It stays on the
control beat instead: the button driver is single-consumer (its `settle()`
mutates one state machine), so the safety reader and a panel-side classifier
would race on the keys. The panel's page reader still drains the classified
presses on its own beat, as it always has; only the classification stays where
it cannot race the stop.

## Verification

Build passes throughout. Behavior is preserved by construction — the config
is filled from `settings.ini`, the hold semantics keep the same latch and
re-assert, and the buzzer stays on the control beat. The on-hardware pass is
the parking lot's verification entry.
