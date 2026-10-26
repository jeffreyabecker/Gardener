# registry.be — the registry of user routines: the list the browser draws, each routine's factory,
# the settings declaration it registers, and the way into each routine's scene.
#
# A routine file's top level calls routines.register(routine_key, title, icon, factory, settings) and
# a RoutineRegistration joins this Berry dict, keyed by the routine's key; nothing native holds it.
# The key is the routine's identity: the registration it is found by and the document its values are
# kept in. The factory is the routine's scene class, and the registry builds the scene when the
# routine is pushed, so a page starts from its own init() every time and nothing a previous visit
# left behind survives; the scene that is showing is ux.top(). The settings are an instance the
# routine writes itself, built when the file registers, holding the values it edits in its
# settings_cfgs property and any rule that binds two of them in its problem() method, so those live
# with the routine rather than in the page.
#
# Registering a routine also reads its document for alternates: every named [section] is an alias,
# a second key that shares the routine's factory, settings declaration and icon but carries its own
# title — the section's routine_title, or the alias key — and its own settings set, which lives in
# that section. The routine's own set stays in the global section, so the root config is untouched
# by an alias.
#
# The browser reads rows() and pushes a routine with push(routine_key, callback, activation).
# A launch hands the factory a RoutineActivation: the routine key and one clean settings map, built
# by load_settings() — every value the routine registers, read from its file, held to its kind and
# repaired by the routine's own rule. So a scene is built from one clean map and initialization is
# the only activation. The device's own scenes — the readout, system and browser — are not
# registered here: they are well-known scenes the system pushes directly.

import string

# One routine's registration: the key it is known by, the label and icon the browser draws, the
# class the registry builds its scene from, and the settings instance that declares them.
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

# One alternate set a routine's document declares: the alias key the set is found under — the
# section name — and the title the browser row shows, read from that section's routine_title or the
# key itself. The parent the alias resolves to is in the registry's aliases dict.
class RoutineAlias
  var routine_key, title

  def init(routine_key, title)
    self.routine_key = routine_key
    self.title = title
  end
end

