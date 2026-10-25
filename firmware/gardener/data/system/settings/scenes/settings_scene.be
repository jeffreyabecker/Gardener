# settings_scene.be — SettingsScene: the list a routine's settings are edited in. It is the list and
# nothing else: the settings and the values are handed to it, the values keyed by setting key, and it
# pops them back on the way out — what was edited when OK kept it, and the set as it was opened when
# Back or Cancel refused it.

class SettingsScene : Scene
  var settings_cfgs
  var settings_values
  var opened_settings_values
  var message
  var list

  def init(settings_cfgs, settings_values, message)
    var box = ux.box()
    super(self).init(box[0], box[1])
    self.settings_cfgs = settings_cfgs
    # The set as it was handed over, apart from the one being edited, so Back and Cancel can hand it
    # back untouched. A map is a reference, so this has to be a copy of what the list shows.
    self.opened_settings_values = {}
    for cfg : settings_cfgs
      self.opened_settings_values[cfg.key] = settings_values[cfg.key]
    end
    self.settings_values = settings_values
    self.message = message
    self.list = List(SettingsView.list_rows(self.dim))
  end

  def activate()
    self.draw()
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
  # The heading is dropped when the block fits — a repair message is kept, as the first line, because
  # it is why the set was mended rather than a title. The heading and the rows are laid out on the
  # settings view's own rows, so the list and the value editors move together.
  def draw_page()
    var rows = self.rows()
    self.list.set(rows)
    if rows.size() > self.list.visible
      if self.message != nil
        screen.draw_text(self.pos.x, self.pos.y, 1, false, self.message)
      else
        screen.draw_text(self.pos.x, self.pos.y, 1, false, "SETTINGS")
      end
      self.list.draw(self.pos.x, self.pos.y + SettingsView.row + SettingsView.gap)
      return
    end

    var wide = 0
    if self.message != nil
      wide = screen.text_bounds(self.message, 1).w
    end
    for cfg : self.settings_cfgs
      var w = screen.text_bounds(cfg.label + "=" + self.value_text(cfg), 1).w
      if cfg.icon != nil
        w += cfg.icon.dim.w + 2
      end
      if w > wide
        wide = w
      end
    end

    var heading_h = 0
    if self.message != nil
      heading_h = SettingsView.row + SettingsView.gap
    end
    var block_h = rows.size() * SettingsView.row + heading_h
    var x = self.pos.x + (self.dim.w - wide) / 2
    var y = self.pos.y + (self.dim.h - block_h) / 2
    if self.message != nil
      var mw = screen.text_bounds(self.message, 1).w
      screen.draw_text(x + (wide - mw) / 2, y, 1, false, self.message)
      y += heading_h
    end
    self.list.draw(x, y)
  end

  def rows()
    var out = []
    for cfg : self.settings_cfgs
      out.push([cfg.icon, cfg.label + "=" + self.value_text(cfg)])
    end
    return out
  end

  # A setting whose key holds nothing draws an empty value rather than its default.
  def value_text(cfg)
    var value = self.settings_values[cfg.key]
    if value == nil
      return "--"
    end
    return cfg.text(value)
  end

  # A row whose setting has no editor - a build fact the device reports - opens nothing.
  def open_row()
    var cfg = self.settings_cfgs[self.list.selected]
    var scene = cfg.scene(is_nil(self.settings_values[cfg.key], cfg.default_value))
    if scene == nil
      return
    end
    ux.push(scene, nil, / v -> self.value_closed(cfg, v))
  end

  # A committed value replaces the one in the values the caller handed over. A cancel leaves the row
  # alone, and a commit takes the opening reason with it: it named the set that was opened.
  def value_closed(cfg, value)
    if value == cancelled
      self.draw()
      return
    end
    self.settings_values[cfg.key] = value
    self.message = nil
    self.draw()
  end
end