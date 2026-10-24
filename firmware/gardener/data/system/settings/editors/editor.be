# editor.be — Editor: what a setting carries as its editor, the whole of what the value is.
# SettingsScene asks it instead of branching on a kind string, and every value that comes in — from
# the routine's file or from a switch's map — is held to it before a page sees it.
#
# A kind that cannot work — a choice with no values, bounds the wrong way round — says so in
# `problem` and logs one line where it is built. It does not raise: the routine still registers, its
# settings view leaves that row out, and a read of it answers the setting's default. One subclass
# per kind is in this folder, each opening the value scene of the same name in ../scenes/.

class Editor
  var nullable
  var problem

  def init(nullable)
    self.nullable = nullable
    self.problem = nil
  end

  # The setting's value from the routine's file, or its default when nothing was stored. A nullable
  # setting that holds nothing reads as nil whatever its default says.
  def read(name, key, default)
    if self.problem != nil
      return default
    end
    if self.nullable && store.get(name, key, "") == ""
      return nil
    end
    # The store reads a value as the type of the fallback it is given, so a setting with no default
    # borrows the kind's own zero for that.
    var fallback = default
    if fallback == nil
      fallback = self.lowest()
    end
    return self.value(store.get(name, key, fallback), default)
  end

  # A raw value - from a switch's map, or from a file - held to the kind: nil when the kind is
  # nullable and there is nothing, the default when there is nothing else, and otherwise what
  # bounded() makes of it.
  def value(raw, default)
    if self.problem != nil
      return default
    end
    if raw == nil
      if self.nullable
        return nil
      end
      return default
    end
    var checked = self.bounded(raw)
    if checked == nil
      return default
    end
    return checked
  end
end

