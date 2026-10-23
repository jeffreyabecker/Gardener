# Gardener Bluetooth

The radio is a second contract rather than the line protocol over a pipe, because a phone is a GATT
client and not a terminal: the surface is a table of characteristics rather than lines of text. It
shares the ownership stack with the serial line, so one link drives the machine at a time whichever
transport it arrived on, with the panel beneath both when a panel is fitted.

The device advertises as `gardener`, with the name in the scan response beside the service
`6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e0a10`. Advertising stops on connect and resumes when the central
leaves. The last 16 bits of a characteristic's UUID select the field, so `state` is
`6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e0001`.

## Characteristics at a glance

| Characteristic | Suffix | Properties | Length | Payload |
| --- | --- | --- | --- | --- |
| `state` | `0001` | read, notify | 1 | uint8 run state |
| `pressure_kpa` | `0002` | read, notify | 4 | float32 LE, depth below ambient |
| `target` | `0003` | read, write, notify | 4 | float32 LE, depth to pull to |
| `duty` | `0004` | read, notify | 4 | float32 LE, pump duty in percent |
| `fault` | `0005` | read, notify | 12 | UTF-8 code, empty when none |
| `batt` | `0006` | read, notify | 1 | uint8 percent, `0xFF` when the gauge is not reporting |
| `command` | `0007` | write, notify | 1 | an opcode in, a result out |
| `fw` | `0008` | read | 16 | UTF-8 version |
| `ble_proto` | `0009` | read | 1 | uint8, this contract's revision |
| `unit` | `000A` | read, write | 1 | uint8 display unit |
| `control` | `000B` | read, write, notify | 1 | uint8, who is on top of the stack |
| `sensor` | `000C` | read, notify | 1 | uint8, 1 when a reading can be trusted |
| `floor` | `000D` | read, write, notify | 4 | float32 LE, the pump's measured reach |
| `caps` | `000E` | read | 1 | uint8 capability bitmask |
| `flow` | `000F` | read, write, notify | 4 | float32 LE, the pump's projected free flow |
| `volume` | `0010` | read, write, notify | 4 | float32 LE, the chamber volume |
| `valve` | `0011` | read, write | 1 | uint8, the fitted valve's direction |
| `pump_max` | `0012` | read, write, notify | 4 | float32 LE, the duty cap |
| `deadband` | `0013` | read, write, notify | 4 | float32 LE, the target-reached tolerance |

The suffix is the last 16 bits of the characteristic's UUID, and the length is the characteristic's
own maximum. A read answers at most that many bytes; a write has to be exactly the payload's length,
and a short or long one is ignored rather than padded or truncated.

`batt` and `unit` are the panel's: `batt` is the gauge the panel carries and `unit` is the
preference its pages print in. A build without a panel has neither characteristic, so a client
finds `caps` with the panel bit clear and no `batt` or `unit` to read. Every other characteristic is
the machine's and is always present.

`pressure_kpa`, `target` and `floor` are always kPa: `unit` says what the panel prints, not what the
numbers mean.

## What the machine reports

These carry what the machine knows: the run state, the reading it regulates on, the pump's output,
whether the sensor can be trusted, the fault latch, and the panel's pack.

### `state`

The run state of the machine, as one byte: the vocabulary is [vacuum control](./vacuum-control.md)'s,
and the serial line reports the same state in `state=`.

| Value | Name | What the machine is doing |
| --- | --- | --- |
| `0` | idle | the valve is open and the pump stopped; the chamber is released, or was never pulled |
| `1` | pull | the valve is closed and the pump is working toward the target |
| `2` | hold | the loop has called the target reached — within `deadband` on a pull, or a vent's lead short of it — and keeps regulating to stay there |
| `3` | vent | the pump is stopped and the valve open, so the depth falls back toward ambient: a release, or a vent to a shallower target |
| `4` | fault | a fault is latched; the pump is cut, the valve is open, and only the `0x03` opcode clears it |

### `pressure_kpa`

The depth below ambient in kPa — zero at ambient and rising as the chamber is pulled down, which is
the frame every target and every reach is stated in. It is the same number the serial line reports
as `pressure_kpa`, and it reads zero while no reading can be trusted, which `sensor` distinguishes
from a chamber at ambient.

### `duty`

The pump drive in percent, bounded by `pump_max`, after the driver's slew limit: what the part is
being driven at now rather than what the loop last asked for. There is no write: the pump is driven
by the loop alone.

### `sensor`

`1` when the pressure sensor is responding and its reading is plausible, `0` when it is not. A run
needs `1` to start, and a run that sees it fall to `0` latches a fault.

### `fault`

