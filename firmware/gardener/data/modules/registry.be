# registry.be — the registry of user routines: the list the browser draws and the way into each
# routine's scene.
#
# A routine file's top level calls routines.register(name, title, scene, hidden, icon) and the entry
# joins this Berry list; nothing native holds it. The browser reads names()/rows() and pushes a
# routine's scene with push(name, callback), which looks the scene up and hands it to ux.push. The
# device's own scenes — the readout, system and browser — are not registered here: they are
# well-known scenes the system pushes directly.

import string

class RoutineRegistry
  var entries

  def init()
    self.entries = []
  end

  def register(name, title, scene, hidden, icon)
    # The name is also the file store reads and writes, so it has to be a plain word.
    if name == nil || string.find(name, "/") >= 0 || string.find(name, "\\") >= 0
      vacuum.log("registry: routine name must be a plain word")
      return false
    end
    if hidden == nil
      hidden = false
    end
    self.entries.push([name, title, scene, hidden, icon])
    return true
  end

  # The browser's list: hidden routines are left out, so this is the routines a person can pick.
  def names()
    var out = []
    for e : self.entries
      if !e[3]
        out.push(e[0])
      end
    end
    return out
  end

  # Every name, hidden or not: what a check walks to make sure every registration is accounted for.
  def all()
    var out = []
    for e : self.entries
      out.push(e[0])
    end
    return out
  end

  def hidden(name)
    var e = self.entry(name)
    return e != nil && e[3]
  end

  def get(name)
    var e = self.entry(name)
    if e == nil
      return nil
    end
    return e[2]
  end

  def icon(name)
    var e = self.entry(name)
    if e == nil
      return nil
    end
    return e[4]
  end

  # The browser's rows: [icon, name] per pickable routine, ready for a List.
  def rows()
    var out = []
    for e : self.entries
      if !e[3]
        out.push([e[4], e[0]])
      end
    end
    return out
  end

  # Push a registered routine's scene. The callback, when there is one, receives the value the
  # scene returns when it pops.
  def push(name, callback)
    var scene = self.get(name)
    if scene == nil
      return false
    end
    return ux.push(scene, nil, callback)
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
