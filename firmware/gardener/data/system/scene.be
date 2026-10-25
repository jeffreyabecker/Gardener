# scene.be — the lifecycle a scene shares: the box it draws in, the gesture drain, the repaint
# cadence and the declarative gesture dispatch.
#
# A scene extends Scene and overrides only what differs: activate() for work that must wait until
# the scene is on top, draw_page() for its content, and tick(now) for per-beat work. A scene built
# around its init() — every routine page is — leaves activate() alone: it is the inherited no-op,
# and initializing the scene is the whole of its activation. Gestures are registered with
# on(gesture, fn) in init(), and handle_page() dispatches them. A gesture is one whole string —
# `ok:single`, `cancel:double`, `ok:5+cancel:5` — and a registration matches it in full; a page never
# matches one dimension of a gesture without the other. A scene pushes a sub-scene with
# ux.push(scene, callback) and learns what the sub-scene returned through the callback; the
# callback runs when the sub-scene pops, and the scene below keeps its state.
#
# Every scene carries the box it draws in, `pos` and `dim`, as a component does; a scene opened over
# another one is given that one's box.

class Scene
  var last_draw
  var gestures
  var pos
  var dim

  # A scene draws in a box: the page for one the stack is rooted on, and the box of the scene it is
  # opened over for anything on top of one. `pos` and `dim` are what draw_page() lays out in.
  def init(pos, dim)
    self.last_draw = 0
    self.gestures = []
    self.pos = pos == nil ? Position.top_left() : pos
    self.dim = dim == nil ? Size.page() : dim
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

  def handle(ev)
    return self.handle_page(ev)
  end

  # Called when the scene becomes the top one, with whatever the pusher gave it. A scene that did
  # its work in init() has nothing to do here.
  def activate(args)
  end

  # Declarative gestures: the first registered gesture whose action acts wins. The gesture is one
  # whole string, compared in full — a page matches a gesture or leaves it alone.
  def on(gesture, fn)
    self.gestures.push([gesture, fn])
  end

  def handle_page(ev)
    for g : self.gestures
      if ev == g[0]
        if g[1]()
          return true
        end
      end
    end
    return false
  end

  # The scene's own box is cleared rather than the panel: a scene opened over another one leaves what
  # that one drew around it on the glass. 0 is paper.
  def draw()
    screen.draw_fillrect(self.pos.x, self.pos.y, self.dim.w, self.dim.h, 0)
    self.draw_page()
    screen.flush()
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
