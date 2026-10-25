# list.be — the selectable menu: a selected index into a list of [icon, label] rows, walked with
# Up/Down, wrapped at the ends, drawn with one highlighted row and a scroll window, feeding an
# optional Scrollbar.

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
    self.rows = rows
    if self.selected >= self.rows.size()
      self.selected = 0
    end
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
    var row = 0
    while row < self.visible && start + row < self.rows.size()
      var icon = self.rows[start + row][0]
      var label = self.rows[start + row][1]
      if icon != nil
        icon.pos.x = x
        icon.pos.y = y + row * 8
        icon.draw()
        screen.draw_text(x + icon.dim.w + 2, y + row * 8, 1, start + row == self.selected, label)
      else
        screen.draw_text(x, y + row * 8, 1, start + row == self.selected, label)
      end
      row += 1
    end
    if self.scrollbar != nil
      self.scrollbar.index = start
      self.scrollbar.count = self.rows.size()
      self.scrollbar.visible = self.visible
      self.scrollbar.draw()
    end
  end
end
