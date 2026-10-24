# pressure_display.be — the v2 PressureDisplay: the reading formatted in the panel's unit, filling
# the box with Text's fit, and an optional secondary reading smaller in a corner or side by side.
#
# Berry has no access modifiers, so privacy here is the name: a leading underscore marks what only
# this class touches, and everything without one is what a caller sets or calls.

import string

class pressure_settings
  var family
  var style
  var position
  var size_hint

  def init(family, style, position, size_hint)
    self.family = family == nil ? "sans" : family
    self.style = style == nil ? "none" : style
    self.position = position == nil ? "br" : position
    self.size_hint = size_hint == nil ? 0.5 : size_hint
  end
end

class PressureDisplayV2 : ComponentV2
  # --- public --------------------------------------------------------------
  # Set by the caller before a draw.
  var family
  var style
  var align
  var v_align
  var secondary_family
  var secondary_style
  var secondary_pressure
  var secondary_position
  var size_hint

  # --- private -------------------------------------------------------------
  # The two faces the readings are drawn with, stamped with this component's box and settings on
  # every draw.
  var _number
  var _secondary

  # --- public methods ------------------------------------------------------
  def init(text_settings, text_settings_secondary, secondary_position, size_hint)
    super(self).init()
    text_settings = text_settings == nil ? text_settings.default() : text_settings
    text_settings.apply_to(self)
    self.secondary_family = text_settings_secondary ? text_settings_secondary.family : nil
    self.secondary_style = text_settings_secondary ? text_settings_secondary.style : nil
    self.secondary_pressure = nil
    self.secondary_position = "br"
    self.size_hint = 0.5
    self._number = TextV2()
    self._secondary = TextV2()
  end

  def draw(pressure_kpa, secondary_pressure)
    pressure_kpa = pressure_kpa == nil ? self.pressure_kpa : pressure_kpa
    secondary_pressure = secondary_pressure == nil ? self.secondary_pressure : secondary_pressure
    var main_text = self._format(pressure_kpa)
    var sec_text = nil
    if self.secondary_position != nil && secondary_pressure != nil
      sec_text = self._format(secondary_pressure)
    end

    if sec_text != nil && self.size_hint >= 1
      self._draw_side_by_side(main_text, sec_text)
    else
      self._draw_main(main_text)
      if sec_text != nil
        self._draw_corner(sec_text)
      end
    end
    self.flip()
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
    self._number.family = self.family
    self._number.style = self.style
    self._number.align = self.align
    self._number.v_align = self.v_align
    self._number.x = self.x
    self._number.y = self.y
    self._number.w = self.w
    self._number.h = self.h
    self._number.draw(text)
  end

  # The secondary smaller in the corner the position names: the fit is capped at size_hint of the
  # main number's face height.
  def _draw_corner(text)
    var main_ink = self._number.bounds(self._number.value, self._number.candidate)
    var cap = main_ink[1] * self.size_hint
    var face = self._secondary.fit_height(text, cap)

    self._secondary.family = self.secondary_family != nil ? self.secondary_family : self.family
    self._secondary.style = self.secondary_style != nil ? self.secondary_style : self.style
    if self.secondary_position == "tl"
      self._secondary.align = "left"
      self._secondary.v_align = "top"
    elif self.secondary_position == "tr"
      self._secondary.align = "right"
      self._secondary.v_align = "top"
    elif self.secondary_position == "bl"
      self._secondary.align = "left"
      self._secondary.v_align = "bottom"
    else
      self._secondary.align = "right"
      self._secondary.v_align = "bottom"
    end
    self._secondary.x = self.x
    self._secondary.y = self.y
    self._secondary.w = self.w
    self._secondary.h = self.h
    self._secondary.draw_face(text, face)
  end

  # At size_hint 1 the two numbers are the same size, side by side; the secondary sits on the side
  # the second character of the position names.
  def _draw_side_by_side(main_text, sec_text)
    var half = self.w / 2
    var on_left = string.endswith(self.secondary_position, "l")

    self._number.family = self.family
    self._number.style = self.style
    self._number.align = self.align
    self._number.v_align = self.v_align
    self._number.x = on_left ? self.x + half : self.x
    self._number.y = self.y
    self._number.w = half
    self._number.h = self.h
    self._number.draw(main_text)

    self._secondary.family = self.secondary_family != nil ? self.secondary_family : self.family
    self._secondary.style = self.secondary_style != nil ? self.secondary_style : self.style
    self._secondary.align = "center"
    self._secondary.v_align = "center"
    self._secondary.x = on_left ? self.x : self.x + half
    self._secondary.y = self.y
    self._secondary.w = half
    self._secondary.h = self.h
    self._secondary.draw(sec_text)
  end
end

class AutoPressureDisplayV2: PressureDisplayV2
  var _pressure_order
  def init(pressure_order, secondary_position, size_hint)
    super(self).init()
    if pressure_order == nil
      pressure_order = "pressure:target"
    if type(pressure_order) == "string"
      var parts = string.split(pressure_order, ":")
      var trimmed = []
      for p : parts
        p = string.strip(p)
        if p != ""
          trimmed.push(p)
        end
      end
      pressure_order = trimmed
    end
    self._pressure_order = pressure_order
  end
  def draw()
    var primary_pressure = _pressure_order[0] == "pressure" ? vacuum.pressure_kpa : vacuum.target
    var secondary_pressure = _pressure_order.size() > 1 ? (_pressure_order[1] == "pressure" ? vacuum.pressure_kpa : vacuum.target) : nil
    super(self).draw(primary_pressure, secondary_pressure)
  end
end