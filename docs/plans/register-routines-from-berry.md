# Register a routine from Berry

`/system/browser.be` names `List`, `ux` and `SettingsEditScene`, and none of the three exists, so the
file does not compile and no browser appears; `routines.push`, which its OK and Forward call, is
unanswered too. What is left is the path from a row to a launched, editable routine, and the gesture
that shows the list; the device's own settings list is another plan.

## What is left

| Call | Used by | What is missing |
| --- | --- | --- |
| `SettingsEditScene(cfgs, values, title)` | the browser's Forward and both pages' edit | the settings list as a page |
| `routines.push(key, nil, activation)` | the browser's OK and Forward | a record's page on the stack |
| `ux.push`, `ux.pop`, `ux.pop_to_root` | the browser's Forward and Back, the routines' `leave()` and `finish()` | the stack from Berry |
| `List(6)`, `set`, `move`, `selected_row`, `rows`, `selected`, `first()`, `visible` | the browser's rows | a window over the rows |
| Home's Up double press | home's gestures | the browser's way in |

## A key launches its page

The registry's `push` builds a page through the record's class and hands it to the stack, and nothing
calls it. `routines.push(key, callback, activation)` does, and answers whether a page is showing: an
instance of the class the record holds, the activation its one argument — the key and the clean set
`load_settings` answers. An unregistered key answers false with the stack as it was; so does a page
whose `init` raises, which logs one `[E]` line naming the routine and the message.

Checked: OK on a row shows that routine's page, built from the set the record holds; Back returns to
the rows; a page whose `init` raises logs one line and leaves the rows showing.

## The stack is moved from Berry

`ux.push(page, callback)` shows a page over the one showing, running the callback when it pops and
handing it what the page answered. `ux.pop()` closes the top page and `ux.pop_to_root()` every page
above the root, running no callback. Each answers whether a page is showing, a pop at the root answers
false and leaves it, and the eight-frame cap stays. The nils the image passes these calls are not part
of the shape, so its call sites come to it.

Checked: Forward shows the settings list over the browser, OK closes it and the browser is showing
again having read the set it handed back; a run that ends puts home back with no page above it.

## `SettingsEditScene` builds a settings list

`SettingsScene` draws and drives the list the three Forward paths want, with the gestures it already
has, and nothing in the image can build one. `SettingsEditScene(cfgs, values, title)` builds it from a
declaration list and a map: each setting starts from the map, or from the declaration's default where
the map holds nothing, a nil title draws no heading, and the set it settled on answers in the shape
`save_settings` takes. `ux.push` puts it on the stack.

Checked: Forward on a row edits that value, OK closes the list, and the routine's document holds the
new value after the launch that follows; Back leaves the document as it was.

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
another plan. A `.be` file resolves a name when it compiles, so whatever answers `SettingsEditScene`
loads before the browser names it.

## Deferred

- `keys`, `entry` and `icon`: no file calls any of the three.
- The device's own settings list: `DeviceSettings` and the rows `cfg/settings.ini` holds.

## How it is known to hold

1. `python scripts/arduino.py --task build` compiles the calls the module gains and the settings list a
   declaration list builds.
2. On the device, `python scripts/arduino.py --task upload` and the `[E]` lines: the browser loads and
   its three rows draw, a row launches, Forward opens the settings list and hands its set back, Up
   twice on home shows the rows, and no line names a call nothing answers.
3. `python tools/berry/check.py` is broken and is not run. `python tools/unit/check.py` builds only
   `libraries/Gfx` and `libraries/Gardener`, so it holds this plan only where a change reaches them.
