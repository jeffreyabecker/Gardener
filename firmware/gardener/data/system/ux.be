# ux.be — the scene stack: what is showing, what each scene handed back, and the calls the firmware
# makes per beat. `ux` is the global every page and routine already calls.
#
# A scene is activated when it is pushed, polled while it is on top, and hands its value to the
# pusher's callback when it pops. A push or pop asked for from inside activate() or poll() is held
# and applied once that call returns, because a scene's poll ends by drawing: a pop applied during it
# would paint the page that just left over the one that came back. One held request per beat, so a
# push from inside an activate is a beat later rather than a recursion.
#
# A scene that raises takes the page with it — the stack empties, so the panel falls back to the mark
# — and the raise goes on to the firmware, which prints it as `[E] vm: poll: ...`.

# The deepest stack the panel accepts. A scene that pushes from its own activate is a bug either way;
# the cap is what keeps it a log line instead of a heap exhausted in one beat.
max_frames = 8

# The status band the ux component owns across the bottom of the glass: 12 rows of unit, links, run
# state and battery. A page lays out in the area above it (Size.page()), and nothing a page draws
# belongs here.
overlay_band_height = 12

# The Bluetooth rune, packed the way screen.draw_bitmap reads it: width, height, then one bit per
# pixel, most-significant bit first, two bytes a row. Hex rather than a string so the 0x00 rows
# survive the string form's NUL cut.
bluetooth_bits = bytes().fromhex("1009010009400510014000800140051009400100")

