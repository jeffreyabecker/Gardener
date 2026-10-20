# Components: the drawing primitives

Part of the routine-helpers plan — see [_index.md](./_index.md) for the whole effort. A readout
page composes these to draw its face; the components own the box math, the font choice and the
pressure formatting.

They live in `firmware/gardener/data/modules/components.be`.

## The box model — `Component`

Every component has the same geometry, so a routine positions any of them the same way:

- `x`, `y`, `w`, `h` — the component's outer box, in panel pixels, origin top left.
- `margin` — space reserved outside the border that the component never draws into.
- `border` — the border's thickness, drawn on the border box when a component has one.
- `padding` — space between the border and the content.

A component draws only inside its **content rect**:

```
cx = x + margin + border + padding
cy = y + margin + border + padding
cw = w - 2 * (margin + border + padding)
ch = h - 2 * (margin + border + padding)
```

The base provides `content()` for that rect and a `draw(...)` hook each component overrides with
its own arguments. Components are stateless across draws: the same config and the same args draw
the same thing. Where config alone determines a derived value — a face that fits a fixed string —
the component computes it once and caches it; where an argument changes per draw, it re-derives.

```berry
class Component
  var x, y, w, h
  var margin, border, padding

  def init()
    self.x = 0; self.y = 0; self.w = 0; self.h = 0
    self.margin = 0; self.border = 0; self.padding = 0
  end

  def content()
    return [self.x + self.margin + self.border + self.padding,
            self.y + self.margin + self.border + self.padding,
            self.w - 2 * (self.margin + self.border + self.padding),
            self.h - 2 * (self.margin + self.border + self.padding)]
  end

  def draw() end        # each component overrides with its own signature
end
```

The framework does not lay components out: a routine sets each component's `x`, `y`, `w`, `h`.
`margin` exists so a component can hold breathing room without its outer box changing, and so a
container can place siblings from their margins when one is worth adding — not yet.

## Fonts

Components take a family and a style and pick the largest face whose ink box fits the content
rect, falling back to the built-in 5x7 at sizes 3, 2, 1. `text_bounds(text, 1, face)` (and
`text_bounds(text, size)` for the built-in) is the measure.

| family | style | faces, largest first |
| --- | --- | --- |
| `sans` | `none` | `sans24`, `sans18`, `sans12`, `sans9`, `builtin` 3, 2, 1 |
| `sans` | `bold` | `sansbold24`, `sansbold18`, `sansbold12`, `sansbold9`, `builtin` 3, 2, 1 |
| `mono` | `none` | `mono24`, `mono18`, `mono12`, `mono9`, `builtin` 3, 2, 1 |
| `mono` | `bold` | `monobold24`, `monobold18`, `monobold12`, `monobold9`, `builtin` 3, 2, 1 |

The four styles are `none`, `bold`, `oblique` and `bold-oblique`; the four families are `sans`,
`mono`, `serif` and `lcd`. The firmware compiles only the faces above, so the rest resolve to the
nearest face rather than refuse: `serif` → `sans`, `lcd` → `mono`, `oblique` → `none`,
`bold-oblique` → `bold`. These fallbacks are documented here and logged nowhere; compiling real
serif, LCD and oblique faces is a firmware change this plan leaves alone.

Text is placed by `align` horizontally (`left`, `center`, `right`) and `v_align` vertically (`top`,
`center`, `bottom`), both default `center`. With a named face `y` is the baseline (the firmware's
own rule); with the built-in face `y` is the top. The component accounts for the difference.

## Components

Config is a set of fields with defaults, settable at construction or after.

### `FixedText`

- config: `value` (the text), `family`, `style`, `align`, `v_align`
- draw args: none

Draws `value` at the largest face that fits the content rect. The fit depends only on config, so
it is resolved once and cached; every `draw()` is the same call.

### `DynamicText`

- config: `value` (the default text), `family`, `style`, `align`, `v_align`
- draw args: `value` (the text to draw)

As `FixedText`, except the text arrives per draw: `draw(value)` re-fits each call (the face list
is cached; the chosen face is not). A `nil` argument falls back to the config `value`.

### `PressureDisplay`

- config: `show_units` (bool, default true), `family`, `style`, `align`, `v_align`
- draw args: `value` (a depth in kPa)

Reads `screen.unit()` itself each `draw()` — it is a plain read-only query, and reading it fresh
means a unit changed by the system page or a controller is picked up on the next visit with no
activation hook. Converts the kPa to the panel's unit (`value * per_kpa`, formatted to the unit's
`decimals`), appends the unit `suffix` when `show_units` is set, and draws the result at the
largest face that fits the content rect. The suffix rides the same baseline and is measured
together with the number, so the number is only ever as large as the whole line fits. A `nil`
value draws `--`.

### `ProgressBar`

- config: none (the box model is the config; `border` is the frame thickness)
- draw args: `value` (real in 0–100, clamped)

Draws the frame on the border box and a fill rectangle from the left of the content rect,
`cw * value / 100` wide. Monochrome, so the fill is the inverted half of the frame's outline —
no colour, no text.

### `ProgressBubbles`

- config: `max_count` (int), `bubble_style` (`rect`, `roundrect`, `circle`, `tri-up`, `tri-down`,
  `tri-left`, `tri-right`; default `circle`), `unfilled` (`outline` or `none`; default `outline`),
  `fill_direction` (`lr-tb`, `rl-tb`, `tb-lr`, `bt-lr`; default `lr-tb`)
