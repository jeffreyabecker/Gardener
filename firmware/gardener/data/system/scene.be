# scene.be — the lifecycle a scene shares: the box it draws in, the gesture drain, the repaint
# cadence and the gesture dispatch.
#
# A scene extends Scene and overrides only what differs: to_box() to hand the page's box on to the
# layout the page is composed in, activate() for the first paint and for work that must wait until
# the scene is on top, draw_page() for its content, handle(ev) for the gestures it answers, and
# tick(now) for per-beat work. ux stamps the page box on the scene and then calls activate(), in that
# order, so activate() is where a page draws itself. A gesture is one whole string — `ok:single`,
# `cancel:double`, `ok:5+cancel:5` — and a scene matches it in full, so it never matches one
# dimension of a gesture without the other. A scene pushes a sub-scene with ux.push(scene, callback)
# and learns what the sub-scene returned through the callback; the callback runs when the sub-scene
# pops, and the scene below keeps its state.
#
# Every scene draws in the page: it carries `pos` and `dim` as a component does, and they are
# stamped on it by `ux` when it goes on the stack.

class Scene
  var last_draw
  var pos
  var dim

  def init()
    self.last_draw = 0
  end
  def default_box()
    return [0, 0, screen.width(), screen.height() - overlay_band_height]
  end
  # The page's box, stamped on the scene as it is pushed and before its activate() runs. The base
  # takes the whole page; a scene that draws a panel of its own overrides default_box(). Nothing
  # draws a scene that has no box.
  def to_box(x, y, w, h)
    self.pos = point(x, y)
    self.dim = bounds(w, h)
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
