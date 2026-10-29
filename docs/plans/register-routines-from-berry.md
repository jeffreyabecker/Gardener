# Register a routine from Berry

`/system/browser.be` names `List`, and it does not exist, so the file does not compile and no browser
appears. What is left is the window its rows move through and the gesture that shows the list; the
device's own settings list is another plan.

## What is left

| Call | Used by | What is missing |
| --- | --- | --- |
| `List(6)`, `set`, `move`, `selected_row`, `rows`, `selected`, `first()`, `visible` | the browser's rows | a window over the rows |
| Home's Up double press | home's gestures | the browser's way in |

## The browser's rows move through a window

Six rows show at a time. `List(6)` — `set`, `move`, `selected_row`, and the reads `rows`, `selected`,
`first()`, `visible` — is the window: Up and Down move the selection by one and carry it along, the
selected row draws inverted, and the scrollbar marks the window's place in the list. Three
registrations fill less than the window, so it never moves and the scrollbar is full.

Checked: with three routines the rows show with their glyphs and titles and the scrollbar is full;
with more than six registered, Down past the sixth moves the window on by one.

## Home opens the browser

`/system/home.be` answers OK and Cancel only, so the browser is a scene nothing reaches. Home's Up
double press shows it, and Back or Cancel in the browser returns to home's own page. Nothing in the
browser starts a run: its `activate` stops the chamber, and only a launch begins one.

Checked: Up twice on home shows the rows; Back returns to home's readout with the device's target
still set; a run under way is stopped before the rows are drawn.

## Consequences

`/system/order`'s comment has home naming `browserScene` in its gestures and `DeviceSettings` in its
list, and home names neither: the browser is pushed from home, and the device's settings list is
another plan.

## Deferred

- `keys`, `entry` and `icon`: no file calls any of the three.
- The device's own settings list: `DeviceSettings` and the rows `cfg/settings.ini` holds.

## How it is known to hold

1. `python scripts/arduino.py --task build` compiles the window's calls and the gesture it is reached
   by.
2. On the device, `python scripts/arduino.py --task upload` and the `[E]` lines: the browser loads and
   its three rows draw, Up twice on home shows the rows, a row launches, Forward opens the settings
   list and hands its set back, and no line names a call nothing answers.
3. `python tools/berry/check.py` is broken and is not run. `python tools/unit/check.py` builds only
   `libraries/Gfx` and `libraries/Gardener`, so it holds this plan only where a change reaches them.
