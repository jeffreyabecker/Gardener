# Gestures

Six keys, no labels on the glass, and one hand. This covers three things: the gestures the
classifier can produce, the one gesture the firmware reserves for itself, and the standards every
page follows on top of them.

The keys are the panel's, so everything here is a panel contract. **A build without a panel has no
keys and therefore no gestures**: its stop is ownership alone — the chamber vents when the
controller leaves the stack, and an unowned machine is always vented. Nothing below applies to it.

This file is the contract for **any** page. What a particular page does with a press — its own
shortcuts, what OK starts on it, which presses it ignores — belongs in that page's own `.be` file,
and the shipped bindings are listed in [scenes.md](./ui/scenes.md). The routine API is in
[routine.md](./ui/routine.md) and [bindings.md](./ui/bindings.md); this file is the UX rule above
them.

## The keys

| Key | Token | Role at the page level |
| --- | --- | --- |
| Up | `up` | move the selection up, or raise the value being edited |
| Down | `down` | move the selection down, or lower the value being edited |
| Forward | `forward` | enter: open the selection, advance a stage |
| Back | `back` | leave one level up |
| Okay | `ok` | confirm: the primary action — start, save, select |
| Cancel | `cancel` | escape: stop the run, discard an edit, leave the routine |

The tokens are the words `buttonName()` answers, the words a page compares in Berry, and the words
in the press log (`btn: forward:single`), so one vocabulary serves the wire log, the Berry gesture
and the C++ enum.

## How a gesture is recognised

The classifier answers each beat with one whole gesture string, or the empty string. A gesture is
one or more keys, each with the form of its press, joined by `+` in panel order — up, down,
forward, back, okay, cancel. `ok:single`, `cancel:double` and `back:5+ok:5` are all gestures.

| Term | Meaning |
| --- | --- |
| `single` | one press, reported on release, after the 350 ms double window closes |
| `double` | two singles with the second starting within 350 ms of the first release; one event, on the second release |
| `key:2`, `key:3`, … | a key held to `ExtendedHoldMs` (2 s) or more, reported as whole seconds while it is still down |

A single is reported only after the double window closes, so a tap is a little late rather than
acted on twice. A hold's seconds are reported once per second while the key is down; the release
after a mark is the mark's end, not a press of its own. A key held at boot is not a press. The
numbers are `DebounceMs` (20 ms) and `DoublePressMs` (350 ms) in `hal/impl/Buttons.cpp`, and
`ExtendedHoldMs` (2 s) in `hal/Button.h`.

Keys down together are one gesture, not two: each says its own part and the sum is reported on the
last release, in panel order. No shipped page binds a sum; the classifier reports it because the
parts are one event, and a page that needs two keys at once names the whole sum.

## Reserved gestures

These belong to the device. A page must not give them another meaning, and cannot suppress them.

| Gesture | What happens | Where |
| --- | --- | --- |
| **Hold Back+OK for 5 s** | safe reset: stop, vent up to 2 s, restart | every page, every link state |


**Back+OK is the reboot.** It is the same safe reset the wire serves — `device reboot` on serial and
`command 0x04` on Bluetooth — issued from the panel's side, so it works whatever a controller is
doing. It is the only gesture the device keeps for itself; every other press is a page's to answer.

A page that runs something usually wants a stop gesture of its own, and that gesture owns the
**whole** stop rather than the pump alone: `vacuum.stop()` and whatever returns the page to rest
belong in it. Nothing else is reserved — a scene that runs must stop itself on Cancel; no device
hold will do it.

## Standard routine gestures

A routine is a special type of page: it runs the machine, and every routine answers the same four
gestures.

| Gesture | Meaning |
| --- | --- |
| `ok:single` | the scene's primary action |
| `cancel:single` | stop and vent, pause the run |
| `up:double` | launch settings |
| `cancel:double` | quit the routine |

While an external link drives the machine there is no routine page: presses are logged
(`btn: <gesture>`, for example `btn: ok:single`) and only Back+OK still acts, because it is the
reboot and works whatever a controller is doing. A link that is connected but has not claimed
anything leaves the disconnected page — and its routine — in place.

## Rules for a scene

1. Leave the reserved gesture alone; it is the device's.
2. `up:double` opens a routine page's settings; `forward:single` opens a routine's settings from
   the browser.
3. Back leaves the current view without writing; Cancel is the scene's stop. A stop of the scene's
   own is the whole stop — the pump and the scene's state — and no device hold does it for a scene
   that lacks one.
4. OK is the primary action; Up and Down move or change.
5. A view that needs one draws its hint on the panel's last line — the value scenes do. There is no
   permanent legend on the glass.
