# registry.be — the registry of user routines: the list the browser draws, each routine's factory,
# the class that declares its settings, and the way into each routine's scene.
#
# A routine file's top level calls routines.register(routine_key, title, icon, factory, settings) and
# a RoutineRegistration joins this Berry list; nothing native holds it. The key is the routine's
# identity: the registration it is found by and the document its values are kept in. The factory is
# the routine's scene class, and the
# registry builds the scene when the routine is pushed, so a page starts from its own init() every
# time and nothing a previous visit left behind survives; the scene that is showing is ux.top(). The
# settings are a class the routine writes itself, holding the values it edits and any rule that
# binds two of them, so those live with the routine rather than in the page; it is asked through its
# class and is never built, because the list it describes is opened by whoever wants to edit it.
#
# The browser reads keys()/rows() and pushes a routine with push(routine_key, callback[, settings]).
# The settings, when the caller has none, are built by load(): every value the routine registers,
# read from its file, held to its kind and repaired by the routine's own rule. So a scene is built
# from one clean map and initialization is the only activation. The device's own scenes — the
# readout, system and browser — are not registered here: they are well-known scenes the system
# pushes directly.

import string

# One routine's registration: the key it is known by, the label and icon the browser draws, the
# class the registry builds its scene from, and the class that declares its settings.
class RoutineRegistration
  var routine_key, title, icon, factory, settings

  def init(routine_key, title, icon, factory, settings)
    self.routine_key = routine_key
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

  def register(routine_key, title, icon, factory, settings)
    # The key is also the file store reads and writes, so it has to be a plain word. The title is
    # the label the browser draws; a missing one falls back to the key.
    if routine_key == nil || string.find(routine_key, "/") >= 0 || string.find(routine_key, "\\") >= 0
      vacuum.log("registry: routine key must be a plain word")
      return false
    end
    if title == nil
      title = routine_key
    end
    if factory == nil
      vacuum.log("registry: routine must name its scene class")
      return false
    end
    if settings != nil && type(settings) != "class"
      vacuum.log("registry: " + routine_key + " settings must be a class")
      return false
    end
    self.entries.push(RoutineRegistration(routine_key, title, icon, factory, settings))
    return true
  end

  # The browser's list: every registered key, in registration order.
  def keys()
    var out = []
    for e : self.entries
      out.push(e.routine_key)
    end
    return out
  end

  # The settings a routine can edit, as the class it registered declares them, in the order the list
  # shows them; nil when it has none. A setting whose kind cannot work is left out, with one line
  # saying why, because no value scene could edit it.
  def settings(routine_key)
    var e = self.entry(routine_key)
    if e == nil || e.settings == nil
      return nil
    end
    var out = []
    for item : e.settings.settings()
      if item.problem != nil
        vacuum.log("settings: " + routine_key + " " + item.key + ": " + item.problem)
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
  def repair(routine_key, settings_cfgs, values)
    var reason = self.problem(routine_key, settings_cfgs, values)
    if reason == nil
      return nil
    end
    vacuum.log("settings: " + routine_key + ": " + reason)
    var i = 0
    while i < settings_cfgs.size()
      if values[i] != settings_cfgs[i].default_value
        values[i] = settings_cfgs[i].default_value
        if self.problem(routine_key, settings_cfgs, values) == nil
          return reason
        end
      end
      i += 1
    end
    vacuum.log("settings: " + routine_key + ": " + reason + " even at the defaults")
    return reason
  end

  # The rule's verdict on the values held beside their settings, or nil when it accepts them.
  def problem(routine_key, settings_cfgs, values)
    var e = self.entry(routine_key)
    if e == nil || e.settings == nil
      return nil
    end
    var out = {}
    var i = 0
    while i < settings_cfgs.size()
      out[settings_cfgs[i].key] = values[i]
      i += 1
    end
    return e.settings.problem(out)
  end

  def icon(routine_key)
    var e = self.entry(routine_key)
    if e == nil
      return nil
    end
    return e.icon
  end

  # The label the browser draws for a routine; the key is the identity.
  def title(routine_key)
    var e = self.entry(routine_key)
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

  # The settings a routine begins with, as one clean map: every setting it registers, read from its
  # own file and held to its kind, then repaired by the routine's rule so the set is one a run
  # accepts. A routine with no settings begins with an empty map. This is the only place the
  # file-or-nothing rule is applied; a scene is handed the answer and never reads a document.
  def load(routine_key)
    var settings_cfgs = self.settings(routine_key)
    var out = {}
    if settings_cfgs == nil
      return out
    end
    var values = []
    for item : settings_cfgs
      values.push(item.read(routine_key))
    end
    self.repair(routine_key, settings_cfgs, values)
    var i = 0
    while i < settings_cfgs.size()
      out[settings_cfgs[i].key] = values[i]
      i += 1
    end
    return out
  end

  # A settled set written back to the routine's file: the routine's rule is applied first, so what
  # lands is a set a run accepts, and a value the list holds nothing for is written as nothing, which
  # reads back as nil. `settings_cfgs` may be nil, and then the settings the routine declares are
  # looked up here, so a caller that never opened a list has none to hand over. False, writing
  # nothing, when the routine declares no settings.
  def save_settings(routine_key, values, settings_cfgs)
    if settings_cfgs == nil
      settings_cfgs = self.settings(routine_key)
    end
    if settings_cfgs == nil
      return false
    end
    self.repair(routine_key, settings_cfgs, values)
    var i = 0
    while i < settings_cfgs.size()
      store.set(routine_key, settings_cfgs[i].key, values[i] == nil ? "" : values[i])
      i += 1
    end
    return true
  end

  # Push a registered routine's scene, built through its factory. The callback, when there is one,
  # receives the value the scene returns when it pops. `settings` is the map the scene's init() is
  # given; a nil one is the routine's own file, loaded and made clean here.
  def push(routine_key, callback, settings)
    var e = self.entry(routine_key)
    if e == nil
      return false
    end
    if settings == nil
      settings = self.load(routine_key)
    end
    return ux.push(e.factory(settings), nil, callback)
  end

  def entry(routine_key)
    for e : self.entries
      if e.routine_key == routine_key
        return e
      end
    end
    return nil
  end
end

routines = RoutineRegistry()
