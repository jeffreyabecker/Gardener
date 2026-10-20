# List: the selectable menu

Part of the routine-helpers plan — see [_index.md](./_index.md) for the whole effort. `List` is
the stateful sibling of the stateless [components](./components.md): it owns the selection, the
wrap and the highlight a menu shares, and composes a `Scrollbar` for the scroll window.

It lives in `firmware/gardener/data/modules/list.be`.

## What it owns

Three pieces walk a selected index through a list of rows — `browser` (routine names),
`settings` (`label=value` rows) and `system` (three fixed rows). Each hand-rolls the same three
rules, and `List` makes them one. A row is an optional `Icon` and a label, so a page can put a
glyph beside the text:

- **wrap** — Up/Down moves `selected` by ±1 modulo the count, so the ends wrap.
- **window** — `first()` is the first visible row; the list scrolls only as far as the selection
  needs, and never scrolls a list that fits (`count <= visible`).
- **highlight** — the selected row is drawn inverted (paper on ink), the one row drawn that way.

```berry
class List
  var rows                       # [icon, label]; icon nil when none
  var selected
  var visible
  var scrollbar

  def init(visible)
    self.rows = []
    self.selected = 0
    self.visible = visible
    self.scrollbar = nil
  end

  def set(rows)                  # the owner's rows; clamps the selection
    self.rows = rows
    if self.selected >= self.rows.size()
      self.selected = 0
    end
  end

  def move(delta)                # ±1, wrapping; false when empty
    var count = self.rows.size()
    if count == 0
      return false
    end
    self.selected = (self.selected + delta + count) % count
    return true
  end

  def first()                    # first visible row
    if self.selected >= self.visible
      return self.selected - self.visible + 1
    end
    return 0
  end

  def draw(x, y)                 # the visible window; selected row inverted
    var start = self.first()
    var row = 0
    while row < self.visible && start + row < self.rows.size()
      var icon = self.rows[start + row][0]
      var label = self.rows[start + row][1]
      if icon != nil
        icon.draw(x, y + row * 8)
        screen.draw_text(x + icon.w + 2, y + row * 8, 1, start + row == self.selected, label)
      else
        screen.draw_text(x, y + row * 8, 1, start + row == self.selected, label)
      end
      row += 1
    end
    if self.scrollbar != nil
      self.scrollbar.draw(start, self.rows.size(), self.visible)
    end
  end
end
```

Rows are drawn directly with `screen.draw_text` rather than through text components: the highlight
is the one thing the stateless components deliberately do not model, and it belongs here. The icon
draws as-is — not inverted or resized — and the label shifts right by `icon.w + 2`.

## How each page uses it

- **browser** — `set(routines.names())` on activate, one `[nil, name]` row per routine (an icon
  per routine arrives when routines have icons); Up/Down become `move(±1)`; OK and Back stay the
  page's. Positions a `Scrollbar` beside the list when the list can scroll.
- **settings** — `set([[kind_icon, label + "=" + value] …])` on open and after a commit, an icon
  per setting kind; Up/Down become `move(±1)`; OK opens the editor, Back closes. The two-stage
  edit face is unchanged.
- **system** — `set([[nil, "UNIT=" …], [nil, "TGT=" …], [nil, "BT=" …]])` each draw;
  `visible = 3` so there is no scrolling and the wrap is the same rule it hand-wrote for three
  rows. The inline edit face is unchanged.

## Notes

- `List` is stateful, so it is not a `Component`; it is the widget that composes them (the
  `Scrollbar`). `browser` and `system` are list layouts, which is why they never needed the
  readout components — they needed this.
- Constructed by the page or editor in `init()`/`open()`, never at a module's top level.
