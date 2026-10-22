# Six-key panel

Extend the panel from the four keys the 5-way switch exposes — up, down, ok, back — to six
labelled keys: **up, down, forward, back, okay, cancel**. The two new keys do for free what the
current map works around: `back` today is both "leave the current view" and "stop the run", `ok`
today is both "advance a duration stage" and "save", and the page's settings view has to hide
behind a double tap because no single key can carry it.

This plan has three parts, all of which land together:

1. **A shift-register reader.** The six keys are read through a 74HC165, not through six GPIOs.
2. **A gesture map for six keys.** Which presses stay, which change, and what the two new keys
   mean. This picks up the [Gesture Classifier](./parking-lot.md) parking-lot note.
3. **The classifier emits gesture strings.** `buttons.take()` answers a string the C++ classifier
   wrote itself, instead of an `event` record a binding assembles.

What has to be true when it is done is stated here. The design decisions already made — the part,
the six names, the string — are stated as requirements; the choices that were open are recorded in
[Decisions](#decisions) with the answer taken.

This plan supersedes the "button module" line in [alpha-pcb.md](./alpha-pcb.md): instead of a
header carrying a bought key module, the board carries the six switches and the 74HC165, and the
firmware reads them. The rest of that plan is unaffected.

## The six keys

| Key | Token | Role at the page level |
| --- | --- | --- |
| Up | `up` | move the selection up, or raise the value being edited |
| Down | `down` | move the selection down, or lower the value being edited |
| Forward | `forward` | enter: open the selection, advance a stage |
| Back | `back` | leave one level up |
| Okay | `ok` | confirm: the primary action — start, save, select |
| Cancel | `cancel` | escape: stop the run, discard an edit, leave the routine |

The tokens are the values `buttonName()` answers and the values a page compares in Berry. They are
also the words in the press log (`btn: forward:single`), so one vocabulary serves the wire log, the
Berry event and the C++ enum.

**`ok`, not `okay`.** The key's human name is "okay" and the glass may say whatever it likes, but
the token stays `ok`. `ok` is the name the press log, the `Button::Ok` enumerator, `buttonName()`
and every routine that writes `ev.btn == "ok"` already use, and nothing anywhere spells it `okay`.
Renaming it would churn four files and the log format for no behaviour. It is settled here, before
the work starts, because it is the one name already in use.

## Part 1 — the 74HC165 reader

The six switches are read through one 74HC165: an 8-bit parallel-in/serial-out shift register.
This is a new `hal/impl` implementation of the existing `ButtonPanel` contract; the contract itself
does not change.

### What has to be true

- The reader satisfies `ButtonPanel` exactly as the GPIO reader does: `pressed(Button)`,
  `consumePress(Button)`, `dropPress(Button)` and `heldMs(Button)` keep their meanings, and the
  press classifier's mechanics are **unchanged** — 20 ms debounce, 350 ms double window, and the
  2 s mark between a press and a hold. The two new keys are ordinary keys: they debounce, classify
  and double exactly like the four existing ones. What the classifier *reports* changes with part
  3 — it emits a gesture string directly, and a hold past the 2 s mark says its seconds — but the
  mechanics behind it are the same ones the GPIO reader used.
- The `Button` enum gains two members and a count that stops being written as `4` in five places
  in `hal/impl/Buttons.cpp`. `enum class Button : uint8_t { Up, Down, Forward, Back, Ok, Cancel }`
  and a `kButtonCount = 6` are the shape; the `pressStartMs_`, `pendingPress_` and
  `firstReleaseMs_` arrays and the 32-bit masks all follow the count. Six bits of a 32-bit mask is
  not a change of mechanism, only of bound.
- The panel's order gains the two keys, because a press is composed in panel order: the enum is
  declared in the order the keys sit on the panel, so the order is the declaration. `PressOrder`
  exists only while the panel task reports a press per key; once the classifier composes the
  gesture it has no reader. The two-keys-together recognition the panel task used for the chord
  moves into the classifier, which is the thing that emits the event.

### The part

- **Three signal pins, not six.** `SH/LD` (latch), `CLK` (clock) and `Q7` (serial data); `CLK INH`
  is tied low. A read is: pulse `SH/LD` low to latch the eight parallel inputs, then clock eight
  times reading `Q7` — the part shifts out its `H` input first, so the bit order is fixed and the
  reader names each bit.
- **Six of the eight inputs are the keys**; the two spare inputs are tied high, because the 74HC165
  has no internal pull-ups and a floating input reads as noise. All six keys are active-low with
  pull-ups to 3.3 V — the same sense the module's switches used, so the `LOW == pressed` logic in
  the reader is unchanged.
- **Three GPIOs instead of four**, on the `3V3_SENSE` rail the OLED, sensor and switches already
  share. The current rig spends GPIO 1, 2, 10 and 11 on four keys; the shift register needs three
  pins and hands the other back to the board. The rig takes GPIO 10 for latch, 11 for clock and 1
  for data, leaving GPIO 2 spare, and the inputs in shift order are `H` up, `G` down, `F` forward,
  `E` back, `D` ok, `C` cancel, with `B` and `A` tied high.

### What leaves

- **The change interrupts go.** A 74HC165 has no change output, so the ISR capture path the GPIO
  reader uses (`isr`/`raw_`/`readPins` by `digitalRead`) does not carry over. The reader shifts the
  register in on each panel beat — the panel task already runs on a 10 ms cadence, so the 20 ms
  settle sees the level hold still across two or three samples, which is all the debounce needs.
- **The read, not the classifier, is what changes.** The settle-on-read design — the level accepted
  only after it holds still, the raw-edge hold measurement, the double window, the key-held-at-boot
  exclusion — is input-agnostic and stays. Only the source of the eight bits changes: a shift-in
  instead of `digitalRead()` per pin.
- **The old GPIO reader is not kept beside the new one.** The board replaces the module, so the
  direct-GPIO `Buttons` leaves with it. It is deleted, not flagged or kept for the POC, once the
  shift-register reader reads the board.

## Part 2 — the gesture map

Six keys instead of four, and the map below is the standard. Everything that is reserved stays
reserved; what changes is the split of the two jobs that one key used to do.

### The standard page gestures

| Gesture | Meaning |
| --- | --- |
| **Up / Down** | move the selection, or change the value being edited |
| **Forward** | enter: open the selection, advance a duration stage |
| **Back** | leave one level up |
| **Okay** | confirm: start, save, select |
| **Cancel** | escape: stop the run, discard an edit, leave the routine |

The standard is the singles. A double is a page's own shortcut and no page is required to take
one: the shipped pages use `up:double` for the edit/settings view, `down:double` for the system
settings on the readout, and `cancel:double` to leave a routine.

The one genuinely new rule is the **Back/Cancel split**, which is the point of the sixth key.
Today [the gesture contract](../specs/gestures.md) has Back do both "leave the current view without
writing" and "stop what the page is running". Split them:

- **Back** is *structural*: it answers "where am I". From a value editor it steps back a stage, from
  a sub-view it returns to the parent, from a list it returns to the readout.
- **Cancel** is *transactional*: it answers "do I keep this". It abandons an edit without writing,
  stops the run and vents, and on a routine page a single pauses while a double leaves the stack.

The stop leaves Back and lands on Cancel, so Back is navigation alone and Cancel is the one key
that stops what is running.

**Forward is the enter key, and Okay stops being two things.** Today Okay opens a value in the
settings list *and* saves it in the editor, and in a `duration` it advances minutes→seconds *and*
then saves. With Forward, the navigation half moves off Okay:

- In a list, **Forward opens the selection** (today Okay does this) — the browser opens the selected
  routine's settings on Forward.
- In an editor, **Okay saves** and **Forward advances the stage** — a `duration` moves from minutes
  to seconds on Forward and writes on Okay, so the `if kind == "duration"` special case in the
  editor goes away.

Where enter and confirm are the same action — opening a value — a page may take either key for it;
the standard only requires that Okay always means confirm and Forward always means advance.

**The settings view is Up double.** The edit view no longer hides behind a chord: a routine page
opens its settings on `up:double`, and the readout opens the browser on the same gesture. The chord
itself goes — no page binds a two-key sum, and the `chord` token the panel task used to report it
leaves the vocabulary. Forward keeps its one meaning rather than doubling as the settings key.

### The reserved gesture

One gesture belongs to the device and no page may suppress it: **Back and Okay held together for
5 s** is the safe reset — stop, vent and restart — whatever is showing and whatever link owns the
machine. The panel task takes it from the gesture string before any page sees it.

Everything else that used to be device-owned is a page gesture now. The Back hold that stopped and
vented is `cancel:single` on the pages that run something, and the way home is `cancel:double` on a
routine page; a page owns the whole stop, so no hold is stolen at the panel level.

### The two-key rule

Two or more keys whose debounced intervals overlap are **one event**, the sum of their parts in
panel order, emitted when the last of them comes up. `back:3+ok:2` is a real event, not just a
vocabulary example. A key in such a group is not also half of a double: the double window belongs
to a key pressed on its own.

The event goes out on the last release rather than the first. A part is named by how far it got —
`single`, `double`, or its whole seconds — and only a released key has finished a press; composing
at the first release would have to guess the still-held partner's part.

No page binds a two-key sum today. The mechanism is there because a held pair has to answer as one
event rather than two; the standard map uses singles and doubles, so the sum waits for a page that
needs one.

### The press log

While an external link drives there is no page, so presses are logged as the classifier's string
with a `btn: ` prefix: `btn: ok:single`, `btn: back:3`, `btn: up:double`. The log and the event are
the same string; the vocabulary simply grows `forward` and `cancel`, and a hold that goes on past
the extended mark becomes its seconds.

### The shipped pages

These bindings are the pages' own to write, but the standard above settles them in advance:

- **the readout** — `ok:single` starts or resumes, `cancel:single` stops and vents, `up:double`
  opens the browser, `down:double` opens the system settings.
- **the browser** — `up:single`/`down:single` move, `ok:single` opens the selected routine,
  `forward:single` opens the selected routine's settings, `back:single` leaves.
- **the system settings** — `up:single`/`down:single` move rows, `ok:single` opens a row,
  `back:single` leaves.
- **the settings list** — `up:single`/`down:single` move rows, `ok:single` opens a row,
  `back:single` closes the list.
- **a value scene** — `up:single`/`down:single` change the value, `ok:single` keeps it,
  `cancel:single` discards it. A `duration` adds `forward:single` to advance minutes→seconds and
  `back:single` to step back.
- **a routine page** — `ok:single` begins or resumes, `cancel:single` stops and vents and leaves the
  page paused, `cancel:double` stops and vents and leaves the stack, `up:double` opens its settings.

## Part 3 — the classifier emits gesture strings

The press classifier in C++ stops handing the rest of the firmware a `Press` enum that a binding
later names, and instead **emits the gesture string itself**. `buttons.take()` answers that string,
or `nil` when the queue is empty. The Berry binding no longer builds a record or maps an enum to a
name; it hands through what the classifier produced, and the `event` record class leaves
`kRecords` — `reading`, `unit` and `bounds` stay, because those are genuinely multi-field values a
routine reads by name.

The string is one event with one part per key:

```
part  := button ':' press
event := part ( '+' part )*
```

- A **tap** is `single`; a **double tap** is `double` — the two a page compares.
- A **hold past the 2 s mark** reports the **whole seconds the key was held**: the held
  milliseconds divided by 1000, so `back:3` is Back held three and a bit seconds and `back:2` is a
  hold just past the mark. Below the mark a release is `single`; there is no `long` form.
- **Two keys together are one event, the sum of their parts.** Up and Down pressed together is
  `up:single+down:single`; a held pair reports each part in its own form, `back:3+ok:single`.

So the full vocabulary is `up`, `down`, `forward`, `back`, `ok`, `cancel` before the colon, and
`single`, `double` or a whole number of seconds after it, joined with `+` when the gesture is more
than one key. No button or press name contains `:` or `+`, so both are safe separators.

The classifier's mechanics do not change with the vocabulary — 20 ms debounce and the 350 ms double
window stay, and the 2 s mark separates a press from a hold reported as seconds. What changes is
the output: `short` is renamed `single`, `long` and `chord` leave, an extended hold says its
seconds, and the classifier strings the gesture itself. The `Button` enum and `buttonName()` stay;
the `Press` enum and `pressName()` go; the two-keys-together recognition moves out of the panel
task and into the class that detects the gesture, which is the thing that emits the event.

A press is queued only while the page is showing, the queue holds eight, and the binding's only job
is to hand the queued string to `take()`.

### One class answers the beat

The drain and the sum are one piece of state, so they are one class rather than a loop in the panel
task. `GestureClassifier` owns the reader, settles it on each beat and answers the gesture that
completed, composed into one string. A second gesture completed in the same beat waits for the
next, so one beat is one delivery.

`buttonsTick` becomes that call plus the one policy the panel keeps: reboot when the string reads
`back:5+ok:5`, and hand anything else to the page. Nothing left in it re-derives a threshold, scans
the keys or composes a string.

The scene framework hands the string through unchanged; a page matches it whole or not at all:

```berry
import string

def take_events()
  var acted = false
  while true
    var ev = buttons.take()
    if ev == nil
      break
    end
    if self.handle(ev)
      acted = true
    end
  end
  return acted
end
```

`handle(ev)` receives the whole string — `"ok:single"`, `"back:3"`, `"up:double"` — and
the direction for the Berry side is to **compare the whole string, always**. The shipped pages'
nested `if ev.btn == "up"` ladders are each one string compared, and they become one test:

```berry
if ev == "ok:single"    # not ev.btn == "ok" && ev.press == "single"
if ev == "up:double"    # the settings gesture, matched whole
```

No page matches one part of a gesture without the other. Splitting a gesture on `+` or `:` to
match a key regardless of press — or the reverse — is a bug, not an option, so no page does it and
the framework does not offer it. A registration is the same shape: `on(gesture, fn)` takes one
whole gesture string and matches it exactly. There is no `*`, no bare key and no list of press
names — `on("back", ["single", "double"], fn)` was two registrations written as one, and each of
those forms is a way to match a part without the whole.

Why the classifier emits strings, stated plainly:

- The `event` record existed because a routine reads a record with a dot, but its two fields were
  the key and a press name. The key is already a string, and the press name is exactly what the
  classifier decides — so the classifier writing the string is the shortest path from a key to a
  page.
- A hold's seconds is data the classifier has and a page wants. An extended hold says how long it
  was, and a release below the mark is a single — there is no `long` to re-derive "long enough"
  from a threshold a page cannot see.
- The `+` sum is what `chord` was pretending not to be: two keys, each with its own press. The sum
  says it outright and drops the special token.

The alternative — keep the record — is the no-op option, and it is only worth taking if a press
ever needs a third field. Nothing in sight needs one; a gesture is one or more keys and how each
was pressed, and the string above is the whole of it.

## Decisions

These were the open questions, settled before the backlog exists because the string, the steal
target and the token names all appear in the gesture map above. The first three were open in the
first draft, the next three came out of reading the code against the map, and the last two correct
the drafts that had seconds replacing `long`, and then `long` surviving at all.

1. **The token is `ok`, not `okay`.** It is the name the press log, `Button::Ok`, `buttonName()` and
   every routine already use. The key is called "okay" and the glass may spell it either way; the
   token does not move.
2. **The way home is Cancel double.** A routine page stops and vents on `cancel:single` and leaves
   the stack on `cancel:double`, so no hold is stolen at the panel level and Okay is never taken
   from a page during a hold.
3. **The reset stays Back+Okay for 5 s.** Nothing about the new keys makes a better reset, and it
   is the one gesture that must work in every link state.
4. **The sum is general, and no page binds one.** Any keys down together are one event, in panel
   order, on the last release. The settings view is a double, not a chord, so the sum waits for a
   page that needs one.
5. **A registration is a whole gesture string.** `on("ok:single", fn)`, `on("up:double", fn)`.
   A page matches a gesture whole or not at all: no `*`, no bare key, no list of press names, and
   no splitting on `:` or `+` anywhere in the framework.
6. **The harness presses a whole gesture.** `harness.press("ok:single")` is exactly what the device
   produces, so the host has no button or press table to drift from the classifier.
7. **`single`, `double` and seconds are the forms.** `short` is renamed `single`, and `long` is
   gone: a release is a single or a double, and a hold that reaches the 2 s mark reports whole
   seconds instead. The 600 ms mark between a tap and a hold goes with it.
8. **Gesture detection is one class; the panel task keeps the reset.** `GestureClassifier` answers
   the beat with the completed gesture. `buttonsTick` reboots on the reset string and hands
   everything else to the page.

## What moves together when it lands

The work is one change, and its parts share the vocabulary, so no part can stand alone for long:

- `hal/Button.h` (enum and `buttonName`; the `Press` enum and `pressName()` go, replaced by the
  classifier's string output), `hal/impl/Buttons.h`/`.cpp` as the shift-register reader,
  `panel/GestureClassifier.h`/`.cpp` (the beat's gesture), `Panel.h`'s pin
  block and `gestures()` accessor, the sketch's `Board` block, and `PanelTasks.cpp` reduced to the
  reset.
- `routines/bindings/Buttons.cpp` (hands through the classifier string), `routines/Bindings.cpp`
  (`kRecords` without `event`), `modules/scene.be`, `modules/ui.be`, the settings scenes and the
  three files under `routines/` (`handle(ev)` takes the string and compares it whole, never
  matching one part without the other).
- The host harness, which is the other caller of the same seam: `tools/berry/host/HarnessModule.cpp`
  (`harness.press` takes the gesture), `harness.be` (the event record goes), `driver.be` and the
  test files that press keys.
- The specs when the work is done: [gestures.md](../specs/gestures.md) (six keys, the Back/Cancel
  split, the way home, the two-key rule and the extended-hold seconds),
  [bindings.md](../specs/ui/bindings.md) (`buttons.take()` answers the classifier's string) and
  [routine.md](../specs/ui/routine.md) (`handle(ev)` takes the string).
