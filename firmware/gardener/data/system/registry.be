# registry.be — the registry of user routines: the list the browser draws, each routine's factory
# and the scene that edits its settings, and the way into each routine's scene.
#
# A routine file's top level calls routines.register(name, title, icon, factory, settings) and the entry
# joins this Berry list; nothing native holds it. The factory is the routine's scene class, and the
# registry builds the scene when the routine is pushed, so a page starts from its own activate()
# every time and nothing a previous visit left behind survives; the scene that is showing is
# ux.top(). The settings are the class of the scene that edits them, so the rules that bind two
# settings together live with the values rather than in the page.
#
# The browser reads names()/rows() and pushes a routine with push(name, callback[, settings]), which
# builds its scene and hands it to ux.push. The device's own scenes — the readout, system and
# browser — are not registered here: they are well-known scenes the system pushes directly.

import string

class RoutineRegistry
  var entries

  def init()
    self.entries = []
  end

  def register(name, title, icon, factory, settings)
    # The name is also the file store reads and writes, so it has to be a plain word. The title is
    # the label the browser draws; a missing one falls back to the name.
    if name == nil || string.find(name, "/") >= 0 || string.find(name, "\\") >= 0
      vacuum.log("registry: routine name must be a plain word")
      return false
    end
    if title == nil
      title = name
    end
    if factory == nil
      vacuum.log("registry: routine must name its scene class")
      return false
    end
    if settings != nil && type(settings) != "class"
      vacuum.log("registry: " + name + " settings must be a scene class")
      return false
    end
    self.entries.push([name, title, icon, factory, settings])
    return true
  end

  # The browser's list: every registered name.
  def names()
    var out = []
    for e : self.entries
      out.push(e[0])
    end
    return out
  end

  # The settings a routine edits, as the class it registered describes them; nil when it has none.
  def settings(name)
    var scene = self.settings_scene(name)
    if scene == nil
      return nil
    end
    return scene.settings()
  end

  # A settings scene for the routine, with no writer: enough to ask what it edits.
  def settings_scene(name)
    var e = self.entry(name)
    if e == nil || e[4] == nil
      return nil
    end
    var factory = e[4]
    return factory(name, nil)
  end

  # Push a routine's settings scene, with the callback that writes a settled value and the one the
  # stack runs when it closes. False when the routine registered no settings.
  def push_settings(name, on_change, closed)
    var e = self.entry(name)
    if e == nil || e[4] == nil
      return false
    end
    var factory = e[4]
    return ux.push(factory(name, on_change), nil, closed)
  end

  def icon(name)
    var e = self.entry(name)
    if e == nil
      return nil
    end
    return e[2]
  end

  # The label the browser draws for a routine; the name is the identity.
  def title(name)
    var e = self.entry(name)
    if e == nil
      return nil
    end
    return e[1]
  end

  # The browser's rows: [icon, title] per routine, ready for a List.
  def rows()
    var out = []
    for e : self.entries
      out.push([e[2], e[1]])
    end
    return out
  end

  # Push a registered routine's scene, built through its factory. The callback, when there is one,
  # receives the value the scene returns when it pops; `settings`, when there is one, is that
  # activation's own values, handed to activate().
  def push(name, callback, settings)
    var e = self.entry(name)
    if e == nil
      return false
    end
    var factory = e[3]
    return ux.push(factory(), settings, callback)
  end

  def entry(name)
    for e : self.entries
      if e[0] == name
        return e
      end
    end
    return nil
  end
end

routines = RoutineRegistry()
