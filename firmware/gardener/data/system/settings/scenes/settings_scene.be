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
    super(self).init()
    self.settings_cfgs = settings_cfgs
    # The set as it was handed over, apart from the one being edited, so Back and Cancel can hand it
    # back untouched. A map is a reference, so this has to be a copy of what the list shows.
    self.opened_settings_values = {}
    for cfg : settings_cfgs
      self.opened_settings_values[cfg.key] = settings_values[cfg.key]
    end
    self.settings_values = settings_values
    self.message = message
    self.list = List(5)
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