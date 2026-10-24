# pressure_display.be — PressureDisplay: the reading formatted in the panel's unit,
# filling the content rect.

import string

class PressureDisplay : Component
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
    self.number = DynamicText()
  end

  # The number fills the content rect. A `border` draws the frame first, which is what lets a value
  # sit in the layout without a label beside it: the box says what the number is. The unit is the
  # overlay's, on the top row with the state icon, so a readout carries no suffix of its own.
  def draw(pressure_kpa)
    var u = screen.unit()
    var text = "--"
    if pressure_kpa != nil
      text = string.format("%." + str(u.decimals) + "f", pressure_kpa * u.per_kpa)
    end
    if self.border > 0
      var f = self.frame()
      screen.draw_rect(f[0], f[1], f[2], f[3])
    end
    self.number.family = self.family
    self.number.style = self.style
    self.number.align = self.align
    self.number.v_align = self.v_align

    var c = self.content()
    self.number.x = c[0]
    self.number.y = c[1]
    self.number.w = c[2]
    self.number.h = c[3]
    self.number.draw(text)
    self.flip()
  end
end
