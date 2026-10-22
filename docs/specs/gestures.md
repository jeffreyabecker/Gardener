# Gestures

Four keys, no labels on the glass, and one hand. This covers three things: the presses the
classifier can produce, the gestures the firmware reserves for itself, and the standards every page
follows on top of them.

The keys are the panel's, so everything here is a panel contract. **A build without a panel has no
keys and therefore no gestures**: its stop is ownership alone — the chamber vents when the
controller leaves the stack, and an unowned machine is always vented. Nothing below applies to it.

This file is the contract for **any** page. What a particular page does with a press — its own
shortcuts, what OK starts on it, which presses it ignores — belongs in that page's own `.be` file,
and the shipped bindings are listed in [scenes.md](./ui/scenes.md). The routine API is in
[routine.md](./ui/routine.md) and [bindings.md](./ui/bindings.md); this file is the UX rule above
them.

## How a press is recognised

| Term | Meaning |
| --- | --- |
| `single` | one press, reported on release, after the 350 ms double window closes |
| `double` | two singles with the second starting within 350 ms of the first release; one event, on the second release |
| hold | a key held to `ExtendedHoldMs` (2 s) or more, reported as whole seconds while it is still down |

A single is reported only after the double window closes, so a tap is a little late rather than
acted on twice. A key held at boot is not a press. The numbers are `DoublePressMs` in
`hal/impl/Buttons.cpp` and `ExtendedHoldMs` in `hal/Button.h`.

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
belong in it. Nothing else is reserved — Back is the scene's to name, and a scene that can run must
stop itself.

## The standard page gestures

| Gesture | Meaning |
| --- | --- |
| **Up+Down** | the scene's edit or settings. A routine page opens the shared settings view. |
| **OK** | the scene's primary action. In the settings list it opens the value; in a value scene it saves. |
| **Back** | the scene's own: usually leave the current view without writing, or stop what it is running. The stack sets no global rule — a scene that needs another meaning names its own Back. |
| **Up / Down** | move the selection, or change the value being edited. |

A routine page takes up+down for the settings view even if it also has an inline editor: one
gesture, one meaning. The device's own scenes have no settings list and use up+down for the editing
they do have.

The settings view follows the same rules wherever it is hosted: OK opens a value, Up/Down changes
it, OK saves, Back closes the list or cancels the value scene. A `duration` adds a stage — OK moves
from minutes to seconds before it saves. A `pressure` edits in the unit on the glass and stores kPa,
so changing the display unit never rewrites a value.

Up and Down are the page's own. A double on either is one event and moves once, so a page that does
not distinguish it simply treats it as that key; a page may give the double a meaning of its own,
and no other page is required to take it.

While an external link drives the machine there is no routine page: presses are logged (`btn: ok
short`) and only Back+OK still acts, because it is the reboot and works whatever a controller is
doing. A link that is connected but has not claimed anything leaves the disconnected page — and
its routine — in place.

## Rules for a scene

1. Leave the reserved gesture alone; it is the device's.
2. Up+Down is the scene's edit gesture. A routine page opens the shared settings view.
3. Back is the scene's to name: usually it leaves without writing, or stops what the scene is
   running. A stop of the scene's own is the whole stop — the pump and the scene's state — and no
   device hold does it for a scene that lacks one.
4. OK is the primary action; Up and Down move or change.
5. A view that needs one draws its hint on the panel's last line — the value scenes do. There is no
   permanent legend on the glass.
