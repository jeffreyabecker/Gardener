# default.be — live pressure readout with target editing (illustrative)

var editing = false
var tt = 0.0                 # value being edited, kPa
var last_ok_ms = 0
var last_back_ms = 0

def within(prev, now) return now - prev <= 350 end

def draw()
  var s = vacuum.state()
  screen.draw_clear()
  if editing
    screen.draw_text(0, 0, 1, 0, "EDIT")
    screen.draw_text(0, 16, 3, 0, string.format("%.1f", tt))
  else
    screen.draw_text(0, 0, 1, 0, s.state)
    screen.draw_text(0, 16, 3, 0, string.format("%.1f", s.kpa))
  end
  if s.target_active
    screen.draw_text(0, 40, 1, 0, "tgt " + string.format("%.1f", s.target))
  else
    screen.draw_text(0, 40, 1, 0, "tgt -")
  end
end

# OK (index 2): double-click enters edit mode; single click commits while editing
buttons.on(buttons.OK, def (kind)
  if kind != "click" return end
  var now = vacuum.now_ms()
  if editing
    vacuum.target(tt)        # commit and exit edit mode
    editing = false
  elif within(last_ok_ms, now)
    editing = true
    var s = vacuum.state()
    if s.target_active
      tt = s.target
    else
      tt = s.kpa             # seed from what's already set
    end
  end
  last_ok_ms = now
end)

# Up / Down (0 / 1): step tt by ±1 kPa while editing (click or hold-to-repeat)
buttons.on(buttons.UP, def (kind) if editing and (kind == "click" or kind == "repeat") tt += 1 end end)
buttons.on(buttons.DOWN, def (kind) if editing and (kind == "click" or kind == "repeat") tt -= 1 end end)

# Back (index 3): short = cancel edit; double = vent and clear target
buttons.on(buttons.BACK, def (kind)
  if kind != "click" return end
  var now = vacuum.now_ms()
  if within(last_back_ms, now)
    vacuum.target_off()      # "reset target to 0": firmware has no 0 kPa target, so clear it
    vacuum.stop()            # vent
    editing = false
  elif editing
    editing = false          # cancel without changing anything
  end
  last_back_ms = now
end)

while true
  draw()
  vacuum.delay(100)          # 10 Hz refresh; also the yield that lets stop land
end