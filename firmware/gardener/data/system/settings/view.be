# view.be — the settings list's geometry: the row it draws a setting on, the gap under its heading,
# and the window a box holds. The value editors carry their own numbers.

class SettingsView
  # The list's row pitch — one row of the built-in face — and the gap it leaves under a heading.
  static var row = List.row
  static var gap = 2

  # The scale a value is drawn in and measured at: every kind's band shares it, so one value reads
  # the same size as the next. The built-in face at two is sixteen pixels tall, a pixel of paper
  # above and below in a twenty-pixel band.
  static var scale = 2

  # The rows a full-box list may show: the box less the heading row and the gap under it.
  static def window_rows(dim)
    return (dim.h - SettingsView.row - SettingsView.gap) / SettingsView.row
  end
end
