# Six-key panel

The tasklist for [the six-key panel plan](../plans/six-key-panel.md). The plan is the requirement
and holds every decision — the six tokens, the Back/Cancel split, the two-key rule, the whole-string
registration, and the three press classes with an extended hold in seconds — so an item here states
the outcome and points at the plan rather than restating it. The plan and this file are deleted together when the last item lands, and each epic's
summary goes to `docs/backlogs/completed/six-key-panel/`.

This backlog targets `feature/berry-test-harness`, not `main`: `main` still has `page.be` and the
plan is written against the `Scene`/`scene.be` rename. Cut `backlog/six-key-panel/main` from that
branch, `backlog/six-key-panel/epic-<n>/main` per epic and `backlog/six-key-panel/epic-<n>/task-<m>`
per item — `<m>` is the item's number in its epic — and merge back with `--no-ff`.

The plan's parts 2 and 3 are worked in the other order. The vocabulary has to be a string before
the map can be written against it — `up:single+down:single` is not expressible while a press is an
`event` record with a `chord` token — so the reader (part 1) comes first, then the class and the
string (part 3), then the map (part 2), then the contract. The class comes before the string
because it is the thing that composes one: the first item writes the class, the second makes the
panel task consume it and carry the gesture to the page, and epic 3 rewrites how a page registers
it and what each scene binds.

Two checks cover the work: `python scripts/arduino.py --task build` for the firmware, and
`python tools/berry/check.py` for the image and the host harness — no flags to compile, load and
drive it, `--tests` to run the suites instead. An item that touches only `.be` files, the harness
or a spec needs only the second; one that touches `hal/`, `PanelTasks` or the bindings needs both.
What the keys feel like is the rig's, not the harness's: the six switches and the shift register are
checked on the bench in epic 1, and the whole map on the bench at the end of epic 3.

## Epic 1 — The shift-register reader

- [x] **1. Name the six keys.** `Button` gains `Forward` and `Cancel`, declared in the order the keys
  sit on the panel, and a count replaces the `4` the classifier spells in its arrays, masks and
  loops. `buttonName()` answers all six, and the panel task's press order lists all six so a press
  is still reported in panel order. The two new keys simply never produce a press yet. Done when the
  build and the check both pass unchanged.

- [x] **2. Read the keys through the 74HC165.** The `hal/impl` reader is one shift register on latch,
  clock and data: it latches and shifts eight inputs in on each settle instead of reading four pins,
  and the interrupt capture goes with the pins it needed. Six of the inputs are the keys in panel
  order, the two spares are tied high, and the input-to-key mapping is named once. `PanelPins` and
  the sketch's `Board` block carry latch on 10, clock on 11 and data on 1, as the plan settles, and
  name no fourth key pin. Done when the build passes, the parts guide and the alpha board's button
  row agree with the sketch, and on the wired rig the four gestures the pages already bind still
  work — Okay starts, Back stops and vents, Up+Down opens the settings — with no key reading
  another's bit.

## Epic 2 — The gesture classifier

- [x] **1. Write the gesture classifier.** One class owns the panel's key reader and answers each beat
  with what it detected: the press that completed, the keys still held, and the marks the plan
  reserves for the device. Keys pressed together are composed into one gesture. Nothing in it knows
  what a caller does with a gesture. Done when it builds, the panel owns one, and the check still
  walks every page.

- [x] **2. Make the panel task consume the classifier.** Replace the key scan, the chord and the
  threshold checks with the gesture the classifier answers, and keep only what to do with it —
  reboot, latch and post the stop, bump the way home, or hand the gesture to the page. Handing it on
  replaces the press pair with the string on the way to the page, so the queue behind `buttons.take()`,
  the binding and the harness carry the string, and a page reads it and splits it where a handler
  matches one dimension. The way home moves to Cancel while Back is held, so Okay is no longer stolen
  during the hold. Done when the panel task is one call and four decisions, the press log prints
  `btn: ok:single`, the check drives every page, and on the rig a Cancel during a Back hold goes home
  while an Okay during the same hold reaches the page.

## Epic 3 — The six-key map

- [x] **1. Rewrite the dispatch to whole gestures.** `on(gesture, fn)` takes one whole gesture string
  and a scene's `handle_page(ev)` compares whole strings, so nothing matches a key without its press.
  The six scenes bind the map the plan settles in the same change — the readout, the browser, the
  system scene, the settings list, the value scenes and the three routine pages — and the drive and
  the suites press the same gestures. Done when no `.be` file splits a gesture or registers less
  than a whole string, the check walks every page, and on the rig each of the six keys does what the
  map says.

## Epic 4 — The contract

- [x] **1. `gestures.md` describes six keys.** The keys and their tokens, the Back/Cancel split, the
  two-key rule, `single`/`double` with the extended-hold seconds, and the press-log line are the
  contract the pages are written against. Done when the spec names no four-key map, no `chord`
  token and no Okay-while-Back steal, and agrees with the firmware.

- [x] **2. `bindings.md` describes the string.** `buttons.take()` answers the classifier's gesture or
  `nil`, the vocabulary is the six keys and `single`/`double`/seconds, the queue holds eight,
  and the `event` record is gone from the record list. Done when the `buttons` section and the
  record list agree with the binding.

- [x] **3. `routine.md` describes the scene's gestures.** `handle(ev)` receives the whole gesture
  string, `on(gesture, fn)` is the declarative form, and the standard page gestures are the six-key
  ones. Done when the framework section names no `(btn, presses, fn)` and no `handle_page` key
  comparison, and agrees with the scenes in the image.

- [x] **4. `berry-harness.md` describes the host's press.** The record list loses `event`, the harness
  module pushes a gesture string, and the drive's key sweep is the six-key one. Done when the spec
  agrees with `tools/berry/`.

- [ ] **5. Close the effort.** The parking lot keeps the nudge line but drops the paragraph claiming it
  has no key, and the Gesture Classifier line goes. `docs/README.md` stops listing this backlog.
  With epic 4's summary committed and the epic merged, delete this file and the plan, commit the
  deletion on the backlog branch, and merge it into `feature/berry-test-harness`. Done when the
  deletion is one commit and no spec, guide, comment or completed summary points at either file.