class RoutineRegistry
  var entries
  var order
  var aliases
  var alias_order

  def init()
    self.entries = {}
    self.order = []
    self.aliases = {}
    self.alias_order = []
  end

  def register(routine_key, title, icon, factory, settings)
    # The key is also the name of the document its values live in, so it has to be a plain word.
    # The title is the label the browser draws; a missing one falls back to the key.
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
    if settings != nil && type(settings) != "instance"
      vacuum.log("registry: " + routine_key + " settings must be an instance")
      return false
    end
    if self.entries.find(routine_key) == nil
      self.order.push(routine_key)
    else
      self.drop_aliases(routine_key)
    end
    self.entries[routine_key] = RoutineRegistration(routine_key, title, icon, factory, settings)
    # Every named section of the routine's document is an alternate set under its own key.
    var document = device.config("routines/" + routine_key + ".ini")
    for name : document.section_keys()
      if name == ""
        continue
      end
      if self.aliases.find(name) != nil || self.entries.find(name) != nil
        vacuum.log("registry: " + routine_key + " alias " + name + " already taken")
        continue
      end
      self.aliases[name] = routine_key
      var stored_title = document.get(name, "routine_title", "")
      self.alias_order.push(RoutineAlias(name, stored_title == "" ? name : stored_title))
    end
    return true
  end

  # Every alias this routine already owns, dropped before a re-registration rescans its document, so
  # a section that no longer exists does not leave an alias behind.
  def drop_aliases(routine_key)
    var aliases = {}
    for name : self.aliases.keys()
      if self.aliases[name] != routine_key
        aliases[name] = self.aliases[name]
      end
    end
    self.aliases = aliases
    var order = []
    for alias : self.alias_order
      if self.aliases.find(alias.routine_key) == nil
        continue
      end
      order.push(alias)
    end
    self.alias_order = order
  end

  # Every registered key, in registration order, each alias following its routine.
  def keys()
    var out = []
    for routine_key : self.order
      out.push(routine_key)
      for alias : self.alias_order
        if self.aliases.find(alias.routine_key) == routine_key
          out.push(alias.routine_key)
        end
      end
    end
    return out
  end

  # The settings a routine can edit, as the instance it registered declares them in settings_cfgs,
  # in the order the list shows them; nil when it has none. A setting whose kind cannot work is left
  # out, with one line saying why, because no value scene could edit it.
  def settings_cfgs(routine_key)
    var e = self.entry(routine_key)
    if e == nil || e.settings == nil
      return nil
    end
    var out = []
    for item : e.settings.settings_cfgs
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
  # value under its key. The reason that was applied is returned for the list to draw, or nil when
  # the set was already valid. Nothing here touches a file: the caller decides what to write.
  def repair(routine_key, settings_cfgs, settings_values)
    var reason = self.problem(routine_key, settings_cfgs, settings_values)
    if reason == nil
      return nil
    end
    vacuum.log("settings: " + routine_key + ": " + reason)
    for cfg : settings_cfgs
      if settings_values[cfg.key] != cfg.default_value
        settings_values[cfg.key] = cfg.default_value
        if self.problem(routine_key, settings_cfgs, settings_values) == nil
          return reason
        end
      end
    end
    vacuum.log("settings: " + routine_key + ": " + reason + " even at the defaults")
    return reason
  end

  # The rule's verdict on the values held beside their settings, or nil when it accepts them.
  def problem(routine_key, settings_cfgs, settings_values)
    var e = self.entry(routine_key)
    if e == nil || e.settings == nil
      return nil
    end
    var out = {}
    for cfg : settings_cfgs
      out[cfg.key] = settings_values[cfg.key]
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

  # The label the browser draws for a routine; the key is the identity. An alias carries its own
  # title, read from its section, so a row shows that rather than the parent's.
  def title(routine_key)
    for alias : self.alias_order
      if alias.routine_key == routine_key
        return alias.title
      end
    end
    var e = self.entry(routine_key)
    if e == nil
      return nil
    end
    return e.title
  end

  # The browser's rows: one ListRow per routine and per alias, carrying the routine key it stands
  # for. An alias row shares its parent's icon and carries the alias key as its value.
  def rows()
    var out = []
    for routine_key : self.order
      var e = self.entries[routine_key]
      out.push(ListRow(e.icon, e.title, routine_key))
      for alias : self.alias_order
        if self.aliases.find(alias.routine_key) == routine_key
          out.push(ListRow(e.icon, alias.title, alias.routine_key))
        end
      end
    end
    return out
  end

  # The settings a routine begins with, as one clean map: every setting it registers, read from its
  # own document and held to its kind, then repaired by the routine's rule so the set is one a run
  # accepts. The document is opened once, so reading every key costs one file read rather than one
  # per key. The root set reads the global section; an alias reads its own named section of the
  # parent's document, and also carries its routine_title on the map. A routine with no settings
  # begins with an empty map. This is the only place the file-or-nothing rule is applied; a scene
  # is handed the answer and never reads a document.
  def load_settings(routine_key)
    var parent = self.aliases.find(routine_key)
    var section = ""
    if parent != nil
      section = routine_key
    else
      parent = routine_key
    end
    var settings_cfgs = self.settings_cfgs(routine_key)
    var settings_values = {}
    if settings_cfgs == nil
      return settings_values
    end
    var document = device.config("routines/" + parent + ".ini")
    for cfg : settings_cfgs
      var raw = document.get(section, cfg.key, "")
      if raw == ""
        settings_values[cfg.key] = cfg.value(nil)
      else
        settings_values[cfg.key] = cfg.value(cfg.parse(raw))
      end
    end
    if section != ""
      var stored_title = document.get(section, "routine_title", "")
      if stored_title != ""
        settings_values["routine_title"] = stored_title
      end
    end
    self.repair(routine_key, settings_cfgs, settings_values)
    return settings_values
  end

  # A settled set written back to the routine's document: the routine's rule is applied first, so
  # what lands is a set a run accepts, and a value the map holds nothing for is written as nothing,
  # which reads back as nil. Every key is put into one document and the document is written once, so
  # a settled set costs one write and a set that changes nothing costs none. The root set writes the
  # global section; an alias writes its own named section of the parent's document, leaving the root
  # config alone. `settings_cfgs` may be nil, and then the settings the routine declares are looked
  # up here, so a caller that never opened a list has none to hand over. False, writing nothing,
  # when the routine declares no settings.
  def save_settings(routine_key, settings_values, settings_cfgs)
    var parent = self.aliases.find(routine_key)
    var section = ""
    if parent != nil
      section = routine_key
    else
      parent = routine_key
    end
    if settings_cfgs == nil
      settings_cfgs = self.settings_cfgs(routine_key)
    end
    if settings_cfgs == nil
      return false
    end
    self.repair(routine_key, settings_cfgs, settings_values)
    var document = device.config("routines/" + parent + ".ini")
    for cfg : settings_cfgs
      var value = settings_values[cfg.key]
      document.set(section, cfg.key, value == nil ? "" : value)
    end
    return document.save()
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

  # The registration a key names: an alias resolves to its parent, so every lookup goes through
  # here and serves both.
  def entry(routine_key)
    var parent = self.aliases.find(routine_key)
    if parent == nil
      parent = routine_key
    end
    return self.entries.find(parent)
  end
end

routines = RoutineRegistry()
