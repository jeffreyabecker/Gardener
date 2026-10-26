# records.be — the records the image shares: the classes a native call answers with, and the pair a
# page builds a box from. A routine reads a record with dots — `s.pressure_kpa`, `ev.btn` — and this
# interpreter allows that only on an instance whose class declares the field; only the parser makes
# an instance variable, so the field list here is the contract with whoever fills it.
#
# A record a native call answers is the state at the moment of the call, and writing one of its
# fields changes the routine's copy and nothing else. `point` and `bounds` are the page's own: the
# corner a box starts at and the width and height it covers.
#
# The file loads before any page, so it also holds the one global the pages share: `is_nil`, the
# first of its arguments that is not nil, which turns a stored-or-default choice into one line.

class reading
  var pressure_kpa, target, duty, floor, min, state, fault, batt, batt_low, sensor_ok
end

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

# The width and height of a box, in pixels; the share of a parent's box an AutoLayout child declares,
# each a number in [0, 1] or nil for the whole of the parent; and the ink box `screen.text_bounds`
# answers.
class bounds
  var w, h
  def init(w, h)
    self.w = w
    self.h = h
  end
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
