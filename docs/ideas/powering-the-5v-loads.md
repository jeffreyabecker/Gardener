# How should the pump and valve be powered off the USB 5 V rail?

The 5 V build's two loads are now a 6 V diaphragm pump (rated under 400 mA, 2–3.2 LPM, under
420 mmHg) and a 5–6 V normally-closed solenoid valve, driven by the same DRV8833 module and fed
from the PC's USB port. The parts list in
[poc-parts.md](../../guides/poc-parts.md) feeds that driver from a 3.3 V buck because its pump is a
3.7 V winding and its valve is 3.0–5.0 V; these two parts are 5–6 V, so the rail the list builds no
longer reaches them. The question is what rail should, and what keeping the pump and the valve out
of the sensor's ground costs.

## The options

**5 V direct.** The driver's VM is the USB rail itself. The valve runs at its rated voltage; the
pump runs at 83 % of its 6 V rating, so it moves less air and pulls less vacuum — maybe 60–80 % of
its rated flow and around 40–48 kPa of the ~56 kPa it is rated for, though both are extrapolations
the seller does not publish. Nothing extra is bought or built; the cost is the pump's ceiling and
the whole rig's appetite landing on one USB port.

**6 V boost.** A small boost converter between the USB rail and VM. The pump runs at its rated
voltage and the valve at the top of its 5–6 V range. The cost is a switching converter beside an
analog sensor whose output moves with its rail, and a USB input current that rises by roughly the
step-up ratio — the port budget the 5 V rail already sits on gets tighter, not looser.

**3.3 V buck.** The documented build's rail. It does not reach these parts: the valve is rated
5–6 V and a 6 V pump on 3.3 V is a fan. Ruled out by the parts' minimum coil voltage, not by taste.

**A separate supply.** A wall adapter or bench rail for the loads alone. It removes every USB-port
and surge constraint at once, but it breaks the build's one-cable shape and adds a second thing to
plug in. Held in reserve rather than taken.

## What has to be true for 5 V direct

The DRV8833 has one VM, so the pump and the valve share a rail whatever it is; 5 V is the only
single rail both parts are inside, and it is already in the cable. What it demands:

- The USB socket matters. The two loads together want roughly half an amp to eight tenths of an amp,
  with a surge over an amp when the pump starts. That fits a USB-C or USB 3 port and exceeds a plain
  USB 2.0 port's 500 mA, so the socket is part of the decision.
- A 1000 µF bulk capacitor at VM absorbs the surge and the solenoid's kickback locally. The DRV8833's
  body diodes are the flyback path, so no diodes are added — but that only holds if the driver is a
  DRV8833; a discrete H-bridge needs its own.
- The pump's 5 V ceiling becomes the rig's ceiling. The firmware already reports the pump's reach as
  the floor and pins the duty there, so nothing breaks — a deep target simply cannot be reached. The
  open question is whether 40–48 kPa is deep enough for the runs anyone will set.
- The sensor's ground must not carry the loads' return. It is a star ground, not a second ground:
  the pump and valve return through the driver's fat wire to the USB ground, the sensor ties
  directly to the MCU's ground with a short wire twisted with its output, and the two paths meet
  only at the one point. Nothing is floated — the sensor has to share the ADC's reference — so the
  aim is no shared wire segment rather than no shared network.

## What is still unknown

- The valve's coil current is not published, so the final combined draw is a guess until it is
  measured.
- The pump's actual depth at 5 V is not published; 40–48 kPa is an extrapolation from the 6 V
  rating.
- The valve is normally closed, which reverses the fail-safe the spec assumes: de-energised, the
  chamber seals instead of venting. That is a question of its own and this document does not settle
  it — either the valve is swapped for a normally-open one or the spec's fail-safe direction is
  revisited.
- Whether the H-bridge in hand is the DRV8833 with its body diodes, or a discrete bridge that needs
  flyback diodes added.

## Recommendation

Feed the driver straight from the 5 V rail with the bulk capacitor and a USB-C or USB 3 port, and
keep the 6 V boost as the fallback if the pump's 5 V depth turns out too shallow. This is held
moderately: it is the cheapest path and both parts sit inside their ratings on it, but the depth and
the fail-safe direction are real unknowns that a bench reading resolves in minutes.
