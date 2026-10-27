# device.be — the device's own settings as a settings-list declaration.
#
# The device's own settings are not a routine's: they live behind `device` and `screen` rather than
# in a routine's document. The rows only declare them — the home scene reads the device to fill the
# set in and writes it back when the list commits — and the editors are the settings folder's.
#
# It is named in /system/order after the kinds its rows are built from, the jingle row included, and
# before home.be, whose HomeScene opens this set and commits it back to the device.

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

class DeviceSettings
  # The home scene's set, in the order the list shows them: the target it works to, the unit it
  # prints and the jingle it finishes with, then the radio as a build fact. The rows only declare
  # them.
  var settings_cfgs

  def init()
    self.settings_cfgs = [
      PressureEditCfg("target_pressure", 0.0, "Target", nil, 1, false),
      ChoiceEditCfg("unit", screen.unit().name, "UNIT", nil, unit_choices(), false),
      # A build that ships no jingle library still shows the row, so the list's shape does not
      # change with the image: it opens on no entry and keeps none.
      JingleEditCfg(),
      BluetoothEditCfg()
    ]
  end

  def problem(values)
    return nil
  end
end
