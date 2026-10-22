# registry.be — the registry of user routines: the list the browser draws, each routine's factory
# and the settings it offers, and the way into each routine's scene.
#
# A routine file's top level calls routines.register(name, icon, factory, settings) and the
# entry joins this Berry list; nothing native holds it. The factory is the routine's scene class,
# and the registry builds the scene when the routine is pushed, so a page starts from its own
# activate() every time and nothing a previous visit left behind survives; the scene that is
# showing is ux.top(). The settings are the list of setting records the settings editor walks, so
# the scene below no longer answers for them.
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

  def register(name, icon, factory, settings)
    # The name is also the file store reads and writes, so it has to be a plain word.
    if name == nil || string.find(name, "/") >= 0 || string.find(name, "\\") >= 0
      vacuum.log("registry: routine name must be a plain word")
      return false
    end
    if factory == nil
      vacuum.log("registry: routine must name its scene class")
      return false
    end
    self.entries.push([name, icon, factory, settings])
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

  # The settings a routine offers, as the list it registered; nil when the name is not registered.
  def settings(name)
    var e = self.entry(name)
    if e == nil
      return nil
    end
    return e[3]
  end

  def icon(name)
    var e = self.entry(name)
    if e == nil
      return nil
    end
    return e[1]
  end

  # The browser's rows: [icon, name] per routine, ready for a List.
  def rows()
    var out = []
    for e : self.entries
      out.push([e[1], e[0]])
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
    var factory = e[2]
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
