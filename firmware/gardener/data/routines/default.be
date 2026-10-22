# default.be — the disconnected page: the chamber's depth and the target.
#
# The readout face is ReadoutChrome, and the gestures are registered declaratively on Page.

class ReadoutChrome
  var header
  var pressure
  var target_label
  var target_value

  def init()
    self.header = DynamicText()
    self.header.x = 0; self.header.y = 0; self.header.w = 116; self.header.h = 12

    self.pressure = PressureDisplay()
    self.pressure.x = 0; self.pressure.y = 12; self.pressure.w = 116; self.pressure.h = 24

    self.target_label = FixedText()
    self.target_label.value = "TGT"
    self.target_label.x = 0; self.target_label.y = 44; self.target_label.w = 40; self.target_label.h = 8

    self.target_value = PressureDisplay()
    self.target_value.x = 40; self.target_value.y = 44; self.target_value.w = 76; self.target_value.h = 8
  end

  def draw(s)
    self.header.draw(s.state)
    self.pressure.draw(s.sensor_ok ? s.kpa : nil)
    self.target_label.draw()
    if s.target > 0.0
      self.target_value.draw(s.target)
    else
      self.target_value.draw(nil)
    end
  end
end

class Readout : Page
  var chrome

  def init()
    super(self).init()
    self.chrome = ReadoutChrome()

    self.on("up", "chord", / -> routines.set_active("system"))
    self.on("ok", "short",  / -> self.start())
    self.on("back", ["short", "double"], / -> self.back())
    self.on("up", "double", / -> routines.set_active("browser"))
    self.on("down", "double", / -> routines.set_active("system"))
  end

  def activate()
    self.restore()
    self.draw()
  end

  def draw_page()
    self.chrome.draw(vacuum.state())
  end

  # Back is the page's one command: release the chamber. A fast double tap is classified as a
  # double rather than a short, and it means the same thing here.
  def back()
    vacuum.stop()
    vacuum.log("default: venting")
    return true
  end

  # The target outlives a boot as a device setting: the system settings page owns it, and this page
  # hands it to the machine whenever it becomes visible.
  def restore()
    var kpa = device.target()
    if kpa > 0.0
      vacuum.target(vacuum.clamp(kpa))
    end
  end

  # OK resumes the run, unless one is already going. The machine answers for itself: with no
  # target, no reading or a latched fault, start() refuses and nothing changes.
  def start()
    var s = vacuum.state()
    if s.state == "PULL" || s.state == "HOLD"
      return true
    end
    if !vacuum.start()
      vacuum.log("default: start refused")
    end
    return true
  end
end

routines.register("default", "Pressure readout", Readout(), true)
return Readout
