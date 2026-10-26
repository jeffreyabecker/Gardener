# settings_edit.be — SettingsEditScene: the list a routine's settings are edited in. It is the list
# and nothing else: the settings and the values are handed to it, the values keyed by setting key,
# and it pops them back on the way out — what was edited when OK kept it, and the set as it was
# opened when Back or Cancel refused it. A title, when it is handed one, is drawn above the rows. A
# row carries its own setting as its value, so the row showing is the setting Forward opens.

class SettingsEditScene : Scene
  var settings_cfgs
  var settings_values
  var opened_settings_values
  var title
  var list

  def init(settings_cfgs, settings_values, title)
    super(self).init()
    self.settings_cfgs = settings_cfgs
    # The set as it was handed over, apart from the one being edited, so Back and Cancel can hand it
    # back untouched. A map is a reference, so this has to be a copy of what the list shows.
    self.opened_settings_values = {}
    for cfg : settings_cfgs
      self.opened_settings_values[cfg.key] = settings_values[cfg.key]
    end
    self.settings_values = settings_values
    self.title = title
  end

  # The page's box decides how many rows the list window holds, so the list is built here rather
  # than in init, where the scene has no box yet.
  def to_box(x, y, w, h)
    super(self).to_box(x, y, w, h)
    self.list = List(SettingsView.window_rows(self.dim))
  end

  def activate()
    self.draw()
  end

  def handle(ev)
    if ev == "up:single"
      self.list.move(-1)
      return true
    end
    if ev == "down:single"
      self.list.move(1)
      return true
    end
    if ev == "forward:single"
      # A row whose setting has no editor - a build fact the device reports - opens nothing.
      var row = self.list.selected_row()
      if row == nil
        return true
      end
      var cfg = row.value
      var scene = cfg.scene(is_nil(self.settings_values[cfg.key], cfg.default_value))
      if scene != nil
        ux.push(scene, nil, / v -> self.value_closed(cfg, v))
      end
      return true
    end
    if ev == "ok:single"
      # OK keeps what was edited: the list hands the set it settled on back to the caller.
      ux.pop(self.settings_values)
      return true
    end
    if ev == "back:single" || ev == "cancel:single"
      # Back and Cancel keep nothing: the set as it was opened goes back untouched.
      ux.pop(self.opened_settings_values)
      return true
    end
    return false
  end

  # A short list centers a block the size of its rows; a long one fills the scene's box and scrolls.
  # A title, when there is one, is the block's first line and the scrolling list's heading, standing
  # in for the default "SETTINGS". The heading and the rows are laid out on the settings view's own
  # rows, so the list and the value editors move together.
  def draw_page()
    var rows = []
    for cfg : self.settings_cfgs
      # A setting whose key holds nothing draws an empty value rather than its default.
      var value = self.settings_values[cfg.key]
      rows.push(ListRow(cfg.icon,
                        cfg.label + "=" + (value == nil ? "--" : cfg.text(value)), cfg))
    end
    self.list.set(rows)

    # The widest of the heading and the rows: the width of the band the selected row inverts, and of
    # the block a short list is centred as.
    var wide = 0
    if self.title != nil
      wide = screen.text_bounds(self.title, 1).w
    end
    for row : rows
      var w = screen.text_bounds(row.title, 1).w
      if row.icon != nil
        w += row.icon.dim.w + 2
      end
      if w > wide
        wide = w
      end
    end

    if rows.size() > self.list.visible
      if self.title != nil
        screen.draw_text(self.pos.x, self.pos.y, 1, false, self.title)
      else
        screen.draw_text(self.pos.x, self.pos.y, 1, false, "SETTINGS")
      end
      self.list.to_box(self.pos.x, self.pos.y + SettingsView.row + SettingsView.gap, wide,
                       self.list.visible * SettingsView.row)
      self.list.draw()
      return
    end

    var heading_h = 0
    if self.title != nil
      heading_h = SettingsView.row + SettingsView.gap
    end
    var block_h = rows.size() * SettingsView.row + heading_h
    var x = self.pos.x + (self.dim.w - wide) / 2
    var y = self.pos.y + (self.dim.h - block_h) / 2
    if self.title != nil
      var mw = screen.text_bounds(self.title, 1).w
      screen.draw_text(x + (wide - mw) / 2, y, 1, false, self.title)
      y += heading_h
    end
    self.list.to_box(x, y, wide, rows.size() * SettingsView.row)
    self.list.draw()
  end

  # The value an editor handed back replaces the one in the values the caller gave. The title is not
  # part of the set, so it stays over the rows however many values change.
  def value_closed(cfg, value)
    self.settings_values[cfg.key] = value
    self.draw()
  end
end