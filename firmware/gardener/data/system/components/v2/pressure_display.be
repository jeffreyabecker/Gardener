# pressure_display.be — the v2 PressureDisplay: the reading formatted in the panel's unit, filling
# the box with Text's fit, and an optional secondary reading smaller in a corner or side by side.

import string

class PressureDisplayV2 : ComponentV2
  var family
  var style
  var align
  var v_align
  var secondary_family
  var secondary_style
  var secondary_pressure
  var secondary_position
  var size_hint
  var number
  var secondary

  def init()
    super(self).init()
    self.family = "sans"
    self.style = "none"
    self.align = "center"
    self.v_align = "center"
    self.secondary_family = nil
    self.secondary_style = nil
    self.secondary_pressure = nil
    self.secondary_position = "br"
    self.size_hint = 0.5
    self.number = TextV2()
    self.secondary = TextV2()
  end

  def format(pressure_kpa)
    if pressure_kpa == nil
      return "--"
    end
    var u = screen.unit()
    return string.format("%." + str(u.decimals) + "f", pressure_kpa * u.per_kpa)
  end

  def draw(pressure_kpa)
    var main_text = self.format(pressure_kpa)
    var sec_text = self.secondary_pressure != nil ? self.format(self.secondary_pressure) : nil

    if sec_text != nil && self.size_hint >= 1
      self.draw_side_by_side(main_text, sec_text)
    else
      self.draw_main(main_text)
      if sec_text != nil
        self.draw_corner(sec_text)
      end
    end
    self.flip()
  end

  def draw_main(text)
    self.number.family = self.family
    self.number.style = self.style
    self.number.align = self.align
    self.number.v_align = self.v_align
    self.number.x = self.x
    self.number.y = self.y
    self.number.w = self.w
    self.number.h = self.h
    self.number.draw(text)
  end

  # The secondary smaller in the corner the position names: the fit is capped at size_hint of the
  # main number's face height.
  def draw_corner(text)
    var main_ink = self.number.bounds(self.number.value, self.number.candidate)
    var cap = main_ink[1] * self.size_hint
    var face = self.secondary.fit_height(text, cap)

    self.secondary.family = self.secondary_family != nil ? self.secondary_family : self.family
    self.secondary.style = self.secondary_style != nil ? self.secondary_style : self.style
    if self.secondary_position == "tl"
      self.secondary.align = "left"
      self.secondary.v_align = "top"
    elif self.secondary_position == "tr"
      self.secondary.align = "right"
      self.secondary.v_align = "top"
    elif self.secondary_position == "bl"
      self.secondary.align = "left"
      self.secondary.v_align = "bottom"
    else
      self.secondary.align = "right"
      self.secondary.v_align = "bottom"
    end
    self.secondary.x = self.x
    self.secondary.y = self.y
    self.secondary.w = self.w
    self.secondary.h = self.h
    self.secondary.draw_face(text, face)
  end

  # At size_hint 1 the two numbers are the same size, side by side; the secondary sits on the side
  # the second character of the position names.
  def draw_side_by_side(main_text, sec_text)
    var half = self.w / 2
    var on_left = string.endswith(self.secondary_position, "l")

    self.number.family = self.family
    self.number.style = self.style
    self.number.align = self.align
    self.number.v_align = self.v_align
    self.number.x = on_left ? self.x + half : self.x
    self.number.y = self.y
    self.number.w = half
    self.number.h = self.h
    self.number.draw(main_text)

    self.secondary.family = self.secondary_family != nil ? self.secondary_family : self.family
    self.secondary.style = self.secondary_style != nil ? self.secondary_style : self.style
    self.secondary.align = "center"
    self.secondary.v_align = "center"
    self.secondary.x = on_left ? self.x : self.x + half
    self.secondary.y = self.y
    self.secondary.w = half
    self.secondary.h = self.h
    self.secondary.draw(sec_text)
  end
end
