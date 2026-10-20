# Chrome: the readout display

Part of the routine-helpers plan — see [_index.md](./_index.md) for the whole effort. `Chrome`
sits on the `Page` base class the index specifies: the page owns `number(kpa)` and `value(kpa)`,
and a chrome reads them back through its `owner`.

## Problem

`default` and `static` draw the same readout by hand: a header line, the big number at (0,12)
size 3, a `TGT …` line, and (`static`) status and hint lines. The two pages copy the layout and
each format their own values, so the geometry, the `--` for a missing reading, and the target-line
wording are three places that have to agree by eye.

## Decision

A **subclassable `Chrome` display class** in `firmware/gardener/data/modules/chrome.be` wholly
owns the readout frame. A routine subclasses it and overrides one hook per line, so it keeps
fine-grained control over its own chrome; the base owns the fixed geometry and delegates
formatting to the owning page (`self.owner.number()` / `self.owner.value()`). Each readout page's
`draw_page()` collapses to `self.chrome.draw(vacuum.state())`. A routine that wants a different
layout overrides `draw(s)` outright — the whole frame is one method.

`browser` and `system` are list/edit layouts and do not use it; `settings.be` draws into the
page's buffer and is untouched.

## The base class

```berry
class Chrome
  var owner

  def init(owner)                 # the page; reads go through it
    self.owner = owner
  end

  def draw(s)                     # s is vacuum.state(); the fixed readout frame
    screen.draw_text(0, 0, 1, false, self.header(s))
    var c = self.current(s)
    if c == nil
      screen.draw_text(0, 12, 3, false, "--")
    else
      screen.draw_text(0, 12, 3, false, self.owner.number(c))
    end
    var t = self.target(s)
    if t == nil
      screen.draw_text(0, 40, 1, false, "TGT --")
    else
      screen.draw_text(0, 40, 1, false, "TGT " + self.owner.value(t))
    end
    var st = self.status(s)
    if st != nil
      screen.draw_text(0, 48, 1, false, st)
    end
    var h = self.hint(s)
    if h != nil
      screen.draw_text(0, 56, 1, false, h)
    end
  end

  # Hooks a routine overrides (nil hides a line; nil current draws "--").
  def header(s)  return ""  end
  def current(s) return nil end
  def target(s)  return nil end
  def status(s)  return nil end
  def hint(s)    return nil end
end
```

## The shipped subclasses

- `default.be` — `ReadoutChrome : Chrome`: header `s.state`, current `s.sensor_ok ? s.kpa : nil`,
  target `s.target > 0.0 ? s.target : nil`, no status or hint.
- `static.be` — `StaticChrome : Chrome`, the full example:

```berry
# in static.be
class StaticChrome : Chrome
  def header(s)  return self.owner.phase_name() end
  def current(s) return s.sensor_ok ? s.kpa : nil end
  def target(s)  return self.owner.target end
  def status(s)  return self.owner.status_text() end
  def hint(s)    return self.owner.hint_text() end
end

class Static : Page
  var chrome
  def init()
    super(self).init()
    self.chrome = StaticChrome(self)
  end
  def draw_page()
    self.chrome.draw(vacuum.state())
  end
end
```

## Cosmetic reconciliations

Two deliberate, cosmetic reconciliations the chrome imposes: `default`'s target line moves from
y=44 to y=40 (`static` already uses 40), and an unset target reads `TGT --` without the unit
suffix (`default` previously printed `TGT -- kPa`). Preserving either exactly would push per-line
positions back into the page, which is what the chrome exists to absorb.

## Notes

- `Chrome` is standalone — it holds only its `owner` — and must not be constructed at a module's
  top level: `/modules` loads in no particular order. A page constructs its chrome in `init()`.
- A chrome's owner is the `Page`-derived routine, so the hooks read `phase_name()`,
  `status_text()` and any other page state through `self.owner`.

## Validation

Shared with the whole plan: `python scripts/arduino.py --task build` does not parse `.be` (the
files ship as data and are interpreted at boot), and the repo has no host-side Berry validator.
The chrome changes are verified by careful review; on-device boot logs (`vm: … compile/load`) are
the runtime check and need hardware.
