# hold_vacuum.be - pull down to a target and hold it there.
#
# Runs on the Eggplan Vacuum Berry VM. Available module: vacuum
# (see the README for the full API surface).

var target = 40.0     # kPa absolute

vacuum.log("hold_vacuum: target " + vacuum.fmt(target, 1) + " kPa")

while true
    var p = vacuum.pressure()
    vacuum.display_line(0, "hold " + vacuum.fmt(target, 1) + " kPa")
    vacuum.display_line(1, "now  " + vacuum.fmt(p, 1) + " kPa")

    if p > target + 2.0
        vacuum.pump(70)
    elif p < target - 2.0
        vacuum.pump(0)
    end
    vacuum.delay(100)
end
