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
the map can be written against it — `up:short+down:short` is not expressible while a press is an
`event` record with a `chord` token — so the reader (part 1) comes first, then the string (part 3),
then the map (part 2), then the contract.

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

- [ ] **2. Read the keys through the 74HC165.** The `hal/impl` reader is one shift register on latch,
  clock and data: it latches and shifts eight inputs in on each settle instead of reading four pins,
  and the interrupt capture goes with the pins it needed. Six of the inputs are the keys in panel
  order, the two spares are tied high, and the input-to-key mapping is named once. `PanelPins` and
  the sketch's `Board` block carry latch on 10, clock on 11 and data on 1, as the plan settles, and
  name no fourth key pin. Done when the build passes, the parts guide and the alpha board's button
  row agree with the sketch, and on the wired rig the four gestures the pages already bind still
  work — Okay starts, Back stops and vents, Up+Down opens the settings — with no key reading
  another's bit.

## Epic 2 — The gesture is a string

- [ ] **1. A registration is a whole gesture.** `on(gesture, fn)` replaces `on(btn, presses, fn)`: a
  registration is the event string it matches, `*` in the press slot means any press on that key,
  and the list of press names becomes one registration per string. Every shipped registration is
  rewritten — the five settings chords become `on("up:chord", fn)` for now — and the framework
  matches whole strings instead of one key and one press. The four hand-rolled `handle_page`
  overrides compare the string they are given rather than reading `ev.btn` and `ev.press`. Done when
  no `.be` file passes a bare key or a list to `on`, every page still walks, and the check passes.

- [ ] **2. The classifier emits the string.** The press is named in one place: the classifier queues
  `key:press` with the tokens it has today, `ButtonPanel` hands the panel task one string per press,
  the scene queue holds strings, `buttons.take()` answers one, and the `event` record leaves
  `kRecords`. The framework stops assembling the string from the record. The panel task keeps its
  own chord, delivering `up:chord` until the two-key item takes it over, and the harness joins the
  two names it is given so the host keeps its current call until the next item. Done when the check
  drives every routine, the press log prints `btn: ok:short`, and `hal/Button.h` declares neither
  `Press` nor `pressName`.

- [ ] **3. The harness presses a gesture.** `harness.press("ok:short")` replaces the button and press
  arguments, so the host pushes exactly what the device would and has no button or press table to
  drift from the classifier. `harness_event` goes with the record, and the drive and the suites that
  press keys press strings. Done when the check's drive and suites press only strings and pass.

- [ ] **4. An extended hold reports its seconds.** `short`, `long` and `double` stay exactly as they
  are; a hold at or past the **2 s** mark reports the whole seconds instead, so `back:long` is an
  ordinary hold and `back:3` one that has gone on. `ExtendedHoldMs` sits beside `LongPressMs` and
  `DoublePressMs`, and the drive's sweep presses a short, a double and a hold on each key so the
  `long` form still has a caller. Done when the build passes and, with a link driving, the
  press log shows `btn: back:long` for a one-second press and `btn: back:3` for a three-second one;
  the spec that describes the vocabulary follows in epic 4.

- [ ] **5. Two keys together are one event.** The classifier composes keys that are down together into
  one string in panel order and emits it when the last comes up, so the panel answers one whole
  gesture rather than one press per key, and its chord state, `dropPress` and `PressOrder` all go.
  Up+Down is `up:short+down:short` and the five settings registrations name that string; go-home
  becomes the debounced Cancel level while Back is held, and the press that follows is not
  delivered. Done when a driven Up+Down opens a routine's settings and the check passes; go-home is
  the panel task's, which the harness does not run, so it is checked on the rig with the other
  keys.

## Epic 3 — The six-key map

- [ ] **1. The readout takes the map.** Forward opens the browser, Up+Down opens the system settings,
  Okay starts or resumes, Back stops and vents, and Cancel does nothing. The two double-tap
  shortcuts are gone with the keys that no longer need them. Done when the driven readout reaches
  the browser on Forward and the check passes.

- [ ] **2. The browser takes the map.** Forward opens the selected routine as Okay does, Up+Down opens
  the selected routine's settings, Up/Down move, and Back returns to the readout. Done when the
  browser suite still lists every routine and the check passes.

- [ ] **3. The system scene takes the map.** Forward opens the row Okay would, Okay saves in the value
  scene, Cancel discards the edit, and Back leaves the value scene and then the system scene. Done
  when the system suite walks the jingle row with Forward to open and Okay to save, and passes.

- [ ] **4. The settings list takes the map.** Forward opens the row and Okay still does, and Back and
  Cancel both close the list. Done when the shared settings walk opens a row on Forward and the
  check passes.

- [ ] **5. A value scene takes the map.** Okay saves, Forward moves a `duration` from minutes to
  seconds, Back steps back a stage and then leaves, Cancel discards the value and leaves, and each
  scene's hint names the keys that act. Done when the shared settings walk edits a `duration` with
  Forward and Okay, and passes.

- [ ] **6. The routine pages take the map.** Cancel returns the run to rest and leaves the page
  showing on all three — the two ramped pages move their pause there, and `interval` gains it — Back
  double stops the run and pops to the browser, one Back does nothing to a running page, Okay starts
  or resumes, and Up+Down opens the settings. Forward is not taken. Done when the two pause/resume
  suites press Cancel, a run is left by a Back double, the check passes, and on the rig each of the
  six keys does its own thing — which is the first point every key is bound.

## Epic 4 — The contract

- [ ] **1. `gestures.md` describes six keys.** The keys and their tokens, the Back/Cancel split, the
  moved go-home, the two-key rule, `short`/`long`/`double` with the extended-hold seconds, and the
  press-log line are the contract the pages are written against. Done when the spec names no
  four-key map, no `chord` token and no Okay-while-Back steal, and agrees with the firmware.

- [ ] **2. `bindings.md` describes the string.** `buttons.take()` answers the classifier's gesture or
  `nil`, the vocabulary is the six keys and `short`/`long`/`double`/seconds, the queue holds eight,
  and the `event` record is gone from the record list. Done when the `buttons` section and the
  record list agree with the binding.

- [ ] **3. `routine.md` describes the scene's gestures.** `handle(ev)` receives the whole gesture
  string, `on(gesture, fn)` is the declarative form, and the standard page gestures are the six-key
  ones. Done when the framework section names no `(btn, presses, fn)` and no `handle_page` key
  comparison, and agrees with the scenes in the image.

- [ ] **4. `berry-harness.md` describes the host's press.** The record list loses `event`, the harness
  module pushes a gesture string, and the drive's key sweep is the six-key one. Done when the spec
  agrees with `tools/berry/`.

- [ ] **5. Close the effort.** The parking lot keeps the nudge line but drops the paragraph claiming it
  has no key, and the Gesture Classifier line goes. `docs/README.md` stops listing this backlog.
  With epic 4's summary committed and the epic merged, delete this file and the plan, commit the
  deletion on the backlog branch, and merge it into `feature/berry-test-harness`. Done when the
  deletion is one commit and no spec, guide, comment or completed summary points at either file.
