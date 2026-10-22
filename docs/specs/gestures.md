# Gestures

Four keys, no labels on the glass, and one hand. This covers three things: the presses the
classifier can produce, the gestures the firmware reserves for itself, and the standards every page
follows on top of them.

The keys are the panel's, so everything here is a panel contract. **A build without a panel has no
keys and therefore no gestures**: its stop is ownership alone — the chamber vents when the
controller leaves the stack, and an unowned machine is always vented. Nothing below applies to it.

This file is the contract for **any** page. What a particular page does with a press — its own
shortcuts, what OK starts on it, which presses it ignores — belongs in that page's own `.be` file.
The routine API is in [routine.md](./ui/routine.md) and [bindings.md](./ui/bindings.md); this file
is the UX rule above them.

## How a press is recognised

| Term | Meaning |
| --- | --- |
| `short` | released before 600 ms |
| `long` | held 600 ms or more, reported on release |
| `double` | two short presses with the second starting within 350 ms of the first release; one event, on the second release |
| `chord` | Up and Down pressed together; one event on the first release, reported on the up key |
| hold | a key watched while it is down; the firmware acts at its own mark |

A single short is reported only after the 350 ms window closes, so a tap is a little late rather
than acted on twice. A long second press is a `long`, never half a double. A key held at boot is not
a press. The numbers are `LongPressMs` and `DoublePressMs` in `hal/impl/Buttons.cpp`.

## Reserved gestures

These belong to the device. A page must not give them another meaning, and cannot suppress them.

| Gesture | What happens | Where |
| --- | --- | --- |
| **Hold Back for 3 s** | the panel releases control, which vents the chamber; the valve stays open as long as Back is held | every page, while the panel drives |
| **OK while Back is held** | give the panel back to `default` | the disconnected pages |
| **Hold Back+OK for 5 s** | safe reset: stop, vent up to 2 s, restart | every page, every link state |

The Back hold fires at the 3 s mark while the key is still down, and the vent it starts stays open
for as long as the key is held. The release that follows arrives at the page showing by then as a
`long` Back, which a page that does not use it can ignore; the page stays where it was, the chamber
vented and the run it was on abandoned. A short, long or double OK pressed while Back is still held
is the way out on the disconnected pages: `default` takes the panel.

**Back releases the panel's control.** It is the panel's own way out of the stack, not a separate
stop: the machine vents because the controller left, exactly as it does when a serial or Bluetooth
link lets go. While an external link is on top the panel is not driving, so the Back hold does
nothing and only Back+OK acts.

**Back+OK is the reboot.** It is the same safe reset the wire serves — `device reboot` on serial and
`command 0x04` on Bluetooth — issued from the panel's side, so it works whatever a controller is
doing. The Back hold may fire first on the way to the 5 s mark; the machine is already on the
release path by then, so that is harmless.

**Hold Back is the panel's release.** It is the device's, it acts on every page while the panel
drives, and it is why a routine never has to know about the pump. It is also why `poll` must
return — a poll that never returns blocks the loop that watches the hold.

**OK during the hold is the way out.** On the disconnected page a short, long or double OK while
Back is held leaves whatever routine is showing and puts `default` back on the panel, so any page is
one hold and one tap from home and no page needs an exit of its own. The page being left does not
see that switch as `vacuum.stopped()`: the hold fired first and the page already took that notice,
so its own `abandon()` is what put its state right.

A page may still have a stop gesture of its own, and a page that runs something usually wants one:
the hold stops the run and OK during the hold is the way out, so Back is how a page stops a run
without leaving it. That gesture is the page's to choose and it owns the **whole** stop rather than
the pump alone: `vacuum.stop()` and whatever returns the page to rest belong in it. It adds to the
hold and never replaces it — a page cannot suppress Back for 3 s, and the hold still stops a run the
page's own gesture started.

## The standard page gestures

| Gesture | Meaning |
| --- | --- |
| **Up+Down** | the page's edit or settings. A page with `describe()` opens the shared settings view. |
| **OK** | the page's primary action. In the settings list it opens the value; in a value editor it saves. |
| **Back** | leave the current view without writing, or stop what the page is running. The settings list closes; a value editor cancels. |
| **Up / Down** | move the selection, or change the value being edited. |

A page that describes settings takes up+down for the settings view even if it also has an inline
editor: one gesture, one meaning. A page without settings may use up+down for whatever editing it
does have.

The settings view follows the same rules wherever it is hosted: OK opens a value, Up/Down changes
it, OK saves, Back closes the list or cancels the editor. A `duration` adds a stage — OK moves from
minutes to seconds before it saves. A `pressure` edits in the unit on the glass and stores kPa, so
changing the display unit never rewrites a value.

Up and Down are the page's own. A double on either is one event and moves once, so a page that does
not distinguish it simply treats it as that key; a page may give the double a meaning of its own,
and no other page is required to take it.

While an external link drives the machine there is no routine page: presses are logged (`btn: ok
short`) and only Back+OK still acts, since Back is the panel's own release and the panel is not
driving. A link that is connected but has not claimed anything leaves the disconnected page — and
its routine — in place.

## Rules for a page

1. Leave the reserved holds alone; they are the device's.
2. Up+Down is the page's edit gesture. With `describe()`, it opens `Settings`.
3. Back leaves without writing, or stops what the page is running. A stop of the page's own is the
   whole stop — the pump and the page's state — and it adds to the Back hold, which stops every
   page whether or not the page has a gesture of its own.
4. OK is the primary action; Up and Down move or change.
5. Draw the hint for the view that is showing — the settings editor does on its last line. There is
   no permanent legend on the glass.
