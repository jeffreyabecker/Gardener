# view.be — the settings view's geometry, in one place: the frame a value editor draws in and the
# rows both scenes lay their drawing out on. The list and the editors have to agree about these, so
# they read them here rather than carrying the numbers themselves.
#
# Every scene already carries the box it draws in, so a scene's size is its own `dim`; what is left
# to state is how a drawing sits inside that box.

class SettingsView
  # Paper outside the border, the border's own ink, and the paper inside it. A value editor's box is
  # three pixels bigger on each axis than the ink it holds.
  static var margin = 1
  static var border = 1
  static var padding = 1

  # A row of the built-in face — a label, a hint, or one row of a list — and the gap a scene leaves
  # between a chrome row and the drawing between them. List.draw draws at the face's own height, so
  # `row` is that height rather than a free choice.
  static var row = 8
  static var gap = 2

  # The tallest ink a value or a chip may take. An editor is a control rather than a readout: this is
  # what keeps a switch and a field a control's size instead of the page's.
  static var ink = 16

  # The two chrome rows and the gaps around them: what a body never has.
  static def chrome()
    return 2 * (SettingsView.row + SettingsView.gap)
  end

  # The most a body has in a box: the box less the frame and the chrome.
  static def body_cap(dim)
    return dim.h - 2 * (SettingsView.margin + SettingsView.border + SettingsView.padding) -
           SettingsView.chrome()
  end

  # The rows a full-box list may show: the box less the heading row and the gap under it.
  static def list_rows(dim)
    return (dim.h - SettingsView.row - SettingsView.gap) / SettingsView.row
  end

  # The box the border is drawn in: the content plus its padding and border, centred in the scene's
  # box with the margin kept. Content too big for the box gives up its padding before its margin.
  static def frame(pos, dim, size)
    var w = size.w + 2 * (SettingsView.padding + SettingsView.border)
    var h = size.h + 2 * (SettingsView.padding + SettingsView.border)
    var cap_w = dim.w - 2 * SettingsView.margin
    var cap_h = dim.h - 2 * SettingsView.margin
    if w > cap_w
      w = cap_w
    end
    if h > cap_h
      h = cap_h
    end
    return [Position(pos.x + (dim.w - w) / 2, pos.y + (dim.h - h) / 2), Size(w, h)]
  end

  # The box a kind draws in, inside the border and its padding. It is asked of frame(), so the two
  # boxes can never disagree.
  static def inner(pos, dim, size)
    var frame = SettingsView.frame(pos, dim, size)
    var inset = SettingsView.padding + SettingsView.border
    return [Position(frame[0].x + inset, frame[0].y + inset),
            Size(frame[1].w - 2 * inset, frame[1].h - 2 * inset)]
  end
end
