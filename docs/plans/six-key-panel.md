# Six-key panel

Extend the panel from the four keys the 5-way switch exposes — up, down, ok, back — to six
labelled keys: **up, down, forward, back, okay, cancel**. The two new keys do for free what the
current map works around: `back` today is both "leave the current view" and "stop the run", `ok`
today is both "advance a duration stage" and "save", and the page's settings view has to hide
behind the Up+Down chord because no single key can carry it.

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
| Forward | `forward` | enter: open the selection, advance a stage, next item |
| Back | `back` | leave one level up; the page's own whole stop of a run |
| Okay | `ok` | confirm: the primary action — start, save, select |
| Cancel | `cancel` | escape: leave without writing, abort an edit |

The tokens are the values `buttonName()` answers and the values a page compares in Berry. They are
also the words in the press log (`btn: forward:short`), so one vocabulary serves the wire log, the
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
  press classifier's thresholds are **unchanged** — 20 ms debounce, the 600 ms mark between a tap
  and a reported hold, 350 ms double window. The two new keys are ordinary keys: they debounce,
  classify and double exactly like the four existing ones. What the classifier *reports* changes
  with part 3 — it emits a gesture string directly, and a hold is its seconds rather than `long` —
  but the mechanics behind it are the same ones the GPIO reader used.
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
| **Forward** | enter: open the selection, advance a duration stage, next item |
| **Back** | leave one level up; the page's own whole stop of a run |
| **Okay** | confirm: start, save, select |
| **Cancel** | escape: leave without writing, abort an edit |
| **Up+Down** | the page's edit/settings view |

The one genuinely new rule is the **Back/Cancel split**, which is the point of the sixth key.
Today [the gesture contract](../specs/gestures.md) has Back do both "leave the current view without
writing" and "stop what the page is running". Split them:

- **Back** is *structural*: it answers "where am I". From a value editor it returns to the list,
  from a sub-view it returns to the parent, from a top-level page it stops the run. It is the
  navigation key and the stop key it always was.
- **Cancel** is *transactional*: it answers "do I keep this". It abandons an edit without writing,
  aborts a pending action, and returns a run to rest without leaving the page.

On a view with no parent the two coincide, exactly as a single key did before — a page that is not
editing and is not running may take Cancel as nothing or as home. On a view with a parent they
differ, and that difference is what the key buys.

**Forward is the enter key, and Okay stops being two things.** Today Okay opens a value in the
settings list *and* saves it in the editor, and in a `duration` it advances minutes→seconds *and*
then saves. With Forward, the navigation half moves off Okay:

- In a list, **Forward opens the selection** (today Okay does this).
- In an editor, **Okay saves** and **Forward advances the stage** — a `duration` moves from minutes
  to seconds on Forward and writes on Okay, so the `if kind == "duration"` special case in the
  editor goes away.
- On a run page, **Okay stays start/resume**, and **Forward is the page's own "next"** if the page
  has a next — the nudge the parking lot noted had no key. A page without a next simply does not
  take Forward.

Where enter and confirm are the same action — choosing a routine, opening a value — a page may take
either key for it; the standard only requires that Okay always means confirm and Forward always
means advance.

