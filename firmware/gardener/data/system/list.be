# list.be — the selectable menu: a selected index into a list of ListRow records, walked with
# Up/Down, wrapped at the ends, drawn with one highlighted row and a scroll window, feeding an
# optional Scrollbar.

# One row of a list: the glyph drawn at its left, the label drawn beside it, and the value the row
# stands for — a routine key, a setting, whatever the caller keeps. The list draws the icon and the
# title; the value is the caller's.
class ListRow
  var icon, title, value

  def init(icon, title, value)
    self.icon = icon
    self.title = title == nil ? "" : str(title)
    self.value = value
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

class List
  var rows
  var selected
  var visible
  var scrollbar

  def init(visible)
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

  def draw(x, y)
    var start = self.first()
    var line = 0
    while line < self.visible && start + line < self.rows.size()
      var entry = self.rows[start + line]
      if entry.icon != nil
        entry.icon.pos.x = x
        entry.icon.pos.y = y + line * 8
        entry.icon.draw()
        screen.draw_text(x + entry.icon.dim.w + 2, y + line * 8, 1, start + line == self.selected,
                         entry.title)
      else
        screen.draw_text(x, y + line * 8, 1, start + line == self.selected, entry.title)
      end
      line += 1
    end
    if self.scrollbar != nil
      self.scrollbar.index = start
      self.scrollbar.count = self.rows.size()
      self.scrollbar.visible = self.visible
      self.scrollbar.draw()
    end
  end
end
