# Chrome: the readout composition

Part of the routine-helpers plan — see [_index.md](./_index.md) for the whole effort. `Chrome`
sits on the `Page` base class the index specifies: the page keeps its lifecycle, and the chrome
owns every pixel the page draws. It builds its face from the drawing primitives in
[components.md](./components.md).

It lives in `firmware/gardener/data/modules/chrome.be`; the components live beside it in
`components.be`.

## Problem

`default` and `static` draw the same readout by hand — a header line, the big number, a `TGT …`
line, and (`static`) status and hint lines — and each page formats its own values, so the
geometry, the `--` for a missing reading, and the pressure conversion are three places that have
to agree by eye.

## Decision

A `Chrome` is a routine's readout face: a subclass that builds its components in `init()` and
maps state onto them in `draw(s)`. The page's `draw_page()` stays a single call,
`self.chrome.draw(vacuum.state())`. A routine that wants something else overrides `draw(s)`
outright — the components are the vocabulary, not the frame.

`browser` and `system` are list/edit layouts and do not use it; `settings.be` draws into the
page's buffer and is untouched.

## The base class

```berry
class Chrome
  var owner

  def init(owner)                 # the page; reads go through it
    self.owner = owner
  end

  def draw(s) end                 # the readout face; each routine overrides
end
```

## The shipped example

```berry
# in static.be
class StaticChrome : Chrome
  var pressure
  var header
  var target
  var status
  var hint

  def init(owner)
    super(self).init(owner)

    self.pressure = PressureDisplay()
    self.pressure.x = 0; self.pressure.y = 12; self.pressure.w = 116; self.pressure.h = 24
    self.pressure.show_units = false

    self.header = DynamicText()
    self.header.x = 0; self.header.y = 0; self.header.w = 116; self.header.h = 12

    self.target = DynamicText()
    self.target.y = 40

    self.status = DynamicText()
    self.status.y = 48

    self.hint = DynamicText()
    self.hint.y = 56
  end

  def draw(s)
    self.header.draw(self.owner.phase_name())
    self.pressure.draw(s.sensor_ok ? s.kpa : nil)
    self.target.draw("TGT " + self.owner.value(self.owner.target))
    self.status.draw(self.owner.status_text())
    self.hint.draw(self.owner.hint_text())
  end
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

## Boundaries

- **The overlay owns its corners.** The battery icon (top right), the state icon (`x` 64–78,
  `y` 0–14) and the connection icons (right edge from `x` 116) are drawn over every flush; a
  chrome must not size a component across them. Components keep their own boxes by measuring —
  nothing on the panel clips.

## Notes

- `Chrome` must not be constructed at a module's top level: `/modules` loads in no particular
  order. A page constructs its chrome in `init()`.
- A chrome's owner is the `Page`-derived routine, so `draw(s)` reads `phase_name()`,
  `status_text()` and any other page state through `self.owner`.

## Validation

Shared with the whole plan — see [_index.md](./_index.md).