The latched fault's code as text, empty when nothing is latched, at most 12 bytes. `sensor` is the
code a lost reading raises; a fault raised without a code reads `unknown`. A code stays until the
`0x03` opcode clears it, and while it is set `state` stays `fault`.

### `batt` (panel builds only)

The pack gauge in percent: a straight line between the configured empty and full volts, and no more
accurate than that. `0xFF` when no gauge is reporting, which is how a panel whose sense pin did not
come up reads — `caps` still says the build carries the gauge, because that is a fact about the
wiring.

## Driving the machine

### `target`

The depth below ambient to pull to, in kPa. `0` means no target: it is how the characteristic reads
before one is set, and writing `0` clears one, which releases the chamber if a run is active.

While a run is active, a deeper target re-enters `pull` and the pump works to it; a shallower one
stops the pump, opens the valve and vents to the new target before the run carries on. A target
deeper than `floor` is accepted and only pins the duty at `pump_max` — the pump cannot reach it, and
nothing refuses the attempt. A target shallower than `CONTROL_MIN_TARGET_KPA` (5.0 kPa) is refused.

### `command`

Write-only: one byte is an opcode, and the notification that follows carries the result. Every
opcode is answered, applied or refused; the table is under Commands.

### `control`

Who is on top of the ownership stack, as one byte: `0` none, `1` serial, `2` bluetooth, `3` panel.
Reading it says who is driving; writing a non-zero value claims the machine for the writing link and
writing `0` gives it back. The claim is the point where commands start being accepted, and it always
succeeds and vents the chamber.

## Parameters

The seven settings that configure the machine and its panel. They are compile-time defaults,
restored from `/settings.cfg`, and overridable over the wire by the link that owns the machine. A
stored value that is missing, unparseable or out of bounds keeps the default at boot.

| Parameter | Unit | Default | What it changes |
| --- | --- | --- | --- |
| `flow` | L/min | `PUMP_FLOW_LPM`, 1.0 | the pump's projected free-flow rate; with `volume`, the loop's gains |
| `volume` | L | `CHAMBER_VOLUME_L`, 0.05 | the chamber volume; the other half of the gains |
| `floor` | kPa | `CONTROL_FLOOR_KPA`, 60.0 | the pump's measured reach, the deepest target worth offering |
| `pump_max` | percent | `PUMP_DUTY_MAX_PERCENT`, 90.0 | the duty cap the loop clamps to |
| `deadband` | kPa | `CONTROL_DEADBAND_KPA`, 0.2 | the target-reached tolerance |
| `valve` | `0` or `1` | `0`, normally open | the fitted valve's direction; applies on the next boot |
| `unit` | `0`–`3` | `0`, kPa | the unit the panel prints pressure in |

Every one of them except `valve` takes effect on the next pass of the loop or of the panel.
`flow`, `volume`, `floor`, `pump_max` and `deadband` notify when the applied value changes; `unit`
and `valve` do not, so the read-back is their only answer.

### `flow`

The pump's projected free-flow rate: what the fitted pump moves into an open chamber. It is one of
the two numbers the PID gains are derived from, so a different pump is retuned by writing this and
`volume`, never by changing the firmware. Refused at zero, negative or not finite.

### `volume`

The chamber volume in litres, the other number the gains are derived from. Refused at zero, negative
or not finite.

### `floor`

The pump's measured reach in kPa: how deep this rig can pull. It does not clamp a target — a deeper
one is accepted — it is what a target's useful range is measured against, and the deepest value a
client should offer. Refused shallower than `CONTROL_MIN_TARGET_KPA`.

### `pump_max`

The ceiling in percent that the control loop clamps its duty to. `PUMP_DUTY_MAX_PERCENT` is also the
pump driver's own cap, so a larger value would mean nothing. Refused at zero, above 90, or not
finite.

### `deadband`

The tolerance in kPa within which the loop calls the target reached and `state` becomes `hold`. It
is the one thing a client cannot work out for itself, and the reason `hold` is the honest cue that
the target has arrived. Refused at zero, negative or not finite.

### `valve`

The direction of the fitted solenoid: `0` normally open, so a de-energised coil vents; non-zero
normally closed. It names a part rather than tuning the loop, so a write stores the choice for the
next boot and the characteristic keeps reporting the part that is fitted until then.

### `unit` (panel builds only)

`0` kPa, `1` psi, `2` inHg, `3` mmHg: the unit the panel prints pressure in. It changes the glass
and nothing on the wire, so `pressure_kpa`, `target` and `floor` stay kPa whatever it says. Refused
above `3`.

## Identity and capability

### `fw`

