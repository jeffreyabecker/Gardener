# overlay.be — the status band as a layout: the four cells the ux component owns across the bottom
# of the glass, laid out as a row of fractional cells the way a page lays out its rows. The band is
# the layout; ux positions it with to_box() and draws it each beat, so the cells resolve from the
# box they are given rather than hard-coded pixel numbers.
#
# A cell's width is the fraction of the panel its field has always occupied: the unit left of the
# first gap, the two links, the run state, and the battery at the right edge. On the 128-wide panel
# the fractions land on the same pixels the hand-placed band did.

# The Bluetooth rune, packed the way screen.draw_bitmap reads it: width, height, then one bit per
# pixel, most-significant bit first, two bytes a row. Hex rather than a string so the 0x00 rows
# survive the string form's NUL cut.
bluetooth_bits = bytes().fromhex("1009010009400510014000800140051009400100")

# The unit the glass prints, in the built-in face at the left of its cell, as the band always has.
class UnitText : Component
  def draw()
    screen.draw_text(self.pos.x + 2, self.pos.y + 2, 1, false, screen.unit().suffix)
    super(self).draw()
  end
end

# The two ways in: a glyph each, drawn only while its link is up. The serial glyph sits at the
# cell's left and the Bluetooth rune 13 px in, the same offsets the hand-placed band used.
class LinksCell : Component
  def draw()
    var links = device.links()
    if links.serial
      screen.draw_glyph(self.pos.x, self.pos.y + 2, 0x15)
    end
    if links.bluetooth
      screen.draw_bitmap(self.pos.x + 13, self.pos.y, bluetooth_bits)
    end
    super(self).draw()
  end
end

# One glyph per state, from the built-in face's symbol range, so the mode reads without the word
# beside it. The two states that move the chamber blink — lit for 65% of a 1000 ms period — so the
# band says a machine is working rather than stopped.
class StateCell : Component
  def draw()
    var s = vacuum.state()
    var lit = (vacuum.now_ms() % 1000) < 650
    if s.state == "PULL"
      if lit
        screen.draw_glyph(self.pos.x, self.pos.y + 2, 0x1E)
      end
    elif s.state == "VENT"
      if lit
        screen.draw_glyph(self.pos.x, self.pos.y + 2, 0x1F)
      end
    elif s.state == "HOLD"
      screen.draw_glyph(self.pos.x, self.pos.y + 2, 0x0A)
    elif s.state == "FAULT"
      screen.draw_glyph(self.pos.x, self.pos.y + 2, 0x58)
    else
      screen.draw_glyph(self.pos.x, self.pos.y + 2, 0x08)
    end
    super(self).draw()
  end
end

# The battery: the icon is the whole of it, the number is on the wire, so the panel does not spend
# characters on it. No gauge draws the slashed outline; below the low threshold the fill blinks.
class BatteryCell : Component
  def draw()
    var s = vacuum.state()
    if s.batt == -1
      self.empty_battery_icon(self.pos.x, self.pos.y + 2)
      super(self).draw()
      return
    end
    var blink_off = s.batt_low && ((vacuum.now_ms() / 500) % 2 == 0)
    self.battery_icon(self.pos.x, self.pos.y + 2, blink_off ? 0 : s.batt)
    super(self).draw()
  end

  # The icon owns its whole footprint, so it erases it first: a page may have drawn anything
  # under it, and filling over the page without clearing would read as a battery with the page's
  # pixels still visible inside it.
  def battery_icon(x, y, percent)
    screen.draw_fillrect(x, y, 12, 7, 0)
    screen.draw_rect(x, y, 10, 7, 1)
    screen.draw_fillrect(x + 10, y + 2, 2, 3, 1)
    if percent == 0
      return
    end
    var fill = 6 * percent / 100
    if fill < 1
      fill = 1
    end
    if fill > 6
      fill = 6
    end
    screen.draw_fillrect(x + 2, y + 2, fill, 3, 1)
  end

  # No gauge fitted: the battery's outline with a slash through it, so the cell says there is no
  # battery rather than no charge.
  def empty_battery_icon(x, y)
    screen.draw_fillrect(x, y, 12, 7, 0)
    screen.draw_rect(x, y, 10, 7, 1)
    screen.draw_fillrect(x + 10, y + 2, 2, 3, 1)
    screen.draw_line(x + 1, y + 1, x + 8, y + 5, 1)
  end
end

# The band is the layout ux draws; it is not bound to the page, so ux positions it over the bottom
# rows with to_box().
ux.band = AutoLayout(bounds(1, nil), [
  AutoLayout(bounds(.25, 1), [UnitText()]),
  AutoLayout(bounds(.25, 1), [LinksCell()]),
  AutoLayout(bounds(.25, 1), [StateCell()]),
  AutoLayout(bounds(.25, 1), [BatteryCell()])
])