**Up+Down stays the settings gesture** — that is where the page's edit view lives, and it needs no
new key. The classifier emits it as the sum `up:short+down:short` rather than a `chord` token
reported on the up key, and the event goes out when the **last** of the two comes up, so each part
is named from a press that has finished. It does not move to Forward, because Forward already
means "enter" and giving it a second meaning is exactly the overloading this plan is removing.
No other *page* chord is added: Forward+Back and the rest wait for a page that needs one. The sum
is not a list of chords, though — any keys down together are one event, as
[the two-key rule](#the-two-key-rule) says.

### The reserved holds

These belong to the device and no page may suppress them. What changes:

| Gesture | Disposition | Reason |
| --- | --- | --- |
| **Hold Back 3 s** — stop and vent | **keeps** | Back is still Back; the release gesture is unchanged |
| **Back+Okay 5 s** — safe reset | **keeps**, `ok` → `okay` in name only | the one gesture that must work in every link state; it is not the place to experiment |
| **Okay while Back is held** — go home | **moves to Cancel** | Okay is confirm; with a Cancel key, "get me out" is Cancel's job, and Okay is no longer stolen during a hold |

The go-home move is the second real change. Today the panel task steals an Okay *press* while Back
is held, and a press is classified on release, so it can only act then. Cancel is taken from the
**debounced level** instead: while Back is held, Cancel going down bumps the go-home counter once,
and the press that follows is not delivered. The level is the only thing that works once a pair is
one event — a Cancel tapped during Back's hold is part of `back:3+cancel:short`, and waiting for
that string would hold the way out until Back came up. The gesture stays device-owned — taken in
the panel task before any page sees it — so a misbehaving page still cannot suppress the way out.

### The two-key rule

Two or more keys whose debounced intervals overlap are **one event**, the sum of their parts in
panel order, emitted when the last of them comes up. `back:3+ok:2` is a real event, not just a
vocabulary example, and the settings chord is the same mechanism rather than a special case in the
classifier. A key in such a group is not also half of a double: the double window belongs to a key
pressed on its own.

The event goes out on the last release rather than the first. A part is named by how far it got —
`short`, `double`, or its whole seconds — and only a released key has finished a press; composing
at the first release would have to guess the still-held partner's part. For the settings chord the
difference is invisible, except that the view opens when the second key comes up rather than the
first.

### The press log

While an external link drives there is no page, so presses are logged as the classifier's string
with a `btn: ` prefix: `btn: ok:short`, `btn: back:3`, `btn: up:short+down:short`. The log and the
event are the same string; the vocabulary simply grows `forward` and `cancel`, and a hold becomes
its seconds.

### The shipped pages

These bindings are the pages' own to write, but the standard above settles them in advance:

- **`default`** — Okay short starts, Back stops and vents. The double-tap shortcuts (Up double →
  browser, Down double → system) can go: **Forward opens the browser** and **Up+Down opens the
  system settings** as it does today.
- **`browser`** — Up/Down move, Forward (or Okay) opens the selected routine, Back returns to
  `default`, Up+Down opens the selected routine's settings.
- **`system`** — Up/Down move rows, Forward (or Okay) opens the row, Okay saves in an editor, Back
  returns to `default` or back out of the editor, Cancel discards the edit.
- **the settings editor** — Up/Down change the value, Forward opens/advances, Okay saves, Back and
  Cancel both leave; Cancel discards and Back keeps "back out one level" so a `duration` can step
  minutes→seconds with Forward and back with Back.
- **a routine page** — Okay starts or resumes, and Cancel returns the run to rest and leaves the
  page showing: the two ramped pages already do this on Back, and `interval` gains it, so the key
  means the same thing on all three. **Back is a double tap**: one Back does nothing on a running
  routine page, Back double stops the run and leaves for the browser. A single tap is too cheap for
  the one action that takes a run away, and while a run is going the page is where the user is
  looking. Forward is the page's own "next" and a page without one does not take it; Up+Down is
  its settings.

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

- A **tap** is `short`; a **double tap** is `double` — the two a page has always compared.
- A **hold** reports the **whole seconds the key was held** instead of `long`: the held
  milliseconds divided by 1000, so `back:3` is Back held three and a bit seconds and `up:0` is a
  hold just past the tap mark. The classifier already measures the hold; it now reports it.
- **Two keys together are one event, the sum of their parts.** The `chord` token is gone, and Up
  and Down pressed together is `up:short+down:short`; a held pair reports each part's seconds,
  `back:3+ok:2`.

So the full vocabulary is `up`, `down`, `forward`, `back`, `ok`, `cancel` before the colon, and
`short`, `double` or a whole number of seconds after it, joined with `+` when the gesture is more
than one key. No button or press name contains `:` or `+`, so both are safe separators.

The classifier's mechanics do not change with the vocabulary — 20 ms debounce, the 600 ms mark
between `short` and a reported duration, and the 350 ms double window all stay. What changes is the
output: the named `long` and `chord` leave, and the classifier strings the gesture itself. The
`Button` enum and `buttonName()` stay; the `Press` enum and `pressName()` go; the
two-keys-together recognition moves out of the panel task and into the classifier, which is the
thing that emits the event.

A press is queued only while the page is showing, the queue holds eight, and the binding's only job
is to hand the queued string to `take()`.

The scene framework hands the string through unchanged; a page matches it whole or splits it when
it wants the parts:

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

`handle(ev)` receives the whole string — `"ok:short"`, `"back:3"`, `"up:short+down:short"` — and
the direction for the Berry side is to **compare the whole string wherever a page can**. The
shipped pages' nested `if ev.btn == "up"` ladders are each one string compared, and they become
one test:

```berry
if ev == "ok:short"              # not ev.btn == "ok" && ev.press == "short"
if ev == "up:short+down:short"   # the settings chord, matched whole
```

A page splits on `+` and `:` only when it matches one dimension regardless of the other — an
editor that takes Up and Down at any press, say — and even then it looks at the whole string
first. The declarative registration takes the same shape: `on(gesture, fn)` is given the whole
event string, and `*` in the press slot when the press does not matter (`on("ok:*", fn)`). There
is no registration that names a bare key: "any press" is `ok:*`, which says so. There is no list
of press names either — `on("back", ["short", "double"], fn)` was two registrations written as
one, and it cannot express a hold at all.

Why the classifier emits strings, stated plainly:

- The `event` record existed because a routine reads a record with a dot, but its two fields were
  the key and a press name. The key is already a string, and the press name is exactly what the
  classifier decides — so the classifier writing the string is the shortest path from a key to a
  page.
- A hold's seconds is data the classifier has and a page wants; `long` threw it away and made the
  page re-derive "long enough" from a threshold it could not see.
- The `+` sum is what `chord` was pretending not to be: two keys, each with its own press. The sum
  says it outright and drops the special token.

The alternative — keep the record — is the no-op option, and it is only worth taking if a press
ever needs a third field. Nothing in sight needs one; a gesture is one or more keys and how each
was pressed, and the string above is the whole of it.

## Decisions

These were the open questions, settled before the backlog exists because the string, the steal
target and the token names all appear in the gesture map above. The first three were open in the
first draft; the rest came out of reading the code against the map.

1. **The token is `ok`, not `okay`.** It is the name the press log, `Button::Ok`, `buttonName()` and
   every routine already use. The key is called "okay" and the glass may spell it either way; the
   token does not move.
2. **Go-home is Cancel while Back is held**, taken from the debounced level in the panel task as
   described above. A plain Hold Cancel was the alternative; it reserves a hold on the key a page
   most wants for its own cancel, so it is not taken.
3. **The reset stays Back+Okay for 5 s.** Nothing about the new keys makes a better reset, and it
   is the one gesture that must work in every link state.
4. **The sum is general.** Any keys down together are one event, in panel order, on the last
   release. Up+Down is the only *page* chord; that is a statement about the standard, not a
   special case in the classifier.
5. **A registration is a whole gesture string.** `on("ok:short", fn)`, `on("up:short+down:short",
   fn)`, and `*` in the press slot for any press on that key. A bare key name is not a gesture, and
   a list of press names is several registrations with a hole where the hold would go.
6. **The harness presses a whole gesture.** `harness.press("ok:short")` is exactly what the device
   produces, so the host has no button or press table to drift from the classifier.

## What moves together when it lands

The work is one change, and its parts share the vocabulary, so no part can stand alone for long:

- `hal/Button.h` (enum and `buttonName`; the `Press` enum and `pressName()` go, replaced by the
  classifier's string output), `hal/impl/Buttons.h`/`.cpp` as the shift-register reader,
  `Panel.h`'s pin block, the sketch's `Board` block, `PanelTasks.cpp` (press order, holds and the
  steal — the chord recognition moves into the classifier).
- `routines/bindings/Buttons.cpp` (hands through the classifier string), `routines/Bindings.cpp`
  (`kRecords` without `event`), `modules/scene.be`, `modules/ui.be`, the settings scenes and the
  three files under `routines/` (`handle(ev)` takes the string and compares it whole, splitting on
  `+` and `:` only for a dimension match).
- The host harness, which is the other caller of the same seam: `tools/berry/host/HarnessModule.cpp`
  (`harness.press` takes the gesture), `harness.be` (the event record goes), `driver.be` and the
  test files that press keys.
- The specs when the work is done: [gestures.md](../specs/gestures.md) (six keys, the Back/Cancel
  split, the moved go-home, the sum chord and the seconds vocabulary),
  [bindings.md](../specs/ui/bindings.md) (`buttons.take()` answers the classifier's string) and
  [routine.md](../specs/ui/routine.md) (`handle(ev)` takes the string).
