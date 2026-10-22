# Epic 4 — The control stack

The one-owner record with its stale flag becomes a stack of links. `None` sits
at the bottom and vents; a panel build pushes the panel above it at boot, so
the panel is the default controller; serial and Bluetooth push themselves when
they claim and are removed when they release or drop. Whoever is on top
drives, and any change of the top vents the chamber.

## What changed

- **The stack.** `Ownership` is a stack of links above an implicit `None`
  floor. `acquire` pushes and always succeeds, `release` and `drop` remove
  the link wherever it sits, and `owner()` is the top. The `busy` reply and
  the stale flag are gone: a claim is never refused, and a vanished link is
  simply removed. `Link` gains `Panel`.
- **Vent on any change.** The control beat compares the top against the last
  it saw and calls `vacuum().stop()` when it differs, so a claim, a release
  or a drop all vent and a new controller starts from an open chamber.
- **The panel is the default controller.** The composition root pushes the
  panel at boot, beneath every external link. The ownership-stop capability
  is set: every build vents when its owner leaves.
- **The buttons leave the control task.** The press classifier and the two
  holds move to the panel beat, which is now the button driver's one
  consumer. Back is the panel's own stop, effective while the panel drives;
  Back+OK is the reboot. The `StopSource` slot, `ButtonStopSource` and the
  button-event queue are deleted.
- **Who owns, named.** Serial `control get` and the BLE `control`
  characteristic answer `serial`, `bluetooth`, `panel` or `none` — the top
  of the stack — instead of "owned by this link, or not". The BLE `control`
  value is the `Link` enum: `0` none, `1` serial, `2` bluetooth, `3` panel.

## Verification

The build passes throughout. The stack's push, release and drop semantics —
including releasing a link from beneath the top, a vanished owner falling
back to the panel, and an absent release being a no-op — are exercised by a
throwaway host test against `Ownership.h`, which passes. The on-hardware pass
is the parking lot's verification entry.

## What the headless build will do with it

A machine-alone composition root pushes no panel, so its stack is `None`
alone: an unowned machine vents, and a dropped owner vents. The safety rule
Epic 4 built is the whole headless story — that is Epic 6.
