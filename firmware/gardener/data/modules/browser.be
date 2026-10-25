# browser.be — the routine browser: every registration as a row, the way into one of them, and the
# settings list its Forward opens. It registers with nothing; it is the well-known scene the readout
# pushes, and browserScene is that instance.
#
# It loads before ui.be, which names browserScene in the readout's gestures, so the instance exists
# by the time the readout is compiled.
#
# Choosing a row launches the routine: the registry builds its scene from one clean settings map, so
# the launch is the whole of the routine's initialization. A row carries its routine's key as its
# value, so the list is the only structure the browser keeps.

class BrowserScene : Scene
  var list
  var scrollbar

  def init()
    super(self).init(Position.top_left(), Size.page())
    self.list = List(6)
    self.scrollbar = Scrollbar(Position(self.pos.x + 110, self.pos.y + 2),
                               Size(2, self.dim.h - 4))
    self.list.scrollbar = self.scrollbar

    self.on("up:single", / -> self.step(-1))
    self.on("down:single", / -> self.step(1))
    self.on("ok:single", / -> self.choose())
    self.on("forward:single", / -> self.open_selected())
    self.on("back:single", / -> self.leave())
    self.on("cancel:single", / -> self.leave())
  end

  def activate()
    # A routine may only start from a released chamber, so opening the browser vents whatever the
    # readout was running: the launch it leads to is the one place a routine is allowed to begin.
    vacuum.stop()
    # Read again every time the scene appears: the registry is fixed after boot, but a scene that
    # draws a list should not trust the list it drew last time either.
    self.list.set(self.rows())
    self.draw()
  end

  def rows()
    return routines.rows()
  end

  def step(delta)
    return self.list.move(delta)
  end

  # The row's routine launched as its file describes it: the registry reads the file into one clean
  # settings map and builds the scene from it.
  def choose()
    var row = self.list.selected_row()
    if row == nil
      return false
    end
    var routine_key = row.value
    var settings_cfgs = routines.settings_cfgs(routine_key)
    if settings_cfgs == nil || settings_cfgs.size() == 0
      vacuum.log("settings: " + routine_key + " has none")
      return false
    end
    var settings_values = routines.load_settings(routine_key)
    return routines.push(routine_key, nil, settings_values)
  end

  # Open the selected routine's settings. The browser has no state of its own to show, so the values
  # are the routine's document: read through the declaration, mended when the routine's rule refuses
  # them, and written back as the list settles. The list itself only edits them.
  def open_selected()
    var row = self.list.selected_row()
    if row == nil
      return false
    end
    var routine_key = row.value
    var settings_cfgs = routines.settings_cfgs(routine_key)
    if settings_cfgs == nil || settings_cfgs.size() == 0
      vacuum.log("settings: " + routine_key + " has none")
      return false
    end
    var settings_values = routines.load_settings(routine_key)
    ux.push(SettingsEditScene(settings_cfgs, settings_values, routines.title(routine_key)), nil,
            / v -> self.settings_closed(routine_key, v))
    return true
  end

  # The set the list settled on is written and then launched with, so a Forward that edits a routine
  # leads into the routine rather than back to the browser's rows.
  def settings_closed(routine_key, settings_values)
    routines.save_settings(routine_key, settings_values)
    routines.push(routine_key, nil, settings_values)
    self.draw()
  end

  def leave()
    ux.pop(nil)
    return true
  end

  def draw_page()
    if self.list.rows.size() == 0
      return
    end
    self.list.draw(self.pos.x, self.pos.y + 2)
  end
end

browserScene = BrowserScene()
