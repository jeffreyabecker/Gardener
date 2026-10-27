# list.be — the selectable menu: a selected index into a list of ListRow components, walked with
# Up/Down, wrapped at the ends, drawn with one inverted row and a scroll window, feeding an optional
# Scrollbar.

# One row of a list: the glyph drawn at its left, the label drawn beside it, and the value the row
# stands for — a routine key, a setting, whatever the caller keeps. The row draws its own glyph and
# title; the value is the caller's and is never drawn. The list sets a row's box before each draw —
# the list's width by one row pitch tall — so the whole of that box is what the selection inverts.
# The glyph, the gap beside it and the title are cells of one layout, so the row works nothing out.
class ListRow : Component
  var icon, title, value
  var page

  def init(icon, title, value)
    super(self).init()
    self.icon = icon
    self.title = title == nil ? "" : str(title)
    self.value = value
    self.page = nil
  end

  def to_box(x, y, w, h)
    self.set_box(point(x, y), bounds(w, h))
    self.page = self.row()
    self.page.to_box(x, y, w, h)
  end

  def draw()
    self.page.draw()
    super(self).draw()
  end

  # The glyph's own measure, two pixels of paper and the rest of the row for the title.
  def row()
    var cells = []
    if self.icon != nil
      cells.push(AutoLayout(bounds(Icon.width, Icon.height), [self.icon]))
      cells.push(AutoLayout(bounds(2, 1), []))
    end
    cells.push(AutoLayout(bounds(nil, 1), [SysText(self.title)], alignment("left", "top")))
    return AutoLayout(bounds(nil, nil), cells)
  end
end

# Rows as ListRows, whatever a caller wrote: one of these, the [icon, title] pair the list took
# before rows were records, or a bare title with no glyph.
def list_rows(rows)
  var out = []
  for entry : rows
    if isinstance(entry, ListRow)
      out.push(entry)
    elif isinstance(entry, list)
      out.push(ListRow(entry[0], entry.size() > 1 ? entry[1] : nil, nil))
    else
      out.push(ListRow(nil, entry, nil))
    end
  end
  return out
end

# The menu itself: the rows it holds, the index showing and the window they are drawn in. It is a
# component, so it is placed with to_box() by the scene that holds it and drawn with draw() — the
# box's width is the band a row inverts, and its height only has to hold the visible rows.
class List : Component
  # One row of the built-in face: the pitch a row is drawn at, and the height of its band.
  static var row = 8

  var rows
  var selected
  var visible
  var scrollbar

  def init(visible)
    super(self).init()
    self.rows = []
    self.selected = 0
    self.visible = visible
    self.scrollbar = nil
  end

  def set(rows)
    self.rows = list_rows(rows)
    if self.selected >= self.rows.size()
      self.selected = 0
    end
  end

  # The row the selected index names, or nil when there is none. It is what a caller reads instead of
  # indexing the list beside it.
  def selected_row()
    if self.selected >= self.rows.size()
      return nil
    end
    return self.rows[self.selected]
  end

  def move(delta)
    var count = self.rows.size()
    if count == 0
      return false
    end
    self.selected = (self.selected + delta + count) % count
    return true
  end

  def first()
    if self.selected >= self.visible
      return self.selected - self.visible + 1
    end
    return 0
  end

  def to_box(x, y, w, h)
    self.set_box(point(x, y), bounds(w, h))
  end

  # Every visible row is given its box — the list's width, one row pitch tall — and drawn, the one
  # the index names inverted so the whole of its band reads as paper on ink.
  def draw()
    var start = self.first()
    var line = 0
    while line < self.visible && start + line < self.rows.size()
      var entry = self.rows[start + line]
      entry.to_box(self.pos.x, self.pos.y + line * List.row, self.dim.w, List.row)
      entry.inverted = start + line == self.selected
      entry.draw()
      line += 1
    end
    if self.scrollbar != nil
      self.scrollbar.index = start
      self.scrollbar.count = self.rows.size()
      self.scrollbar.visible = self.visible
      self.scrollbar.draw()
    end
    super(self).draw()
  end
end