- draw args: `count` (int, 0–`max_count`, clamped)

Draws `max_count` bubbles, filled up to `count`, in the content rect. The component computes the
bubble size and the horizontal/vertical spacing itself, wrapping onto further lines when one row
does not hold `max_count` — a large count fills the box top-to-bottom in a grid rather than
squeezing one row. `fill_direction` is the order the bubbles fill across the grid: `lr-tb` left to
right then top to bottom, `rl-tb` the reverse, `tb-lr` top to bottom then left to right, `bt-lr`
bottom to top. The first `count` are filled; the rest are drawn as `unfilled` says — `outline`
draws each as a hollow shape so the full count stays visible, `none` draws nothing for them.
`bubble_style` picks the shape, mapped to the screen primitives (`rect`/`roundrect`/`circle` to
`draw_*rect`/`draw_*roundrect`/`draw_*circle`, the triangles to `draw_*triangle`): filled for the
active bubbles, outline for the rest.

The layout degrades rather than overflowing: the gap between bubbles shrinks first, down to zero
(bubbles touching). If the bubbles still will not fit at a legible size, they collapse to solid
2×2 and then 1×1 squares with no outline and no shape — only the filled count is drawn, so the
progress reads as a dense block of solid squares.

The whole layout — bubble size, spacing, grid geometry and every bubble's position — follows from
config and the box alone, so the component computes it once and caches it; `draw(count)` then just
repaints the cached grid with the filled/unfilled split.

### `Scrollbar`

- config: `invert` (bool, default false)
- draw args: `index` (the first visible row), `count` (rows in the list), `visible` (rows the box
  shows)

Draws a vertical track the height of the content rect and a thumb showing the window's share and
position. Thumb height is `ch * visible / count`, pinned to a one-pixel minimum; thumb top is
`cy + (ch - thumb) * index / (count - visible)`, clamped so the thumb never leaves the track, and
filling it when `count <= visible`. Normally the track is the content rect's outline and the thumb
is filled; `invert` swaps the two — a solid bar with the window carved out.

### `Triangle`

- config: `direction` (`up`, `down`, `left`, `right`; default `up`), `style` (`empty` or `filled`;
  default `filled`)
- draw args: none

Draws an isosceles triangle inscribed in the content rect, apex toward `direction`, base on the
opposite edge. `style` `empty` draws the outline (`screen.draw_triangle`); `filled` fills it
(`screen.draw_filltriangle`).

## `Icon` — a glyph at a point

An icon is a lighter primitive than a `Component`: it has an intrinsic size and draws itself at a
point, with no box to fit into.

```berry
class Icon
  var w
  var h

  def init(w, h)
    self.w = w
    self.h = h
  end

  def draw(x, y) end    # draws within (x, y, x + w, y + h)
end
```

Concrete icons are subclasses that implement `draw(x, y)` with the low-level `screen.draw_*`
calls — an arrow is a filled `screen.draw_triangle`, a bullet a filled circle — so an icon is a
named glyph the way a face is a named font. An icon draws as-is: it is not fitted, centred or
inverted, because a glyph is the size it is.

## Composing a readout

A readout page holds its components as fields and feeds them state in `draw_page()` — there is no
separate chrome object. Component fields are page fields, so where a role shares a name with a
state field the component gets a distinct one (`target_value` below, because `target` is already
the page's kPa target).

```berry
# in static.be
class Static : Page
  var header
  var readout
  var target_label
  var target_value
  var status
  var hint

  def init()
    super(self).init()

    self.header = DynamicText()
    self.header.x = 0; self.header.y = 0; self.header.w = 116; self.header.h = 12

    self.readout = PressureDisplay()
    self.readout.x = 0; self.readout.y = 12; self.readout.w = 116; self.readout.h = 24
    self.readout.show_units = false

    self.target_label = FixedText()
    self.target_label.value = "TGT"
    self.target_label.x = 0; self.target_label.y = 40; self.target_label.w = 40; self.target_label.h = 8

    self.target_value = PressureDisplay()
    self.target_value.x = 40; self.target_value.y = 40; self.target_value.w = 76; self.target_value.h = 8
    self.target_value.show_units = true

    self.status = DynamicText()
    self.status.x = 0; self.status.y = 48; self.status.w = 116; self.status.h = 8

    self.hint = DynamicText()
    self.hint.x = 0; self.hint.y = 56; self.hint.w = 116; self.hint.h = 8
  end

  def draw_page()
    var s = vacuum.state()
    self.header.draw(self.phase_name())
    self.readout.draw(s.sensor_ok ? s.kpa : nil)
    self.target_label.draw()
    self.target_value.draw(self.target)
    self.status.draw(self.status_text())
    self.hint.draw(self.hint_text())
  end
end
```

A routine that wants something else writes its own `draw_page()`; the components are the
vocabulary, not the frame.

## Notes

- No clipping: nothing on the panel clips; a component's box is a contract it keeps by measuring,
  not by clipping. A component sized to overflow simply draws past its box, so a page sets boxes
  the content will fit.
- Components must not be constructed at a module's top level: `/modules` loads in no particular
  order. A page constructs its components in `init()`.
- The overlay owns its corners: the battery icon (top right), the state icon (`x` 64–78,
  `y` 0–14) and the connection icons (right edge from `x` 116) are drawn over every flush; a
  page must not size a component across them.
