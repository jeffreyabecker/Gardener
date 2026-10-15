# hold_vacuum.be - pull down to a target and hold it there.
#
# Runs on the Eggplan Vacuum Berry VM. Available module: vacuum
# (see the README for the full API surface).

var target = 400      # hPa absolute

vacuum.log("hold_vacuum: target " + str(target) + " hPa")

while true
    var p = vacuum.pressure()
    vacuum.display_line(0, "hold " + str(target) + " hPa")
    vacuum.display_line(1, "now  " + str(p, 0) + " hPa")

    if p > target + 20
        vacuum.pump(70)
    elif p < target - 20
        vacuum.pump(0)
    end
    vacuum.delay(100)
end
