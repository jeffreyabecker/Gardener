# Epic 4 — The contract

The specs now describe the six-key panel as it is: the gesture vocabulary, the string the page
framework matches, the host harness that drives it, and the parking-lot notes the work leaves
behind. This closes the effort.

## Task 1 — gestures.md describes six keys

`gestures.md` names the six keys and their tokens, the whole gesture string the classifier answers
(one or more keys joined by `+` in panel order), the `single`/`double`/seconds forms, the two-key
sum and the `btn: <gesture>` press log. The reserved gestures shrink to Back+OK for 5 s, and the
standard gestures a routine answers are the four the map gives it.

## Task 2 — bindings.md describes the string

`buttons.take()` answers the classifier's gesture string or `nil`; the `event` record is gone. The
`vacuum.stopped()` notice is an external link claim or release, not a Back hold, and the queue, the
drain and the reserved reset match the binding.

## Task 3 — routine.md describes the scene's gestures

A new section says `handle(ev)` receives one whole gesture string and `on(gesture, fn)` is the
declarative form; both match whole or not at all. The shared-code and editor sections follow: the
settings list opens on `up:double`, a value scene keeps on `ok:single` and discards on
`cancel:single`, and a duration advances on `forward:single`.

## Task 4 — berry-harness.md describes the host's press

The record list drops `event`, `harness.press` pushes one whole gesture string, and the drive's
walk is the six-key one: every key's single, the settings open and close, and a double Cancel that
leaves the page.

## Task 5 — Close the effort

The parking lot keeps the nudge line with its new free keys, and the Gesture Classifier note goes.
`docs/README.md` moves the six-key panel into the completed list, the epic summaries are written,
and the backlog and its plan are deleted together.

**Not yet done:** the bench check. The plan's completion for the whole effort is that each of the
six keys does what the map says on the wired rig, which has not happened here.
