# settings_scene.be — SettingsScene: the list a routine's settings are edited in. It is the list and
# nothing else: the settings, the values and the reason a set was repaired are handed to it, and it
# pops the values it was given, so whoever opens it reads them from the file and writes them back.

class SettingsScene : Scene
  var items
  var values
  var message
  var list

  def init(items, values, message)
    super(self).init()
    self.items = items
    self.values = values
    self.message = message
    self.list = List(5)
  end

  def activate()
    self.draw()
  end

  def index_of(item)
    var i = 0
    while i < self.items.size()
      if self.items[i].key == item.key
        return i
      end
      i += 1
    end
    return -1
  end

  def handle_page(ev)
    if ev == "up:single"
      self.list.move(-1)
      return true
    end
    if ev == "down:single"
      self.list.move(1)
      return true
    end
    if ev == "forward:single"
      self.open_row()
      return true
    end
    if ev == "back:single"
      # The list hands its values back so the caller can apply the rule and write them; nothing has
      # been written while it was open.
      ux.pop(self.values)
      return true
    end
    return false
  end

  # The title line carries the reason the set was repaired when the list opened, because the rows
  # fill everything between it and the overlay's band.
  def draw_page()
    if self.message != nil
      screen.draw_text(0, 0, 1, false, self.message)
    else
      screen.draw_text(0, 0, 1, false, "SETTINGS")
    end
    self.list.set(self.rows())
    self.list.draw(0, 8)
  end

  def rows()
    var out = []
    var i = 0
    while i < self.items.size()
      var item = self.items[i]
      out.push([item.icon, item.label + "=" + self.value_text(i)])
      i += 1
    end
    return out
  end

  def value_text(index)
    var value = self.values[index]
    if value == nil
      return "--"
    end
    return self.items[index].text(value)
  end

  def open_row()
    var item = self.items[self.list.selected]
    var value = is_nil(self.values[self.list.selected], item.default_value)
    ux.push(item.scene(value), nil, / v -> self.value_closed(item, v))
  end

  # A committed value replaces the one in the list the caller handed over. A cancel leaves the row
  # alone, and a commit takes the opening reason with it: it named the set that was opened.
  def value_closed(item, value)
    if value == cancelled
      self.draw()
      return
    end
    var index = self.index_of(item)
    if index >= 0
      self.values[index] = value
      self.message = nil
    end
    self.draw()
  end
end

