# edit_scene.be — EditScene: the modal scene a setting is edited in, and the whole of what every
# value editor shares. It owns the keys while it is on top: Up/Down step the value it holds within
# the bounds it was handed, OK pops it, and Cancel hands back the value it was opened on. It draws
# the column every value editor draws its content in — the label row, the body the kind answers for,
# and the hint row — across the page it was given.
#
# A kind answers only what is its own: body() for its look, text() for its store, and the step hooks
# — step_of(), within(), hold() and low() — where a keypress, a bound or a clamp is the kind's. A
# plain number is no kind at all: it is an EditScene with the whole flag and the bounds it was
# declared with.

# The rows every value editor's column shares, and the chrome they cost down the page: a label row
# and a hint row at the system size, with two pixels of paper above and below the kind's body.
editor_text_row = 8
editor_paper_row = 2
editor_column_chrome = editor_text_row * 2 + editor_paper_row * 2

# The hint row a value editor opens with: the two arrows and OK, which keeps the value and closes. A
# kind with another key to offer appends it.
editor_hint = "\x1E/\x1F  OK=SAVE"

# The column every value editor draws its content in: the label row, two pixels of paper, the kind's
# body, two pixels of paper and the hint row, declared in pixels so the layout resolves them to the
# rows the content was measured at. The label, the body and the hint are centred in their rows; a
# paper row is a layout with no children: it takes its pixels and draws nothing. The label and the
# hint are the built-in face, the one that carries the arrows a hint prints.
def editor_column(label, hint, body, body_h)
  return Layout(bounds(nil, nil), [
    Layout(bounds(1, editor_text_row), [Text(label)], alignment("center", "center")),
    Layout(bounds(1, editor_paper_row), []),
    Layout(bounds(1, body_h), [body], alignment("center", "center")),
    Layout(bounds(1, editor_paper_row), []),
    Layout(bounds(1, editor_text_row), [Text(hint)], alignment("center", "center"))
  ])
end

class EditScene : Scene
  var label
  var opened
  var page
  var current
  var step
  var min
  var max
  var nullable
  var whole

  # The value being edited and the rules it steps within: the one the editor was opened on, the
  # increment a keypress takes, the bounds it is held between (nil for none), and whether the kind
  # holds whole numbers. A kind that is not a number leaves the rest to its own rules.
  def init(label, current, step, min, max, nullable, whole)
    super(self).init()
    self.label = label
    self.opened = current
    self.whole = whole == true
    self.current = self.number(current)
    self.step = self.number(step == nil ? 1.0 : step)
    self.min = self.number(min)
    self.max = self.number(max)
    self.nullable = nullable
    self.page = nil
  end

  # The scene owns the keys while it is on top: Up/Down changes the value, OK pops with it, Cancel
  # pops with the value the scene was opened on. A scene that pops itself pops from inside its own
  # poll; the runtime defers the pop until the poll returns, so it is safe to ask for it here.
  def activate()
    self.draw()
  end

  def handle(ev)
    if ev == "up:single"
      self.nudge(1)
      return true
    end
    if ev == "down:single"
      self.nudge(-1)
      return true
    end
    if ev == "ok:single"
      ux.pop(self.value())
      return true
    end
    if ev == "cancel:single"
      self.cancel()
      return true
    end
    return false
  end

  # Cancel is the row as it stands: the value the editor was opened on goes back, and the list takes
  # it as it takes any other. A kind that has to do something on the way out - the jingle that was
  # auditioning goes quiet - overrides this and calls up.
  def cancel()
    ux.pop(self.opened)
  end

  # The column, drawn on the page the scene was given: the label row at its top, the body under it
  # and the hint row under that, the whole width of the page and only as tall as the rows it holds.
  # It is built on every draw, so a kind whose body changes - a value, a stage, a row showing - has
  # nothing to keep in step.
  def draw_page()
    self.page = editor_column(self.label, self.hint(), self.body(), self.body_h())
    self.page.to_box(self.pos.x, self.pos.y, self.dim.w, editor_column_chrome + self.body_h())
    self.page.draw()
  end

  # The value at the one face every kind's band shares, over the bar that shows where it sits
  # between the bounds when the setting has both. A kind that shows something else - a switch, a
  # list, two fields - answers this itself.
  def body()
    var value = Text(self.text(), nil, SettingsView.scale)
    if self.min == nil || self.max == nil
      return value
    end
    return Layout(bounds(nil, nil), [
      Layout(bounds(1, self.measure()[1]), [value], alignment("center", "center")),
      Layout(bounds(1, 2), []),
      Layout(bounds(1, 6), [ProgressBar(self.percent())])
    ])
  end

  # The bar under the value takes six pixels with a gap of two above it.
  def body_h()
    var tall = self.measure()[1]
    if self.min != nil && self.max != nil
      tall += 2 + 6
    end
    return tall
  end

  # The value's ink at the scale every kind's band shares.
  def measure()
    var ink = screen.text_bounds(self.text(), SettingsView.scale)
    return [ink.w, ink.h]
  end

  # Where the value sits between the bounds, in whole percent; a nil value has no place on the bar.
  def percent()
    if self.current == nil || self.max == nil || self.max <= self.min
      return 0
    end
    return int((self.current - self.min) * 100 / (self.max - self.min))
  end

  def hint()
    return editor_hint
  end

  # The value the keys act on: OK pops it.
  def value()
    return self.current
  end

  # The value as the panel prints it; a setting holding nothing prints `--`.
  def text()
    if self.current == nil
      return "--"
    end
    return str(self.current)
  end

  # Up/Down step the value. A nullable setting's nil sits one step below the lowest value: Up from
  # it enters the floor, Down at the floor clears back to nil, and Down at nil stays.
  def nudge(dir)
    if self.nullable
      var floor = self.low()
      if self.current == nil
        if dir < 0
          return
        end
        self.current = floor
        return
      end
      if dir < 0 && self.current == floor
        self.current = nil
        return
      end
    end
    self.current = self.number(self.hold(self.within(self.current + self.step_of(dir))))
  end

  # What one keypress adds, in the unit the kind holds its value in.
  def step_of(dir)
    return dir * self.step
  end

  # The value brought inside the bounds it steps between: a step past one lands on it.
  def within(value)
    if self.min != nil && value < self.min
      return self.min
    end
    if self.max != nil && value > self.max
      return self.max
    end
    return value
  end

  # The value as the kind's own rule holds it - seconds as whole seconds, a pressure through
  # vacuum.clamp(). A plain number is what its bounds already made of it.
  def hold(value)
    return value
  end

  # The floor a nullable value steps up from: the lowest bound when there is one, nothing below zero.
  def low()
    if self.min != nil
      return self.min
    end
    return self.number(0.0)
  end

  # A number as the kind holds it: a whole-number setting takes the whole of it, so its value, its
  # step and its bounds stay the numbers the declaration and the keys agree on; a real one is left
  # as it is.
  def number(value)
    if value == nil || !self.whole
      return value
    end
    return int(value)
  end
end