The firmware version as text, at most 16 bytes: the same string the serial line reports as `fw`, and
constant for the life of the image.

### `ble_proto`

The revision of this contract, as one byte: `1`. A client compares it against the revision it was
written for and treats a mismatch as a table it does not know.

### `caps`

A bitmask of what the build carries: `1` a panel is fitted, `2` the build has a battery gauge. `batt`
and `unit` exist only when the panel bit is set, so a machine-alone build reads `0`. The gauge bit
says the wiring is there, not that the part answered — `batt` is where that shows.

## Reading, writing and owning

Connecting is read-only: every characteristic with a read answers without pairing, and the page
does not change. Writing anything other than `0` to `control` acquires the machine — the point where
commands are accepted and the panel switches to the Bluetooth rune — and writing `0` is the graceful
exit, which stops and vents and gives the page back. A claim always succeeds, and it vents: the
claimant takes the top of the stack and the link beneath is set aside. The write is answered with
the owner that resulted, so a client does not have to read `control` back to learn whether it took.

Disconnecting removes the link from the stack, so a run does not outlive its owner: the chamber
vents and control falls to the link beneath — the panel, which resumes or idles, or `none`, which
vents. A phone that disconnects while another link drives leaves the stack when it reaches the top.
A reconnected phone is a reader until it claims again.

The writable characteristics require an encrypted link. The device pairs Just Works and keeps the
bond in NVS, so reads stay open to a scanner while nothing can command the machine until it pairs.

## Writes

A write is applied on the loop's next pass, not in the radio's callback, so a read immediately
after a write may still show the previous value. The value the characteristic next reports, and the
notification that carries it, is what the machine holds, and that read-back is the answer to a
refused write: the value is not echoed, and nothing is notified unless it changed.

| Characteristic | Accepted | Refused |
| --- | --- | --- |
| `target` | `0` clears; `5.0` or deeper, in kPa | shallower than `5.0`, or not finite |
| `floor` | `5.0` or deeper, in kPa | shallower, or not finite |
| `flow` | greater than `0`, L/min | zero, negative, or not finite |
| `volume` | greater than `0`, litres | zero, negative, or not finite |
| `pump_max` | greater than `0` and at most `90`, percent | outside those bounds, or not finite |
| `deadband` | greater than `0`, kPa | zero, negative, or not finite |
| `unit` | `0` to `3` | above `3` |
| `valve` | `0` normally open, non-zero normally closed | — |
| `control` | any uint8: non-zero claims, `0` releases | — |

The target and floor bounds are the loop's shallowest target, `CONTROL_MIN_TARGET_KPA`, and the
`pump_max` bound is `PUMP_DUTY_MAX_PERCENT`. A payload of the wrong length is ignored rather than
truncated.

Every field except `command` and `control` is gated on control as well: a value written by a link
that is not on top of the stack is dropped, and the read-back shows the unchanged setting.

## Commands

`command` carries one byte and has no read: a write is an opcode, and the notification that follows
is the result, sent after every write whatever it is.

| Opcode | Meaning |
| --- | --- |
| `0x01` | start: close the valve, then pull to the target |
| `0x02` | stop and vent |
| `0x03` | reset a latched fault |
| `0x04` | reboot |

| Result | Meaning |
| --- | --- |
| `0x00` | applied |
| `0x01` | no target to pull to |
| `0x02` | no valid reading |
| `0x03` | fault latched |
| `0xFE` | not controlling: the writing link is not on top of the stack |
| `0xFF` | unknown opcode |

`0x00`–`0x03` are the machine answering; `0xFE` and `0xFF` are the protocol refusing the request
itself.

`start` refuses for the same three reasons the serial line reports — no target, no valid reading
and a latched fault — because both transports ask the same check, so the two surfaces cannot
disagree about why a run will not start.

## Notifications

A notification goes only to a central that has subscribed to that characteristic. After a
subscription the link sends the current value of every on-change field once, so a client starts from
the machine's state rather than from the first thing that moves.

`pressure_kpa` and `duty` are notified at 5 Hz while a central is connected, and `batt` every 10 s.
Every other notifying characteristic is sent when its value changes, with `command` sent after every
opcode and `control` after every write to it.

The three periodic fields are not part of the subscription baseline, so a client that wants one of
them before its next tick reads it once after subscribing.

## Build

The radio is a build fact, not a preference. A full-device image may be compiled with
`GARDENER_BLUETOOTH=0` (`python scripts/arduino.py --task build --no-enable-bluetooth`), which
leaves this GATT table, the NimBLE stack and the `bluetooth` link out of the image entirely: such a
build has one transport, and `control` never reports `bluetooth`. The machine-alone build always
keeps the radio.