class Ux
  var frames      # [[scene, callback], ...], root first
  var pending     # the one held request, or nil
  var polling

  def init()
    self.frames = []
    self.pending = nil
    self.polling = false
  end

  # The firmware's way in: the root scene at boot, and the go-home gesture. A nil root empties the
  # stack and leaves the mark.
  def reset(root)
    self.frames = []
    self.pending = nil
    if root != nil
      self.place(root, nil, nil)
    end
    return self.depth() > 0
  end

  # --- what a scene calls ----------------------------------------------------

  def push(scene, args, callback)
    if self.polling
      self.pending = ["push", scene, args, callback]
      return true
    end
    return self.place(scene, args, callback)
  end

  def pop(value)
    if self.polling
      self.pending = ["pop", value]
      return true
    end
    return self.leave(value)
  end

  def pop_to_root(value)
    if self.polling
      self.pending = ["to_root", value]
      return true
    end
    return self.home(value)
  end

  def top()
    if self.frames.size() == 0
      return nil
    end
    return self.frames[-1][0]
  end

  # The box of the scene on top: what a scene being built is about to cover, and so the box it draws
  # in. A scene's init() runs before its push, so the top is the scene it is opened over. Nothing on
  # the stack answers nothing, which a scene takes as the page.
  def box()
    var scene = self.top()
    if scene == nil
      return [nil, nil]
    end
    return [scene.pos, scene.dim]
  end

  def depth()
    return self.frames.size()
  end

  # --- the beat --------------------------------------------------------------

  def enter()
    var scene = self.top()
    if scene == nil
      return false
    end
    self.guard(/ -> scene.activate(nil))
    self.apply()
    self.flush()
    return self.depth() > 0
  end

  def poll(now)
    var scene = self.top()
    if scene == nil
      return false
    end
    self.guard(/ -> scene.poll(now))
    self.apply()
    self.flush()
    return self.depth() > 0
  end

  # --- the stack -------------------------------------------------------------

  def place(scene, args, callback)
    if self.frames.size() >= max_frames
      vacuum.log("ux: the stack is full")
      return false
    end
    self.frames.push([scene, callback])
    self.guard(/ -> scene.activate(args))
    return true
  end

  # The pusher's callback gets the value; a frame pushed without one re-activates the scene below.
  # The root is never popped — that is what pop_to_root() is for.
  def leave(value)
    if self.frames.size() <= 1
      return false
    end
    var frame = self.frames.pop()
    var below = self.top()
    if frame[1] != nil
      self.guard(/ -> frame[1](value))
    elif below != nil
      self.guard(/ -> below.activate(nil))
    end
    return true
  end

  # Every scene above the root, in one move. The value has nowhere to go.
  def home(value)
    if self.frames.size() <= 1
      return true
    end
    while self.frames.size() > 1
      self.frames.pop()
    end
    var scene = self.top()
    self.guard(/ -> scene.activate(nil))
    return self.depth() > 0
  end

  # The one held request, applied between beats rather than inside the call that asked for it.
  def apply()
    var op = self.pending
    if op == nil
      return
    end
    self.pending = nil
    self.polling = true
    if op[0] == "push"
      self.place(op[1], op[2], op[3])
    elif op[0] == "pop"
      self.leave(op[1])
    else
      self.home(op[1])
    end
    self.polling = false
  end

  # One scene call with the guard on: a raise empties the stack and goes on to the firmware, so a
  # broken scene leaves the panel on the mark and the interpreter as it found it.
  def guard(fn)
    var held = self.polling
    self.polling = true
    try
      fn()
    except .. as e, m
      self.frames = []
      self.pending = nil
      self.polling = false
      raise e, m
    end
    self.polling = held
  end

  # --- the band ---------------------------------------------------------------

  # The status row across the bottom, drawn after the top scene's page and before the flush, so no
  # page has to remember it. The band is opaque: it clears its rows and redraws them on every beat.
  def flush()
    if self.frames.size() == 0
      return
    end
    var band_y = screen.height() - overlay_band_height
    screen.draw_fillrect(0, band_y, screen.width(), overlay_band_height, 0)
    self.draw_unit(band_y)
    self.draw_links(band_y)
    self.draw_state(band_y)
    self.draw_battery(band_y)
    screen.flush()
  end

  def draw_unit(band_y)
    screen.draw_text(2, band_y + 2, 1, false, screen.unit().suffix)
  end

  # Both cells say what is true now rather than what was true when the page was painted: the band is
  # cleared on the way to every frame, and an icon is drawn only while its link is up.
  def draw_links(band_y)
    var links = device.links()
    if links.serial
      screen.draw_glyph(53, band_y + 2, 0x15)
    end
    if links.bluetooth
      screen.draw_bitmap(66, band_y, bluetooth_bits)
    end
  end

  # One glyph per state, from the built-in face's symbol range, so the mode reads without the word
  # beside it. The two states that move the chamber blink — lit for 65% of a 1000 ms period — so the
  # band says a machine is working rather than stopped.
  def draw_state(band_y)
    var s = vacuum.state()
    var lit = (vacuum.now_ms() % 1000) < 650
    if s.state == "PULL"
      if lit
        screen.draw_glyph(103, band_y + 2, 0x1E)
      end
    elif s.state == "VENT"
      if lit
        screen.draw_glyph(103, band_y + 2, 0x1F)
      end
    elif s.state == "HOLD"
      screen.draw_glyph(103, band_y + 2, 0x0A)
    elif s.state == "FAULT"
      screen.draw_glyph(103, band_y + 2, 0x58)
    else
      screen.draw_glyph(103, band_y + 2, 0x08)
    end
  end

  # The icon is the whole of it: the number is on the wire, so the panel does not spend characters
  # on it. No gauge draws the slashed outline; below the low threshold the fill blinks.
  def draw_battery(band_y)
    var s = vacuum.state()
    if s.batt == -1
      self.empty_battery_icon(screen.width() - 12, band_y + 2)
      return
    end
    var blink_off = s.batt_low && ((vacuum.now_ms() / 500) % 2 == 0)
    self.battery_icon(screen.width() - 12, band_y + 2, blink_off ? 0 : s.batt)
  end

  def battery_icon(x, y, percent)
    # The icon owns its whole footprint, so it erases it first: a page may have drawn anything
    # under it, and filling over the page without clearing would read as a battery with the page's
    # pixels still visible inside it.
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

ux = Ux()
