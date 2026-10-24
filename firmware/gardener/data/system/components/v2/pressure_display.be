# pressure_display.be — the v2 PressureDisplay: the reading formatted in the panel's unit, filling
# the box with Text's fit.

import string

class PressureDisplayV2 : ComponentV2
  var family
  var style
  var align
  var v_align
  var number

  def init()
    super(self).init()
    self.family = "sans"
    self.style = "none"
    self.align = "center"
    self.v_align = "center"
    self.number = TextV2()
  end

  # The number fills the box. The unit is the overlay's, not the display's.
  def draw(pressure_kpa)
    var u = screen.unit()
    var text = "--"
    if pressure_kpa != nil
      text = string.format("%." + str(u.decimals) + "f", pressure_kpa * u.per_kpa)
    end
    self.number.family = self.family
    self.number.style = self.style
    self.number.align = self.align
    self.number.v_align = self.v_align
    self.number.x = self.x
    self.number.y = self.y
    self.number.w = self.w
    self.number.h = self.h
    self.number.draw(text)
    self.flip()
  end
end
