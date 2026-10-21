# The panel owns the machine

The point of making the panel a link is not the ownership record — it is that the machine stops
assuming hardware. The core machine is architected around the pressure loop alone: the pump and the
sensor, and the vent valve that releases them. Everything else — buttons, display, buzzer, battery,
storage, settings, routines — is the panel, and the panel reaches the machine the way serial and
Bluetooth do: as a link over the one ownership record. Buttons are panel UI, not machine input, and
the control task reads no keys. Control itself is a stack: whoever is on top drives, the panel sits
beneath every external link as the default controller, and `None` at the bottom vents. The machine
holds pressure only while the link on top has it running.

## What has to be true

1. **The machine knows only the pressure loop.** The machine's context is the pump, the sensor, the
   vent valve, the control law, the ownership record and the command surface. It has no buttons,
   display, buzzer, battery, storage, settings or routines, and its control task reads no keys.

2. **Buttons are the panel's.** The Back and Back+OK gestures are panel behaviour, not machine
   protections: the panel reads its keys and expresses each gesture as a command on its link. Back
   stops by releasing, the way `control release` does, and Back+OK is the reboot the wire already
   serves — `device reboot` on serial, `command 0x04` on Bluetooth — issued from the panel's side.

3. **Control is a stack.** The controller is whoever is on top. Boot pushes `None`; a panel build
   pushes the panel above it; serial and Bluetooth push themselves when they claim and are removed
   when they release or drop. A link commands only while it is on top.

4. **The panel is the default controller.** Because it is pushed at boot, the panel is in charge
   whenever no external link is above it. Its routines drive the machine then; a page that only
   reads or draws leaves the machine idle.

5. **`None` is the floor.** `None` sits at the bottom and is the controller when nothing is above
   it. Its only effect is to vent: the valve is open and nothing draws.

6. **Changing the top vents.** The chamber vents whenever the top of the stack changes — a claim, a
   release, or a drop — so a new controller always starts from an open chamber and the link beneath
   takes over only after the chamber is at rest. A vanished external link cannot leave the machine
   held, because it is no longer on the stack.

7. **The wire names the owner.** `control get` and the BLE `control` characteristic report who owns
   the machine — serial, bluetooth, panel, or none — where owning means on top of the stack
   ([Bluetooth](../specs/bluetooth.md)). This replaces today's "owned by this link, or not", which
   could not say who.

## What this changes

* A link that drops while controlling no longer leaves the run going. It is removed from the stack,
  the chamber vents, and control falls to the link beneath — the panel, which resumes or idles, or
  `None`, which vents. The stale-owner record goes with it: there is nothing left to reset, because
  a gone link is no longer on the stack.
* Acquiring is no longer refused, and a claim now vents. A link that claims while another controls
  takes the top and drives from an open chamber; the previous link is set aside, not rejected, and
  the `busy` reply disappears from `control acquire`.
* The device's own stop is the panel releasing, not a key the control task reads. The machine has no
  stop gesture; it has the link on top leaving.
* The ownership report changes from "owned by this link, or not" to naming the owner, so a controller
  can tell whether the machine is being driven by the panel or by another link.

## What the requirement leaves open

* How the glass says who is on top. The connected page stays the external-link icon; whether the
  device driving itself is the mark or something new is a page decision, not a requirement.
