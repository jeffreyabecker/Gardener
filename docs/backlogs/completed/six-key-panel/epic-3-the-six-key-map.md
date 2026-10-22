# Epic 3 — The six-key map

The scene framework matches whole gesture strings and the six scenes bind the six-key map. The
contract updates in epic 4 follow.

## Task 1 — Rewrite the dispatch to whole gestures

`Scene.on` takes one whole gesture string and pushes `[gesture, fn]`; `handle_page(ev)` compares
the two in full, so a scene matches a gesture whole or not at all. `press_matches`, its `string`
import and the `(btn, presses, fn)` signature all went — no `.be` file splits a gesture on `+` or
`:`, and none registers a key without its press.

The six scenes then bind the map:

- the readout starts on `ok:single`, vents on `cancel:single`, and opens the browser or the system
  scene on a double up or down;
- the browser and the system scene move on up/down, open on `ok:single`, and leave on `back:single`
  or `cancel:single`; the browser opens a routine's settings on `forward:single`;
- the settings list and the value scenes keep their up/down/ok/back shape, with a value scene
  discarding on `cancel:single` where it once discarded on back;
- the duration scene advances minutes→seconds on `forward:single` and steps back on `back:single`
  where both were OK before; and
- the three routine pages open settings on `up:double`, begin on `ok:single`, pause on
  `cancel:single` and leave on `cancel:double`. `Interval` gains the paused phase this asks for:
  pause vents and holds the set, and OK picks the run up at the set it paused on.

The drive's settings walk advances a duration with `forward:single`, its run walk presses each
page's whole gestures instead of a two-key sweep, and the pause suites press `cancel:single`; a new
ramped_static case asserts a `cancel:double` leaves the page and a new interval case asserts the
pause. `scenes.md` rule 5 and `gestures.md`'s device section now describe the settled map.

Verified: `python tools/berry/check.py --no-cache --tests` — 22 passed, 0 failed, all three routines
walked — and `python scripts/arduino.py --task build` — 874,657 bytes of program storage, 30,044
bytes of globals.

**Not yet done:** the bench check. The plan's completion for this task is that on the wired rig each
of the six keys does what the map says — the browser opens on a double up, the system scene on a
double down, the settings on forward in the browser, the routines pause on a single cancel and
leave on a double, and the readout vents on a single cancel.
