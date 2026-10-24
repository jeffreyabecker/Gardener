# records.be — the classes a native call answers with, and the one a routine builds for its own
# settings. A routine reads a record with dots — `s.pressure_kpa`, `ev.btn` — and this interpreter
# allows that only on an instance whose class declares the field; only the parser makes an instance
# variable, so the field list here is the contract between this file and the native call that fills
# it.
#
# A record is the state at the moment of the call, and writing one of its fields changes the
# routine's copy and nothing else.

class reading
  var pressure_kpa, target, duty, floor, min, state, fault, batt, sensor_ok
end

class unit
  var name, suffix, per_kpa, whole_step, decimals
end

class bounds
  var w, h
end

# What a routine registers as one of its editable values. The class is constructed by the routine,
# so its fields stay plain data; `editor` is the kind's editor object, which owns how the value is
# read, printed and edited; `default` is what it holds when nothing has been stored, and `icon` is
# what the settings list draws at the left of the row, or nil.
class setting
  var key, label, icon, editor, default

  def init(key, label, editor, default, icon)
    self.key = key
    self.label = label
    self.icon = icon
    self.editor = editor
    self.default = default
  end
end
