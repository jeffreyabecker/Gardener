# Gestures

Four keys, no labels on the glass, and one hand. This is what the firmware reserves, what a page
can rely on another page doing, and the matrix of every press the classifier can produce.

The routine API is in [routines.md](./routines.md); this file is the UX rule above it.

## How a press is recognised

| Term | Meaning |
| --- | --- |
| `short` | released before 600 ms |
| `long` | held 600 ms or more, reported on release |
| `double` | two short presses with the second starting within 350 ms of the first release; one event, on the second release |
| hold | a key watched while it is down; the firmware acts at its own mark |

A single short is reported only after the 350 ms window closes, so a tap is a little late rather
than acted on twice. A long second press is a `long`, never half a double. A key held at boot is not
a press. The numbers are `BUTTON_LONG_MS` and `BUTTON_DOUBLE_MS` in `core/Config.h`.

## Reserved gestures

These belong to the device. A page must not give them another meaning, and cannot suppress them.

| Gesture | What happens | Where |
| --- | --- | --- |
| **Hold Back for 3 s** | stop the pump and vent | every page, with or without a controller |
| **Hold OK for 3 s** | stop the pump, vent, and show `default` | the disconnected pages |
| **Hold Back+OK for 5 s** | stop, vent up to 2 s, restart, and clear ownership | every page, every link state |

The Back hold fires at the 3 s mark while the key is still down; the active routine is notified then
as `vacuum.stopped()`, and the release that follows arrives at the page as a `long` Back, which a
page that does not use it can ignore. The OK hold also fires at the mark and switches pages, so the
page being left does not have to handle the event.

**Back+OK is the local reset.** It is the only way to clear an ownership record left held by a
controller that dropped while owning, and it runs whatever a controller is doing. The Back hold and
the disconnected page's OK hold may fire first on the way to the 5 s mark; the machine is already on
the release path by then, so that is harmless.

**Hold Back is the stop gesture.** A routine does not implement its own stop key and does not use
short Back for it: short Back is the page's Back. The hold is why a routine never has to know about
the pump, and it is why `poll` must return — a poll that never returns blocks the loop that watches
the hold. A page that needs to reset its own state when the hold fires reads `vacuum.stopped()`,
which is true once and then false again.

**Hold OK is the exit gesture.** It stops, vents and returns to `default`, so any page is one hold
away from home. The browser's short Back goes home too, but that is the browser's own shortcut.

## The standard page gestures

| Gesture | Meaning |
| --- | --- |
| **Double OK** | the page's edit or settings. A page with `describe()` opens the shared settings view; the browser's list opens the highlighted routine's settings. |
| **OK** | the page's primary action. In the settings list it opens the value; in a value editor it saves. |
| **Back** | leave the current view without writing. The settings list closes; a value editor cancels. On `default` it vents; on the browser it goes home. |
| **Up / Down** | move the selection, or change the value being edited. |
| **Double Up** | `default` opens the browser. No other page is required to take it. |
| **Double Down** | `default` opens `system`, the device's own settings. No other page is required to take it. |

A page that describes settings takes double OK for the settings view even if it also has an inline
editor: one gesture, one meaning. `default` has no `describe()` today, so double OK there opens the
target editor, while double Down opens `system` — the device's own settings rather than a routine's.
`system` edits its two values in place with the same OK/Up/Down/Back grammar as the shared editor.

The settings view follows the same rules wherever it is hosted: OK opens a value, Up/Down changes
it, OK saves, Back closes the list or cancels the editor. A `duration` adds a stage — OK moves from
minutes to seconds before it saves. A `pressure` edits in the unit on the glass and stores kPa, so
changing the display unit never rewrites a value.

## The matrix

`—` means the page does nothing with the press. `page` means the event is delivered to the active
routine. The holds have their own rows because they are not presses.

| Gesture | Firmware | `default` | `static` | Browser list | Settings list | Settings editor |
| --- | --- | --- | --- | --- | --- | --- |
| Up short | page | — | — | move | move | step + |
| Up long | page | — | — | move | move | step + |
| Up double | page | open browser | — | move | move | step + |
| Down short | page | — / step | — | move | move | step − |
| Down long | page | — / step | — | move | move | step − |
| Down double | page | open system settings / step | — | move | move | step − |
| OK short | page | start / save target | start / — | switch page | edit value | save |
| OK long | page | — | start / — | switch page | edit value | save |
| OK double | page | open target editor | open settings | open settings | edit value | save |
| Back short | page | vent / cancel | — | go home | close | cancel |
| Back long | page | — / cancel | — | go home | close | cancel |
| Back double | page | vent / cancel | — | go home | close | cancel |
| **Hold OK 3 s** | stop, vent, `default` | same | same | same | same | same |
| **Hold Back 3 s** | stop, vent | same | same | same | same | same |

On `default`, the text after the slash is what the target editor does while it is open. A `double`
on Up or Down is one event and moves once; a page that does not distinguish it treats it as that key.

On `static`, OK starts a cycle only from `ready`. Once the pull is under way, Back is not a stop —
the Back hold is, and the page notices the machine's release and returns to `ready`.

`system` follows the settings grammar with two rows: Up and Down move between `UNIT` and `LOG`, OK
opens the selected value, Up/Down cycles the unit or toggles logging, OK saves, and Back cancels or
returns to `default`.

The page columns describe the disconnected device. While a controller owns the device there is no
routine page: presses are logged (`btn: ok short`) and only the Back hold and the Back+OK reset still
act. A link that is connected but has not acquired control leaves the disconnected page — and its
routine — in place.

## Rules for a page

1. Leave the reserved holds alone; they are the device's.
2. Double OK is the page's edit gesture. With `describe()`, it opens `Settings`.
3. Back leaves without writing. Only `default` gives Back a machine action.
4. OK is the primary action; Up and Down move or change.
5. Draw the hint for the view that is showing — the settings editor does on its last line. There is
   no permanent legend on the glass.
