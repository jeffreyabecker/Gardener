# Nullable settings in the settings editor

The shared settings editor — [settings.be](../../firmware/gardener/data/modules/settings.be) —
edits a routine's `describe()` list and reads and writes each value through `store`. Today a value
can never be `nil`: [read_value](../../firmware/gardener/data/modules/settings.be) always supplies a
typed fallback (`0`, `0.0`, `false`, `values[0]`), and `store.get` returns that fallback for a key
that was never set. A setting that should mean "no value" cannot say so, and a `nil` that ever
reached the editor would crash it — `pressure_text` multiplies it, `nudge` adds to it.

This plan gives a setting a real null: shown, edited and cleared, with the routine that owns the
key deciding what null means for its own behaviour.

## What null means

A nullable setting's key is **absent** — or holds an empty `key=` line — in the routine's INI file.
The editor reads that as `nil`, shows `--` for it, and writes an empty value back when the user
clears it. Null is a state of the *value*, not of the `setting`: a nullable setting still has its
kind, its bounds and its fallback shape; it is the stored value that may be nothing.

The editor's job ends at showing null and handing it back. The routine that owns the key decides
what null does (see [What the routine sees](#what-the-routine-sees)).

## The `setting` record gains `nullable`

`setting` is the record a routine's `describe()` builds. It lives in
[Bindings.cpp](../../libraries/VacuumCore/src/app/routines/Bindings.cpp) (`kRecords`) and is mirrored
by [harness.be](../../.github/skills/berry/harness.be) and the table in
[SKILL.md](../../.github/skills/berry/SKILL.md). Add a `nullable` field to all three, and a ninth
constructor argument to `init`:

```berry
setting(key, label, kind, step, min, max, values, nullable)
```

Berry has no default arguments, so every existing call site passes `false`: the seven in
[ramped.be](../../firmware/gardener/data/routines/ramped.be), the five in
[interval.be](../../firmware/gardener/data/routines/interval.be), and the example in the spec. A
routine marks one nullable by passing `true`:

```berry
setting("rest_pressure", "Rest", "pressure", 1, nil, nil, nil, true)
```

## Reading — nil when unset

`read_value` detects absence with the empty-string sentinel, then falls through to today's typed
read:

```berry
if setting.nullable && store.get(self.name, setting.key, "") == ""
  return nil
end
```

`store.get` with an empty-string fallback returns `""` both for a missing key and for a present
`key=` line, so "never set" and "cleared" read the same way. The typed read that follows runs only
when the key holds something, so a present nullable setting costs one extra INI read per `open()` —
not per frame — and reuses the existing typed read-back and clamping untouched.

Rejected: a native `store.has(name, key)`. It is the cleaner read, but it adds an operation to the
store bus, `Routines` and the `store` binding for a presence test the empty-string fallback already
answers. The sentinel lives in one file and touches no native surface.

## Showing null

`value_text` returns `--` before the kind branches, so the list row reads `Rest=--`. `edit_text`
does the same when `edit_value` is nil. One marker for every kind; the hint line is unchanged, and
`--` is what tells the user the position is "nothing".

## Editing — the starting value and the null step

A nullable setting starts at a real value, and null sits one step below it.

`begin_edit` substitutes a kind-appropriate start when the stored value is nil, via a small
`default_edit_value`:

| kind | start |
| --- | --- |
| `int`, `real` | `min`, or `0` / `0.0` when `min` is nil |
| `pressure` | `vacuum.clamp(0.0)` — the shallowest legal target, what an unset pressure clamps to today |
| `bool` | `false` |
| `choice` | `values[0]` |
| `duration` | `min`, or `0` — the minutes stage |

`nudge` then treats nil as one position below the kind's lowest value, in both directions:

* **`int`, `real`, `duration`** — Up from nil enters the lowest value; Down at the lowest value
  enters nil; Down at nil stays nil. The top still clamps as today.
* **`pressure`** — the lowest value is `vacuum.clamp(0.0)`; the same two rules apply, so the editor
  can still only produce a target the machine accepts.
* **`bool`** — a nullable bool has three positions: nil, OFF, ON, with nil below OFF. Up from nil
  goes OFF, Down at OFF goes nil; otherwise OFF and ON toggle. A non-nullable bool keeps today's
  direction-blind toggle.
* **`choice`** — nil sits below `values[0]`: Down at `values[0]` goes nil, Up from nil goes
  `values[0]`; all other cycling is unchanged, including Up wrapping from the last value to the
  first.

A `duration` cleared from the minutes stage keeps its two stages: OK advances to the seconds stage
still nil, and the second OK saves the clear.

## Committing and clearing

`commit` writes `edit_value` through `store.set` as today, except a nil is written as `""` first.
`str(nil)` is `"nil"`, so passing nil straight through would persist the literal text `nil` and the
setting would read back as that string. `store.set(name, key, "")` writes a `key=` line, which
[Ini](../../libraries/VacuumCore/src/app/storage/Ini.cpp) parses and writes back faithfully, so the
clear persists and reads as null after a reboot.

## What the routine sees

The routine keeps reading with its own fallback. A cleared numeric reads as `0` / `0.0` (an empty
text converts to zero), a cleared bool as `false`, and a cleared choice as `""` — which is not one
of its values. A routine that wants to distinguish "nothing" from "a real value" reads the raw text
and branches, the way [interval.be](../../firmware/gardener/data/routines/interval.be) already treats
an unrecognised choice as its default:

```berry
if store.get(key, "") == "" ...
```

The editor does not change what a routine's fallback means; a nullable setting is a UI decision
that a routine opts in to by declaring `nullable` and reading the key with that branch.

## Files touched

| File | Change |
| --- | --- |
| [Bindings.cpp](../../libraries/VacuumCore/src/app/routines/Bindings.cpp) | `setting` record: `nullable` field and 8th `init` argument |
| [harness.be](../../.github/skills/berry/harness.be) | mirror the `setting` field list |
| [settings.be](../../firmware/gardener/data/modules/settings.be) | `read_value`, `value_text`, `edit_text`, `begin_edit`, `nudge`, `next_choice`, `commit` |
| [ramped.be](../../firmware/gardener/data/routines/ramped.be), [interval.be](../../firmware/gardener/data/routines/interval.be) | pass `false` as the 8th argument at every `setting(...)` call |
| [routine.md](../../docs/specs/ui/routine.md) | the `setting` signature, field table and editor paragraph |
| [bindings.md](../../docs/specs/ui/bindings.md) | note that a cleared key reads back as `""` |
| [SKILL.md](../../.github/skills/berry/SKILL.md) | `setting` row lists `nullable` |

## Spec changes

When implemented, the `describe()` section of [routine.md](../../docs/specs/ui/routine.md) changes:

* the signature and field table gain `nullable` — a boolean, `false` unless the routine says the
  setting may be nothing;
* the editor paragraph says a nullable setting reads `nil` when its key is unset, shows `--`, and
  stepping Down at the lowest value clears it back to null; and
* the line "An unset pressure reads as `0.0`" stays true only for a non-nullable pressure, while a
  nullable one reads as `nil`.

[bindings.md](../../docs/specs/ui/bindings.md) adds one sentence to `store`: a key holding an empty
value reads back as `""`, which is how the editor keeps a cleared setting.

## Verification

* `python .github/skills/berry/check.py` — compiles, loads and drives the image, including the
  editor's draw/OK cycle. The nil paths are only exercised if some routine's `describe()` returns a
  nullable setting, so the change lands with one adopter (the natural first candidate is a `rest`
  setting, where null can mean "no rest hold"); without one, `check.py` proves nothing new about the
  null branch.
* `powershell -File scripts/arduino.ps1 -Task build` — the `kRecords` edit is C++.
