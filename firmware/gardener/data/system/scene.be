# scene.be — the lifecycle a scene shares: the box it draws in, the gesture drain, the repaint
# cadence and the gesture dispatch.
#
# A scene extends Scene and overrides only what differs: activate() for work that must wait until
# the scene is on top, draw_page() for its content, handle(ev) for the gestures it answers, and
# tick(now) for per-beat work. A scene built around its init() — every routine page is — leaves
# activate() alone: it is the inherited no-op, and initializing the scene is the whole of its
# activation. A gesture is one whole string — `ok:single`, `cancel:double`, `ok:5+cancel:5` — and a
# scene matches it in full, so it never matches one dimension of a gesture without the other. A
# scene pushes a sub-scene with ux.push(scene, callback) and learns what the sub-scene returned
# through the callback; the callback runs when the sub-scene pops, and the scene below keeps its
# state.
#
# Every scene draws in the page: it carries `pos` and `dim` as a component does, and they are the
# page's box however the scene was pushed.

class Scene
  var last_draw
  var pos
  var dim

  # A scene always takes the whole page. `pos` and `dim` are what draw_page() lays out in, and a
  # scene opened over another one covers the page and clears it when it draws.
  def init()
    self.last_draw = 0
    self.pos = point(0, 0)
    self.dim = bounds(screen.page_width(), screen.page_height() - overlay_band_height)
  end

  # The queue is a pull: drain it every poll so a press is never left waiting for the next scene.
  def take_events()
    var acted = false
    while true
      var ev = device.take_gesture()
      if ev == nil
        break
      end
      if self.handle(ev)
        acted = true
      end
    end
    return acted
  end

  # The gesture the scene answers, or false for one it drops. A scene compares the whole string it
  # was given and answers what it knows; the base knows nothing.
  def handle(ev)
    return false
  end

  # Called when the scene becomes the top one, with whatever the pusher gave it. A scene that did
  # its work in init() has nothing to do here.
  def activate(args)
  end

  # The scene's own box is cleared rather than the panel: a scene opened over another one leaves what
  # that one drew around it on the glass. 0 is paper.
  def draw()
    screen.draw_fillrect(self.pos.x, self.pos.y, self.dim.w, self.dim.h, 0)
    self.draw_page()
    self.last_draw = vacuum.now_ms()
  end

  def draw_page()
  end

  def poll(now)
    var acted = self.take_events()
    self.tick(now)
    if acted || now - self.last_draw >= 250
      self.draw()
    end
  end

  def tick(now)
  end
end
