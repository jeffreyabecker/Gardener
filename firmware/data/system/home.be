# home.be — the home scene: the device's own page, and the root the firmware will push.
#
# It registers with nothing. The home scene is the device's well-known page, so its gestures are the
# machine's: OK starts or resumes a run and Cancel stops and vents, Up held to a double opens the
# device's own settings list, and Forward held to a double pushes the browser. It reads the machine
# through `vacuum` and the clock through `device`, and it draws by answering a tree from draw_page() —
# the chrome `home_chrome()` builds from the run's clock — which the stack paints in the page.
#
# It is a Berry scene, not a class of the old component model: the four hooks are its own methods and
# the page clears itself, and no node carries a pixel of the panel's geometry because a node that
# names no size fills the cell the layout above it gave it.

import string

# The page's chrome from the run's clock: the title over the readout and the timer, a quarter and
# three quarters of the page. `elapsed` is the run's elapsed time in milliseconds, as the device's clock
# keeps it and as the static page's chrome takes it, or nil while nothing is running, so the drawing is
# reached by a case that holds a clock rather than a machine. Both strings name the ttyp0 design, so
# the band's height picks the cut and the page names no size.
def home_chrome(elapsed)
  var timer = screen.text(nil, nil, nil, nil,
                            screen.duration_text("minutes",
                                                 (elapsed == nil ? 0 : elapsed) / 1000,
                                                 elapsed != nil),
                            "ttyp0")
  return screen.column(nil, [
    [bounds(1, 0.25), screen.text(nil, nil, nil, nil, "Gardener", "ttyp0")],
    [bounds(1, 0.75), screen.row(nil, [
      [bounds(0.5, 1), screen.pressure_readout("ttyp0")],
      [bounds(0.5, 1), timer]
    ])]
  ])
end

class HomeScene
  var machine

  def init()
    self.machine = HomeMachine()
  end

  # The page went on top: the phase is read before the first draw, so coming back to a run the
  # device left going shows a run.
  def activate()
    self.machine.advance(device.now_ms())
  end

  # A gesture that names a command outright — cmd:stop — is handed to the machine as it stands with
  # the prefix trimmed; anything else is a key, and the page maps it to the command it means. Up held
  # to a double opens the device's own settings list, and Forward held to a double pushes the browser,
  # which vents the chamber as it opens; Back or Cancel there returns to this page.
  def handle(ev)
    if string.startswith(ev, "cmd:")
      return self.machine.handle_command(string.split(ev, ":")[1])
    end
    if ev == "up:double"
      return open_system_settings()
    end
    if ev == "forward:double"
      return self.machine.handle_command("browser")
    end
    if ev == "up:single"
      return self.machine.handle_command("inc_pressure")
    end
    if ev == "down:single"
      return self.machine.handle_command("dec_pressure")
    end
    if ev == "ok:single"
      return self.machine.handle_command("start")
    end
    if ev == "cancel:single"
      return self.machine.handle_command("stop")
    end
    return false
  end

  # The phase follows the chamber, so the page asks it every beat rather than keeping a copy that can
  # drift from the machine's own state.
  def tick(now)
    self.machine.advance(now)
  end

  # The drawing: the chrome from the run's clock, which is nil while nothing is running.
  def draw_page()
    return home_chrome(self.machine.elapsed())
  end
end

__rootScene = HomeScene()
