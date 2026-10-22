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
3. **The event as a string.** `buttons.take()` answers one string instead of an `event` record.

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
also the words in the press log (`btn: forward short`), so one vocabulary serves the wire log, the
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
  press classifier's thresholds are **unchanged** — 20 ms debounce, 600 ms long press, 350 ms
  double window. The two new keys are ordinary keys: they debounce, classify and double exactly
  like the four existing ones.
- The `Button` enum gains two members and a count that stops being written as `4` in five places
  in `hal/impl/Buttons.cpp`. `enum class Button : uint8_t { Up, Down, Forward, Back, Ok, Cancel }`
  and a `kButtonCount = 6` are the shape; the `pressStartMs_`, `pendingPress_` and
  `firstReleaseMs_` arrays and the 32-bit masks all follow the count. Six bits of a 32-bit mask is
  not a change of mechanism, only of bound.
- `PanelTasks`'s `PressOrder` gains the two keys, because a press is reported in panel order and a
  chord key and a steal target are both found by scanning it.

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
| **Up+Down** | the page's edit/settings view, unchanged |

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

**Up+Down stays the settings gesture.** It is the one chord, one event on the first release,
reported on the up key, made by the panel task — unchanged. It does not move to Forward, because
Forward already means "enter" and giving it a second meaning is exactly the overloading this plan
is removing. No new chord is added: Forward+Back and the rest wait for a page that needs one.

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

While an external link drives there is no page, so presses are logged: `btn: ok short`. The format
is unchanged; the vocabulary grows `forward` and `cancel`. With part 3, the log token and the event
token are the same string with and without the `btn: ` prefix.

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

## Part 3 — the press event as a string

`buttons.take()` today answers an `event` record — a Berry class with `btn` and `press` fields —
because a routine reads a record with a dot and only the parser can declare an instance variable.
That buys a class, a compile-at-boot string in `kRecords`, and `beginRecord`/`setText` plumbing
for what is two short strings every consumer immediately compares to string literals anyway.

**`buttons.take()` answers one string, `"btn:press"`, or `nil` when the queue is empty:**

```berry
var ev = buttons.take()   # "ok:short", "up:chord", "back:long", or nil
```

- The two tokens are the same ones today: `up`, `down`, `forward`, `back`, `ok`, `cancel` and
  `short`, `long`, `double`, `chord`. Neither contains `:`, so `:` is a safe separator.
- The `event` class leaves `kRecords`; `reading`, `unit` and `bounds` stay, because those are
  genuinely multi-field values a routine reads by name.
- The native binding stops building a record and builds the string from `buttonName()` and
  `pressName()` with a `:` between. The queue it drains is unchanged — it still holds `Button` and
  `Press` — only the answer changes.
- The scene framework splits the token once and hands the page two strings, so the page's
  comparison stays as readable as it is now:

```berry
import string

def take_events()
  var acted = false
  while true
    var ev = buttons.take()
    if ev == nil
      break
    end
    var parts = string.split(ev, ':')
    if self.handle(parts[0], parts[1])
      acted = true
    end
  end
  return acted
end
```

`handle(btn, press)` becomes a two-argument method throughout — the scene base, the page
overrides, and the settings editor — and `ev.btn`/`ev.press` become the two arguments. The
declarative `on(btn, presses, fn)` registration and its `press_matches` logic are unchanged; only
the plumbing between `take()` and `handle` moves, and `scene.be` gains the `import string` the
split needs.

Why a string and not the record, stated plainly:

- **The record's only job is to carry two tokens.** A string is the same data with none of the
  class-declaration machinery, and a string is what the event log already prints with a colon
  where the space is.
- **No record class means one fewer entry in `kRecords`** and the whole `event` fill path in the
  binding goes away.
- **A record allocates an instance per press; a string interns the two tokens the interpreter
  already has.** The change is small either way, but it is in the direction of less surface.

The alternative — keep the record — is the no-op option, and it is only worth taking if a press
ever needs a third field. Nothing in sight needs one; a press is a key and a gesture, and the two
tokens are the whole of it.

## Open decisions

These are the places the requirement does not yet name the answer. Each has a recommendation; the
backlog that breaks this plan down should only be written after they are settled, because the
string, the steal target and the token names all appear in the gesture map above.

1. **`ok` vs `okay` as the token.** Recommended: keep `ok` — it is the name already in the log,
   the enum and every routine. The glass can read "okay".
2. **Where go-home lands.** Recommended: **Cancel while Back is held**, stolen in the panel task,
   exactly as Okay-while-Back is today. The alternative is a plain Hold Cancel, which is simpler to
   describe but reserves a hold on a key a page may want for its own cancel-and-hold.
3. **The reset chord.** Recommended: keep **Back+Okay 5 s**. Nothing about the new keys makes a
   better reset; it only has to not move.

## What moves together when it lands

The work is one change and lands as one, because the token names appear in all of these at once:

- `hal/Button.h` (enum, `buttonName`), `hal/impl/Buttons.h`/`.cpp` replaced by the shift-register
  reader, `Panel.h` pin block, the sketch's `PanelPins`, `PanelTasks.cpp` (order, chord, holds,
  steal).
- `routines/bindings/Buttons.cpp` (string answer), `routines/Bindings.cpp` (`kRecords` without
  `event`), `modules/scene.be` (split and two-argument `handle`), the five routine `.be` files and
  `modules/settings.be`.
- The specs when the work is done: [gestures.md](../specs/gestures.md) (six keys, the Back/Cancel
  split, the moved go-home), [bindings.md](../specs/ui/bindings.md) (`buttons.take()` answers a
  string) and [routine.md](../specs/ui/routine.md) (`handle(btn, press)`). The specs still describe
  the `Page`/`page.be` framework names that an in-flight rename has already moved to
  `Scene`/`scene.be`; that reconciliation happens with the same pass.
