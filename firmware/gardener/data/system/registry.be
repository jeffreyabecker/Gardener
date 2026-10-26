# registry.be — the registry of user routines: the list the browser draws, each routine's factory,
# the settings declaration it registers, and the way into each routine's scene.
#
# A routine file's top level builds a Registration and calls routines.register(registration); the
# registry keeps them in one Berry list, in the order the files register, and nothing native holds
# it. The key is the routine's identity: what the registration is found by and what the document its
# values are kept in is named. The factory is the routine's scene class, and the registry builds the
# scene when the routine is pushed, so a page starts from its own init() every time and nothing a
# previous visit left behind survives; the scene that is showing is ux.top(). The settings are the
# registration's own declaration — the values it edits in settings_cfgs and any rule that binds two
# of them in its problem() — and the registration reads, repairs and writes them, so those live with
# the routine rather than in the page.
#
# Registering a routine also reads its document for alternates: every named [section] is an alias,
# a second key that shares the routine's factory, settings declaration and icon but carries its own
# title — the section's routine_title, or the alias key — and its own settings set, which lives in
# that section. The routine's own set stays in the global section, so the root config is untouched
# by an alias.
#
# The browser reads rows() and pushes a routine with push(routine_key, callback, activation).
# A launch hands the factory a RoutineActivation: the routine key and one clean settings map, built
# by load_settings() — every value the registration declares, read from its file, held to its kind
# and repaired by the routine's own rule. So a scene is built from one clean map and initialization
# is the only activation. The device's own scenes — the home scene, system and browser — are not
# registered here: they are well-known scenes the system pushes directly.

import string

class RoutineRegistry
  var registrations

  def init()
    self.registrations = []
  end

  def register(registration)
    if !isinstance(registration, Registration)
      vacuum.log("registry: register takes a Registration")
      return false
    end
    # The key is also the name of the document its values live in, so it has to be a plain word.
    # The title is the label the browser draws; a missing one falls back to the key.
    var routine_key = registration.routine_key
    if routine_key == nil || string.find(routine_key, "/") >= 0 || string.find(routine_key, "\\") >= 0
      vacuum.log("registry: routine key must be a plain word")
      return false
    end
    if registration.title == nil
      registration.title = routine_key
    end
    if registration.factory == nil
      vacuum.log("registry: routine must name its scene class")
      return false
    end
    # A key already registered keeps its place, so the browser's order is the order the files
    # registered in; a re-registration replaces the record it had, and its alternates go with it.
    var index = self.index_of(routine_key)
    if index < 0
      self.registrations.push(registration)
    else
      self.registrations[index] = registration
    end
    registration.aliases = []
    # Every named section of the routine's document is an alternate set under its own key.
    var document = device.config("routines/" + routine_key + ".ini")
    for name : document.section_keys()
      if name == ""
        continue
      end
      if self.entry(name) != nil
        vacuum.log("registry: " + routine_key + " alias " + name + " already taken")
        continue
      end
      var stored_title = document.get(name, "routine_title", "")
      registration.aliases.push(RoutineAlias(name, stored_title == "" ? name : stored_title))
    end
    return true
  end

  # Every registered key, in registration order, each alias following its routine.
  def keys()
    var out = []
    for e : self.registrations
      out.push(e.routine_key)
      for alias : e.aliases
        out.push(alias.routine_key)
      end
    end
    return out
  end

  # The settings a routine can edit, as its registration declares them, in the order the list shows
  # them; nil when the key is not registered. A kind that cannot work was left out when the
  # declaration was made, with one line saying why, because no value scene could edit it.
  def settings_cfgs(routine_key)
    var e = self.entry(routine_key)
    if e == nil
      return nil
    end
    return e.settings_cfgs
  end

  def icon(routine_key)
    var e = self.entry(routine_key)
    if e == nil
      return nil
    end
    return e.icon
  end

  # The label the browser draws for a routine; the key is the identity. An alias carries its own
  # title, read from its section, so a row shows that rather than the parent's.
  def title(routine_key)
    var e = self.entry(routine_key)
    if e == nil
      return nil
    end
    for alias : e.aliases
      if alias.routine_key == routine_key
        return alias.title
      end
    end
    return e.title
  end

  # The browser's rows: one ListRow per routine and per alias, carrying the routine key it stands
  # for. An alias row shares its parent's icon and carries the alias key as its value.
  def rows()
    var out = []
    for e : self.registrations
      out.push(ListRow(e.icon, e.title, e.routine_key))
      for alias : e.aliases
        out.push(ListRow(e.icon, alias.title, alias.routine_key))
      end
    end
    return out
  end

  # The settings a routine begins with, as one clean map: the registration reads its own document,
  # holds every value to its kind and repairs the set, so a scene is handed a set a run accepts. The
  # root set reads the global section; an alias reads its own named section of the parent's document
  # and also carries its routine_title on the map. An unregistered key begins with an empty map.
  def load_settings(routine_key)
    var e = self.entry(routine_key)
    if e == nil
      return {}
    end
    return e.load_settings(self.section(routine_key, e))
  end

  # A settled set written back through the registration, which applies the routine's rule first so
  # what lands is a set a run accepts. False, writing nothing, when the key is not registered or the
  # routine declares no settings.
  def save_settings(routine_key, settings_values)
    var e = self.entry(routine_key)
    if e == nil
      return false
    end
    return e.save_settings(self.section(routine_key, e), settings_values)
  end

  # Push a registered routine's scene, built through its factory from the activation the caller
  # loaded: the routine key and the clean settings map the scene's init() is given. The callback,
  # when there is one, receives the value the scene returns when it pops.
  def push(routine_key, callback, activation)
    var e = self.entry(routine_key)
    if e == nil
      return false
    end
    return ux.push(e.factory(activation), nil, callback)
  end

  # The registration a key names: an alternate resolves to its parent, so every lookup goes through
  # here and serves both. An alternate is looked for before a routine's own record, so a key that is
  # both still answers the alternate's routine.
  def entry(routine_key)
    for e : self.registrations
      for alias : e.aliases
        if alias.routine_key == routine_key
          return e
        end
      end
    end
    return self.registration(routine_key)
  end

  # The section a key's set lives in: the global section of the routine's own document for the
  # routine's key, and the key itself for an alternate, whose set is that named section.
  def section(routine_key, registration)
    return registration.routine_key == routine_key ? "" : routine_key
  end

  # The record a routine's own key names, or nil. An alternate is not looked for here, so a key
  # another routine's document named is not mistaken for one.
  def registration(routine_key)
    var i = self.index_of(routine_key)
    return i < 0 ? nil : self.registrations[i]
  end

  # Where a routine's own record sits in the list, or -1 when the key is not registered.
  def index_of(routine_key)
    var i = 0
    while i < self.registrations.size()
      if self.registrations[i].routine_key == routine_key
        return i
      end
      i += 1
    end
    return -1
  end
end

routines = RoutineRegistry()
