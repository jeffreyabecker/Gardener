# registry.be — the registry of user routines: the list the browser draws, each routine's factory,
# the class that declares its settings, and the way into each routine's scene.
#
# A routine file's top level calls routines.register(name, title, icon, factory, settings) and a
# RoutineRegistration joins this Berry list; nothing native holds it. The factory is the routine's
# scene class, and the
# registry builds the scene when the routine is pushed, so a page starts from its own activate()
# every time and nothing a previous visit left behind survives; the scene that is showing is
# ux.top(). The settings are a class the routine writes itself, holding the values it edits and any
# rule that binds two of them, so those live with the routine rather than in the page; it is asked
# through its class and is never built, because the list it describes is opened by whoever wants to
# edit it.
#
# The browser reads names()/rows() and pushes a routine with push(name, callback[, settings]), which
# builds its scene and hands it to ux.push. The device's own scenes — the readout, system and
# browser — are not registered here: they are well-known scenes the system pushes directly.

import string

# One routine's registration: the name it is known by, the label and icon the browser draws, the
# class the registry builds its scene from, and the class that declares its settings.
class RoutineRegistration
  var name, title, icon, factory, settings

  def init(name, title, icon, factory, settings)
    self.name = name
    self.title = title
    self.icon = icon
    self.factory = factory
    self.settings = settings
  end
end

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
      vacuum.log("registry: " + name + " settings must be a class")
      return false
    end
    self.entries.push(RoutineRegistration(name, title, icon, factory, settings))
    return true
  end

  # The browser's list: every registered name.
  def names()
    var out = []
    for e : self.entries
      out.push(e.name)
    end
    return out
  end

  # The settings a routine can edit, as the class it registered declares them, in the order the list
  # shows them; nil when it has none. A setting whose kind cannot work is left out, with one line
  # saying why, because no value scene could edit it.
  def settings(name)
    var e = self.entry(name)
    if e == nil || e.settings == nil
      return nil
    end
    var out = []
    for item : e.settings.settings()
      if item.problem != nil
        vacuum.log("settings: " + name + " " + item.key + ": " + item.problem)
      else
        out.push(item)
      end
    end
    return out
  end

  # The values the routine's rule accepts: while the rule refuses the set, the settings that differ
  # from their default are put back to them in the order the list shows them, each replacing the
  # value in `values`. The reason that was applied is returned for the list to draw, or nil when the
  # set was already valid. Nothing here touches a file: the caller decides what to write.
  def repair(name, items, values)
    var reason = self.problem(name, items, values)
    if reason == nil
      return nil
    end
    vacuum.log("settings: " + name + ": " + reason)
    var i = 0
    while i < items.size()
      if values[i] != items[i].default_value
        values[i] = items[i].default_value
        if self.problem(name, items, values) == nil
          return reason
        end
      end
      i += 1
    end
    vacuum.log("settings: " + name + ": " + reason + " even at the defaults")
    return reason
  end

  # The rule's verdict on the values held beside their settings, or nil when it accepts them.
  def problem(name, items, values)
    var e = self.entry(name)
    if e == nil || e.settings == nil
      return nil
    end
    var out = {}
    var i = 0
    while i < items.size()
      out[items[i].key] = values[i]
      i += 1
    end
    return e.settings.problem(out)
  end

  def icon(name)
    var e = self.entry(name)
    if e == nil
      return nil
    end
    return e.icon
  end

  # The label the browser draws for a routine; the name is the identity.
  def title(name)
    var e = self.entry(name)
    if e == nil
      return nil
    end
    return e.title
  end

  # The browser's rows: [icon, title] per routine, ready for a List.
  def rows()
    var out = []
    for e : self.entries
      out.push([e.icon, e.title])
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
    return ux.push(e.factory(), settings, callback)
  end

  def entry(name)
    for e : self.entries
      if e.name == name
        return e
      end
    end
    return nil
  end
end

routines = RoutineRegistry()
