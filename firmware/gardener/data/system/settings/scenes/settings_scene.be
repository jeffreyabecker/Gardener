# settings_scene.be — SettingsScene: the list a routine's settings are edited in. It is the list and
# nothing else: the settings and the values are handed to it, the values keyed by setting key, and it
# pops those values back with whatever was committed, so whoever opened it writes them.

class SettingsScene : Scene
  var settings_cfgs
  var settings_values
  var message
  var list

  def init(settings_cfgs, settings_values, message)
    super(self).init()
    self.settings_cfgs = settings_cfgs
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
    if ev == "back:single" || ev == "cancel:single"
      # The list hands its values back so the caller can apply the rule and write them; nothing has
      # been written while it was open.
      ux.pop(self.settings_values)
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