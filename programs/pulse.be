# pulse.be - cycle the pump on and off while logging pressure.

vacuum.pump(0)
for cycle : 1 .. 10
    vacuum.log("cycle " + str(cycle))
    vacuum.pump(80)
    vacuum.delay(3000)
    vacuum.pump(0)
    vacuum.delay(3000)
    vacuum.log("  pressure " + vacuum.fmt(vacuum.pressure(), 1) + " kPa")
end
vacuum.log("pulse: done")
