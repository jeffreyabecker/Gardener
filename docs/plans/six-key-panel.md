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
the six names, the string — are stated as requirements; the places where a choice is genuinely
open are flagged in [Open decisions](#open-decisions) with a recommendation.

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
Renaming it would churn four files and the log format for no behaviour. If this is wrong, it is
one line in `buttonName()` and one string in this document, but it has to be decided before the
work starts, because it is the one name that is already in use.

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
- `PanelTasks`'s `PressOrder` gains the two keys, because a press is reported in panel order and
  the go-home steal target is found by scanning it. The two-keys-together recognition the panel
  task used for the chord moves into the classifier, which is the thing that emits the event.

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
  pins and hands the others back to the board.

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

**Up+Down stays the settings gesture** — that is the recommendation for where the page's edit view
lives, and it needs no new key. It is still the one chord, one event on the first release, but the
classifier now emits it as the sum `up:short+down:short` rather than a `chord` token reported on
the up key. It does not move to Forward, because Forward already means "enter" and giving it a
second meaning is exactly the overloading this plan is removing. No other chord is added:
Forward+Back and the rest wait for a page that needs one.

### The reserved holds

These belong to the device and no page may suppress them. What changes:

| Gesture | Disposition | Reason |
| --- | --- | --- |
| **Hold Back 3 s** — stop and vent | **keeps** | Back is still Back; the release gesture is unchanged |
| **Back+Okay 5 s** — safe reset | **keeps**, `ok` → `okay` in name only | the one gesture that must work in every link state; it is not the place to experiment |
| **Okay while Back is held** — go home | **moves to Cancel** | Okay is confirm; with a Cancel key, "get me out" is Cancel's job, and Okay is no longer stolen during a hold |

The go-home move is the second real change. Today the panel task steals an Okay pressed while Back
is held and sends the panel home; with Cancel, the steal targets **Cancel while Back is held**
instead. The gesture stays device-owned — stolen in the panel task before any page sees it — so a
misbehaving page still cannot suppress the way out. (The alternative, a plain **Hold Cancel**, is
the open decision below; either way it stops being Okay.)

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
between `short` and a reported duration, the 350 ms double window, and the two-keys-together
recognition that today lives in the panel task. What changes is the output: the named `long` and
`chord` leave, and the classifier strings the gesture itself. The `Button` enum and `buttonName()`
stay; the `Press` enum and `pressName()` go, and the two-keys-together recognition moves from the
panel task into the classifier.

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
first. The declarative `on(btn, presses, fn)` registration stays for the cases that genuinely key
on a button and a press separately; a hand-rolled `handle` compares the string.

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

## Open decisions

These are the places the requirement does not yet name the answer. Each has a recommendation; the
backlog that breaks this plan down should only be written after they are settled, because the
string, the steal target and the token names all appear in the gesture map above.

1. **`ok` vs `okay` as the token.** Recommended: keep `ok` — it is the name already in the log,
   the enum and every routine. The glass can read "okay".
2. **Where go-home lands.** Recommended: **Cancel while Back is held**, stolen in the panel task,
   exactly as Okay-while-Back is today. The alternative is a plain Hold Cancel, which is simpler to
   describe but reserves a hold on a key a page may want for its own cancel-and-hold.
3. **The reset hold.** Recommended: keep **Back+Okay 5 s**. Nothing about the new keys makes a
   better reset; it only has to not move.

## What moves together when it lands

The work is one change and lands as one, because the token names appear in all of these at once:

- `hal/Button.h` (enum, `buttonName`; the `Press` enum and `pressName()` go, replaced by the
  classifier's string output), `hal/impl/Buttons.h`/`.cpp` replaced by the shift-register reader,
  `Panel.h` pin block, the sketch's `PanelPins`, `PanelTasks.cpp` (order, holds, steal — the chord
  recognition moves into the classifier).
- `routines/bindings/Buttons.cpp` (hands through the classifier string), `routines/Bindings.cpp`
  (`kRecords` without `event`), `modules/scene.be` and the five routine `.be` files and
  `modules/settings.be` (`handle(ev)` takes the string and compares it whole, splitting on `+` and
  `:` only for a dimension match).
- The specs when the work is done: [gestures.md](../specs/gestures.md) (six keys, the Back/Cancel
  split, the moved go-home, the sum chord and the seconds vocabulary),
  [bindings.md](../specs/ui/bindings.md) (`buttons.take()` answers the classifier's string) and
  [routine.md](../specs/ui/routine.md) (`handle(ev)` takes the string). The specs still describe
  the `Page`/`page.be` framework names that an in-flight rename has already moved to
  `Scene`/`scene.be`; that reconciliation happens with the same pass.
