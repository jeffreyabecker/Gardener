# settings.be — the device's own settings list: the unit the glass prints, the jingle the device
# finishes with, and the longest run it allows. It registers with nothing; the home scene opens it.
#
# It is named in /system/order before home.be, which names open_system_settings in its gestures. The
# rows are the device's, not a routine's: the native list reads them from the document, and the set
# it settles on is written back through `device`.

# The list over the device's own rows: SystemSettingsScene answers the native list, and what it
# settles on is written back to /cfg/settings.ini when it pops. Back and Cancel hand back the set it
# opened on, which the document turns into no write when nothing changed.
def open_system_settings()
  return ux.push(SystemSettingsScene(), / values -> device.save_settings(values))
end
