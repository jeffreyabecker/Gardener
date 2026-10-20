# browser.be — the list of routines, and the way into each one's settings.
#
# The registry is the whole of its data and set_active() the whole of its action. The list is a
# List widget, and a routine that exposes describe() gets a second face through the shared editor
# in /modules/settings.be.

import string

class Browser : Page
  var names
  var list
  var scrollbar

  def init()
    super(self).init()
    self.names = []
    self.list = List(6)
    self.scrollbar = Scrollbar()
    self.scrollbar.x = 110; self.scrollbar.y = 16; self.scrollbar.w = 2; self.scrollbar.h = 48
    self.list.scrollbar = self.scrollbar

    self.on("up", nil, / -> self.step(-1))
    self.on("down", nil, / -> self.step(1))
    self.on("ok", "double", / -> self.open_selected())
    self.on("ok", nil, / -> self.choose())
    self.on("back", nil, / -> routines.set_active("default"))
  end

  def activate()
    # Read again every time the page appears: the registry is fixed after boot, but a page that
    # draws a list should not trust the list it drew last time either.
    self.names = routines.names()
    self.list.set(self.rows())
    self.editor = nil
    self.draw()
  end

  def rows()
    var out = []
    for name : self.names
      out.push([nil, name])
    end
    return out
  end

  def step(delta)
    return self.list.move(delta)
  end

  def choose()
    var count = self.names.size()
    if count == 0
      return false
    end
    routines.set_active(self.names[self.list.selected])
    return true
  end

  def open_selected()
    var count = self.names.size()
    if count == 0
      return false
    end
    return self.open_settings(self.names[self.list.selected])
  end

  def draw_page()
    if self.names.size() == 0
      return
    end
    self.list.draw(0, 16)
  end
end

routines.register("browser", "Routines", Browser())
return Browser
