# browser.be — the routine browser: every registration as a row, the way into one of them, and the
# settings list its Forward opens. It registers with nothing; it is the well-known scene the readout
# pushes, and browserScene is that instance.
#
# It loads before ui.be, which names browserScene in the readout's gestures, so the instance exists
# by the time the readout is compiled.
#
# Choosing a row launches the routine: the registry builds its scene from one clean settings map, so
# the launch is the whole of the routine's initialization. A saved routine would launch the same way
# with its own map instead of the one its file makes.

class BrowserScene : Scene
  var names
  var list
  var scrollbar

  def init()
    super(self).init()
    self.names = []
    self.list = List(6)
    self.scrollbar = Scrollbar(Position(110, 2), Size(2, 48))
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
    self.names = routines.names()
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
    var count = self.names.size()
    if count == 0
      return false
    end
    return routines.push(self.names[self.list.selected], nil)
  end

  # Open the selected routine's settings. The browser reads the values and writes what the list
  # settles on, the way the routine's own page does; the list only edits them.
  def open_selected()
    var count = self.names.size()
    if count == 0
      return false
    end
    var name = self.names[self.list.selected]
    return edit_settings(name, / items, v -> self.settings_closed(name, items, v))
  end

  def settings_closed(name, items, values)
    save_settings(name, items, values)
    self.draw()
  end

  def leave()
    ux.pop(nil)
    return true
  end

  def draw_page()
    if self.names.size() == 0
      return
    end
    self.list.draw(0, 2)
  end
end

browserScene = BrowserScene()
