# pressure_display.be — the PressureDisplay: the reading formatted in the panel's unit and drawn in
# the built-in face at the scale it was built with, and an optional secondary reading in a corner or
# side by side. Each reading is placed by a layout of its own rather than by an offset worked out here.
#
# Berry has no access modifiers, so privacy here is the name: a leading underscore marks what only
# this class touches, and everything without one is what a caller sets or calls.

import string

class PressureDisplay : Component
  # --- public --------------------------------------------------------------
  # Set by the caller before a draw.
  var pressure_kpa
  var scale
  var secondary_scale
  var secondary_pressure
  var secondary_position
  var size_hint

  # --- private -------------------------------------------------------------
  # The two readings, and the layouts that place them: the main one filling the box, the secondary
  # alone in the box so a corner can place it, and the two halves a side-by-side pair shares.
  var _number
  var _secondary
  var _main
  var _corner
  var _left
  var _right
  var _row

  # --- public methods ------------------------------------------------------
  def init(scale, secondary_scale, secondary_position, size_hint)
    super(self).init()
    self.pressure_kpa = nil
    self.scale = scale == nil ? 2 : scale
    self.secondary_scale = secondary_scale == nil ? 1 : secondary_scale
    self.secondary_pressure = nil
    self.secondary_position = secondary_position == nil ? "br" : secondary_position
    self.size_hint = size_hint == nil ? 0.5 : size_hint
    self._number = Text("", nil, self.scale)
    self._secondary = Text("", nil, self.secondary_scale)
    self._main = Layout(bounds(nil, nil), [self._number], alignment("center", "center"))
    self._corner = Layout(bounds(nil, nil), [self._secondary])
    self._left = Layout(bounds(0.5, 1), [], alignment("center", "center"))
    self._right = Layout(bounds(0.5, 1), [], alignment("center", "center"))
    self._row = Layout(bounds(nil, nil), [self._left, self._right])
  end

  def draw()
    var main_text = self._format(self.pressure_kpa)
    var sec_text = nil
    if self.secondary_position != nil && self.secondary_pressure != nil
      sec_text = self._format(self.secondary_pressure)
    end

    if sec_text != nil && self.size_hint >= 1
      self._draw_side_by_side(main_text, sec_text)
    else
      self._draw_main(main_text)
      if sec_text != nil
        self._draw_corner(sec_text)
      end
    end
    super(self).draw()
  end

  # --- private methods -----------------------------------------------------
  def _format(pressure_kpa)
    if pressure_kpa == nil
      return "--"
    end
    var u = screen.unit()
    return string.format("%." + str(u.decimals) + "f", pressure_kpa * u.per_kpa)
  end

  def _draw_main(text)
    self._number.scale = self.scale
    self._number.value = text
    self._main.to_box(self.pos.x, self.pos.y, self.dim.w, self.dim.h)
    self._main.draw()
  end

  # The secondary smaller and in the corner the position names, at the scale it was given.
  def _draw_corner(text)
    self._secondary.scale = self.secondary_scale
    self._secondary.value = text
    self._corner.alignment = self.corner_alignment()
    self._corner.to_box(self.pos.x, self.pos.y, self.dim.w, self.dim.h)
    self._corner.draw()
  end

  # The alignment that puts the secondary in the corner its position names: the side comes from the
  # last character, the row from the first.
  def corner_alignment()
    var on_left = string.endswith(self.secondary_position, "l")
    var at_top = string.startswith(self.secondary_position, "t")
    return alignment(on_left ? "left" : "right", at_top ? "top" : "bottom")
  end

  # At size_hint 1 the two numbers sit side by side at one scale; the secondary takes the side the
  # second character of the position names.
  def _draw_side_by_side(main_text, sec_text)
    var on_left = string.endswith(self.secondary_position, "l")

    self._number.scale = self.scale
    self._number.value = main_text
    self._secondary.scale = self.scale
    self._secondary.value = sec_text

    self._left.children = [on_left ? self._secondary : self._number]
    self._right.children = [on_left ? self._number : self._secondary]
    self._row.to_box(self.pos.x, self.pos.y, self.dim.w, self.dim.h)
    self._row.draw()
  end
end

# A pressure display that pulls its readings from the machine: the order names which of the
# pressure and the target is primary, as "pressure:target" or "target:pressure".
class AutoPressureDisplay : PressureDisplay
  var _pressure_order

  def init(pressure_order, secondary_position, size_hint)
    super(self).init(nil, nil, secondary_position, size_hint)
    if pressure_order == nil
      pressure_order = "pressure:target"
    end
    if type(pressure_order) == "string"
      var parts = string.split(pressure_order, ":")
      var order = []
      for p : parts
        if p != ""
          order.push(p)
        end
      end
      pressure_order = order
    end
    self._pressure_order = pressure_order
  end

  def draw()
    var s = vacuum.state()
    self.pressure_kpa = self._pressure_order[0] == "pressure" ? s.pressure_kpa : s.target
    self.secondary_pressure = nil
    if self._pressure_order.size() > 1
      self.secondary_pressure = self._pressure_order[1] == "pressure" ? s.pressure_kpa : s.target
    end
    super(self).draw()
  end
end
