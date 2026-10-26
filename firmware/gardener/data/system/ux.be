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
# state and battery. A page lays out in the area above it — the panel less those rows — and nothing a
# page draws belongs here.
overlay_band_height = 12

class Ux
  var frames      # [[scene, callback], ...], root first
  var pending     # the one held request, or nil
  var polling
  var band        # the overlay layout the band draws, built by overlay.be

  def init()
    self.frames = []
    self.pending = nil
    self.polling = false
    self.band = nil
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
    # The page is the panel less the band, and the stack is what owns the band: the box is stamped
    # here, once the scene is on top and before its activate() draws.
    scene.to_box(0, 0, screen.width(), screen.height() - overlay_band_height)
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
    self.band.to_box(0, band_y, screen.width(), overlay_band_height)
    self.band.draw()
    screen.flush()
  end
end

ux = Ux()
