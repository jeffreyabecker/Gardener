# records.be — the records the image shares: the classes a native call answers with, and the pair a
# page builds a box from. A routine reads a record with dots — `u.name`, `l.serial` — and this
# interpreter allows that only on an instance whose class declares the field; only the parser makes
# an instance variable, so the field list here is the contract with whoever fills it.
#
# A record a native call answers is the state at the moment of the call, and writing one of its
# fields changes the routine's copy and nothing else. `point` and `bounds` are the page's own: the
# corner a box starts at and the width and height it covers.
#
# The file loads before any page, so it also holds the one global the pages share: `is_nil`, the
# first of its arguments that is not nil, which turns a stored-or-default choice into one line.

class unit
  var name, suffix, per_kpa, whole_step, decimals
end

class links
  var serial, bluetooth
end

# The corner a box starts at. A component carries one as `pos`, and a scene carries one for the page.
class point
  var x, y
  def init(x, y)
    self.x = x
    self.y = y
  end
end

# The width and height of a box, in pixels; the cell a Layout child declares, each a share of
# the parent in [0, 1], a number of pixels when it is above 1, or nil for the whole of the parent; and
# the ink box `screen.measure` answers.
class bounds
  var w, h
  def init(w, h)
    self.w = w
    self.h = h
  end
end

# Where a thing sits in the space it is given: `align` is left, center or right and `v_align` top,
# center or bottom, each nil for the left and the top. A layout takes one after its children, and a
# nil one is alignment("left", "top").
class alignment
  var align, v_align
  def init(align, v_align)
    self.align = align == nil ? "left" : align
    self.v_align = v_align == nil ? "top" : v_align
  end
end

# One node of a page's drawing, as `screen` builds it: `kind` names what the node is — text, centred,
# icon, inverted, bar, scrollbar, duration, pressure_readout, layout, row or column — and the fields
# that kind uses are set, the rest nil. A leaf names its box in pixels; a layout, a row and a column
# name the cell they declare as a share of what holds them, nil for the whole side, so a row sets `h`
# and a column `w`. A layout's `children` is a list of [bounds, draw] pairs — the cell a child
# declares and the child — and `alignment` is where it puts them, the record above. An `inverted`
# node holds the draw it flips in `child`, and an `icon` node holds its fixed bitmap in `glyph`: the
# width, the height, then one row per byte-run, most significant bit first and each row padded to a
# whole byte, as a `bytes`.
class draw
  var kind
  var x, y, w, h
  var value, font, scale, secondary_scale, size_hint
  var format, seconds, known
  var percent
  var index, count, visible, hollow
  var children, child, glyph, alignment
end

class jingle
  var key, full_title, short_title, notes
end

# The first argument that is not nil, or nil when every one of them is. With no arguments it is nil,
# so `is_nil(stored, fallback)` is the stored value when there is one and the fallback when there is
# not.
def is_nil(*values)
  for value : values
    if value != nil
      return value
    end
  end
  return nil
end

# The status band the ux component owns across the bottom of the glass: 12 rows of unit, links, run
# state and battery. A page lays out in the area above it — the panel less those rows — and nothing a
# page draws belongs here.
overlay_band_height = 12
