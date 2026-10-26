# readout_settings.be — the device's own settings as a settings-list declaration.
#
# The device's own settings are not a routine's: they live behind `device` and `screen` rather than
# in a routine's document. The rows only declare them — the readout reads the device to fill the set
# in and writes it back when the list commits — and the editors are the settings folder's.
#
# It loads after jingles_edit.be, which provides the jingle row, and before ui.be, whose ReadoutScene
# opens this set and commits it back to the device.

# The row names a unit the way the glass prints it, so the row and its editor show the same label the
# overlay puts after a reading.
def unit_choices()
  var out = []
  for unit : device.units()
    out.push([unit.name, unit.suffix])
  end
  return out
end

# The radio row reports what the build was compiled with: there is nothing to write and nothing to
# edit, so Forward on it leaves the list where it is.
class BluetoothEditCfg : BoolEditCfg
  def init()
    super(self).init("bluetooth", device.bluetooth(), "BT", nil, false)
  end

  def scene(value)
    return nil
  end
end

class ReadoutSettings
  # The readout's set, in the order the list shows them: the target it works to, the unit it prints
  # and the jingle it finishes with, then the radio as a build fact. The rows only declare them.
  var settings_cfgs

  def init()
    var out = [
      PressureEditCfg("target_pressure", 0.0, "Target", nil, 1, false),
      ChoiceEditCfg("unit", screen.unit().name, "UNIT", nil, unit_choices(), false)
    ]
    # No library is a fault the boot already reported, and a choice with no values has nothing to
    # cycle, so the row is left out rather than opened on an empty list.
    var jingle = JingleEditCfg()
    if jingle.values.size() > 0
      out.push(jingle)
    end
    out.push(BluetoothEditCfg())
    self.settings_cfgs = out
  end

  def problem(values)
    return nil
  end
end
