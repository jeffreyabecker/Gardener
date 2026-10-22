# scene.be — the lifecycle a scene shares: the gesture drain, the repaint cadence and the
# declarative gesture dispatch.

import string
#
# A scene extends Scene and overrides only what differs: activate() for when it becomes the top
# scene, draw_page() for its content, and tick(now) for per-beat work. Gestures are registered with
# on(btn, presses, fn) in init(), and handle_page() dispatches them. A scene pushes a sub-scene
# with ux.push(scene, callback) and learns what the sub-scene returned through the callback; the
# callback runs when the sub-scene pops, and the scene below keeps its state.

class Scene
  var last_draw
  var gestures

  def init()
    self.last_draw = 0
    self.gestures = []
  end

  # The queue is a pull: drain it every poll so a press is never left waiting for the next scene.
  def take_events()
    var acted = false
    while true
      var ev = buttons.take()
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

  # Declarative gestures: the first registered (button, press) match whose action acts wins.
  # `presses` is one gesture token, a list of them, or nil for any press. Back must never be nil:
  # the device's own long Back hold is reserved, so Back names its presses explicitly.
  def on(btn, presses, fn)
    self.gestures.push([btn, presses, fn])
  end

  # The gesture is a string — `ok:short`, `back:long`, `up:short+down:short` — and a registration
  # names one key and one press token, or nil for any press on that key. The first part is the one
  # a registration is matched against.
  def handle_page(ev)
    for g : self.gestures
      if self.press_matches(ev, g[0], g[1])
        if g[2]()
          return true
        end
      end
    end
    return false
  end

  def press_matches(ev, btn, wanted)
    var first = string.split(ev, "+")[0]
    var parts = string.split(first, ":")
    if parts.size() != 2 || parts[0] != btn
      return false
    end
    if wanted == nil
      return true
    end
    var press = parts[1]
    if type(wanted) == "list"
      for p : wanted
        if p == press
          return true
        end
      end
      return false
    end
    return wanted == press
  end

  def draw()
    screen.draw_clear()
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
