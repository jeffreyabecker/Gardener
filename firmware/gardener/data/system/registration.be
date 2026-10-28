# registration.be — Registration: the record a routine registers, which is also the routine's
# settings declaration. It holds the key the routine is found by, the label and icon the browser
# draws, the class the registry builds its scene from, the settings it edits, and the alternates its
# document named.
#
# The key is the routine's identity: what the registration is found by and what its document is
# called, so its set lives in routines/<routine_key>.ini — the global section for the routine's own
# set, a named section for each alternate. The declaration also carries the rule that binds two
# settings in problem(values), and it reads, repairs and writes its own values, so a page is handed
# a set a run accepts and nothing else opens a document.
#
# /system/order names this file before registry.be, which names both classes, and the routine files
# that extend Registration load after the whole system does.

# One alternate set a routine's document declares: the alias key the set is found under — the
# section name — and the title the browser row shows, read from that section's routine_title or the
# key itself. The routine it resolves to is the registration whose aliases list holds it.
class RoutineAlias
  var routine_key, title

  def init(routine_key, title)
    self.routine_key = routine_key
    self.title = title
  end
end

class Registration
  var routine_key, title, icon, factory, settings_cfgs, aliases

  # The settings are the declaration alone; `aliases` is filled by the registry, which scans the
  # routine's document when it registers. A kind that cannot work is left out here, with one line
  # saying why, because no value scene could edit it.
  def init(routine_key, title, icon, factory, settings_cfgs)
    self.routine_key = routine_key
    self.title = title
    self.icon = icon
    self.factory = factory
    self.aliases = []
    self.settings_cfgs = []
    for cfg : settings_cfgs
      if cfg.problem != nil
        device.log("settings: " + routine_key + " " + cfg.key + ": " + cfg.problem)
      else
        self.settings_cfgs.push(cfg)
      end
    end
  end

  # The rule that binds the settings to one another, or nil when every set is one a run can use.
  def problem(settings_values)
    return nil
  end

  # The values the rule accepts: while the rule refuses the set, the settings that differ from their
  # default are put back to them in the order the list shows them, each replacing the value under
  # its key. The reason that was applied is answered for the caller to draw, or nil when the set was
  # already valid. Nothing here touches a file: the caller decides what to write.
  def repair(section, settings_values)
    var label = section == "" ? self.routine_key : section
    var reason = self.problem(settings_values)
    if reason == nil
      return nil
    end
    device.log("settings: " + label + ": " + reason)
    for cfg : self.settings_cfgs
      if settings_values[cfg.key] != cfg.default_value
        settings_values[cfg.key] = cfg.default_value
        if self.problem(settings_values) == nil
          return reason
        end
      end
    end
    device.log("settings: " + label + ": " + reason + " even at the defaults")
    return reason
  end

  # One clean map: every setting the registration declares, read from the section it names of the
  # routine's document, held to its kind, then repaired by the routine's rule so the set is one a
  # run accepts. The document is opened once, so reading every key costs one file read rather than
  # one per key. The root set reads the global section; an alternate reads its own named section of
  # the parent's document and also carries its routine_title on the map. A registration that
  # declares nothing begins with an empty map. This is the only place the file-or-nothing rule is
  # applied; a scene is handed the answer and never reads a document.
  def load_settings(section)
    var settings_values = {}
    if self.settings_cfgs.size() == 0
      return settings_values
    end
    var document = device.config("routines/" + self.routine_key + ".ini")
    for cfg : self.settings_cfgs
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
    self.repair(section, settings_values)
    return settings_values
  end

  # A settled set written back to the routine's document: the rule is applied first, so what lands
  # is a set a run accepts, and a value the map holds nothing for is written as nothing, which reads
  # back as nil. Every key is put into one document and the document is written once, so a settled
  # set costs one write and a set that changes nothing costs none. False, writing nothing, when the
  # registration declares no settings.
  def save_settings(section, settings_values)
    if self.settings_cfgs.size() == 0
      return false
    end
    self.repair(section, settings_values)
    var document = device.config("routines/" + self.routine_key + ".ini")
    for cfg : self.settings_cfgs
      var value = settings_values[cfg.key]
      document.set(section, cfg.key, value == nil ? "" : value)
    end
    return document.save()
  end
end
