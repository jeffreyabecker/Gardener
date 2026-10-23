# Gardener Bluetooth

The radio is a second contract rather than the line protocol over a pipe, because a phone is a GATT
client and not a terminal: the surface is a table of characteristics rather than lines of text. A
bonded central may write at any time, and the serial line or the panel may command the machine at
the same time: the last write to reach the control task wins.

The device advertises as `gardener`, with the name in the scan response beside the service
`6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e0a10`. Advertising stops on connect and resumes when the central
leaves. The last 16 bits of a characteristic's UUID select the field, so `ble_proto` is
`6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e0001`: the contract's own revision is the first field a client can
read, before it trusts the rest of the table.

## Characteristics at a glance

| Characteristic | Suffix | Properties | Length | Payload |
| --- | --- | --- | --- | --- |
| `ble_proto` | `0001` | read | 1 | uint8, this contract's revision |
| `pressure_kpa` | `0002` | read, notify | 4 | float32 LE, depth below ambient |
| `target` | `0003` | read, write, notify | 4 | float32 LE, depth to pull to |
| `duty` | `0004` | read, notify | 4 | float32 LE, pump duty in percent |
| `fault` | `0005` | read, notify | 12 | UTF-8 code, empty when none |
| `batt` | `0006` | read, notify | 1 | uint8 percent, `0xFF` when the gauge is not reporting |
| `command` | `0007` | write, notify | 1 | an opcode in, a result out |
| `fw` | `0008` | read | 16 | UTF-8 version |
| `state` | `0009` | read, notify | 1 | uint8 run state |
| `unit` | `000A` | read, write | 1 | uint8 display unit |
| `sensor` | `000C` | read, notify | 1 | uint8, 1 when a reading can be trusted |
| `floor` | `000D` | read, notify | 4 | float32 LE, the pump's measured reach |
| `caps` | `000E` | read | 1 | uint8 capability bitmask |
| `flow` | `000F` | read, notify | 4 | float32 LE, the pump's projected free flow |
| `volume` | `0010` | read, notify | 4 | float32 LE, the chamber volume |
| `valve` | `0011` | read | 1 | uint8, the fitted valve's direction |
| `pump_max` | `0012` | read, notify | 4 | float32 LE, the duty cap |
| `deadband` | `0013` | read, notify | 4 | float32 LE, the target-reached tolerance |
| `hold_s` | `0014` | read, write, notify | 2 | uint16 LE, the hold to ask for, and the one granted |
| `max_run_s` | `0015` | read | 2 | uint16 LE, the longest hold a start may ask for |
| `write_result` | `0016` | read, notify | 3 | uint16 LE suffix, then a uint8 reason |
| `uptime_ms` | `0017` | read, notify | 4 | uint32 LE, milliseconds since boot |

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
code a lost reading raises, `vent_timeout` a release that never reached the vent floor, `no_progress`
a pull driven hard without headway, and `overshoot` a depth that ran past its target; a fault raised
without a code reads `unknown`. A code stays until the `0x03` opcode clears it, and while it is set
`state` stays `fault`.

### `batt` (panel builds only)

The pack gauge in percent: a straight line between the configured empty and full volts, and no more
accurate than that. `0xFF` when no gauge is reporting, which is how a panel whose sense pin did not
come up reads — `caps` still says the build carries the gauge, because that is a fact about the
wiring.

### `uptime_ms`

The device's uptime in milliseconds, a `uint32` that advances across notifications. It is how a
client places a `state`, a `fault` or a command result in time, and how it tells a reconnect from a
restart.

## Driving the machine

### `target`

The depth below ambient to pull to, in kPa. `0` is the ambient target: it holds the chamber
released, so writing it stops the pump and vents, and it is what the characteristic reads before
anything else is set.

While a run is active, a deeper target re-enters `pull` and the pump works to it; a shallower one
stops the pump, opens the valve and vents to the new target before the run carries on. A target
deeper than `floor` is refused — the pump cannot reach it — and the read-back stays where it was, so
`write_result` is the only other notice. A negative or non-finite value is refused the same way.

### `hold_s` and `max_run_s`

`max_run_s` is the compiled maximum, `CONTROL_MAX_RUN_MS` in whole seconds; nothing may exceed it.
`hold_s` is a write of the hold to ask for, in whole seconds, applied to the next `start` — `0`
asks for the maximum — and a read of the hold granted to the run, `0` while nothing is running. A
value above `max_run_s` is refused and reported through `write_result`.

### `command`

Write-only: one byte is an opcode, and the notification that follows carries the result. Every
opcode is answered, applied or refused; the table is under Commands.

## Parameters

The seven settings that configure the machine and its panel. They are compile-time defaults,
restored from `/settings.cfg`, and changed with `device set` on the serial line — the radio reads
them and never writes them, so a bonded phone cannot move the bound the machine is held to. A
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
the two numbers the PID gains are derived from, so a different pump is retuned by setting this and
`volume`, never by changing the firmware. Held to 0.05–10 L/min by the loader and the setter alike.

### `volume`

The chamber volume in litres, the other number the gains are derived from. Held to 0.005–5 L.

### `floor`

The pump's measured reach in kPa: how deep this rig can pull, and the deepest target accepted. A
target deeper than it is refused. Held to 5–100 kPa, and written only on the serial line, because it
is the bound the machine holds a controller to.

### `pump_max`

The ceiling in percent that the control loop clamps its duty to. `PUMP_DUTY_MAX_PERCENT` is also the
pump driver's own cap, so a larger value would mean nothing. Held above zero and at most 90.

### `deadband`

The tolerance in kPa within which the loop calls the target reached and `state` becomes `hold`. It
is the one thing a client cannot work out for itself, and the reason `hold` is the honest cue that
the target has arrived. Held to 0.05–2 kPa.

### `valve`

The direction of the fitted solenoid: `0` normally open, so a de-energised coil vents; non-zero
normally closed. It names a part rather than tuning the loop, so `device set valve` stores the
choice for the next boot and the characteristic keeps reporting the part that is fitted until then.
It is read-only over the radio: a phone may know which way the valve fails, not change it.

### `unit` (panel builds only)

`0` kPa, `1` psi, `2` inHg, `3` mmHg: the unit the panel prints pressure in. It changes the glass
and nothing on the wire, so `pressure_kpa`, `target` and `floor` stay kPa whatever it says. Refused
above `3`.

## Identity and capability

### `fw`

The firmware version as text, at most 16 bytes: the same string the serial line reports as `fw`, and
constant for the life of the image.

### `ble_proto`

The revision of this contract, as one byte: `1`. It is the table's first field, at `0001`, so a
client reads the revision before it trusts anything else. A client compares it against the revision
it was written for and treats a mismatch as a table it does not know.

### `caps`

A bitmask of what the build carries: `1` a panel is fitted, `2` the build has a battery gauge, `4` the
fitted valve is fail-safe — normally open, so a de-energised coil vents and the chamber cannot hold
pressure through a crash, a cut wire or a dead coil. `batt` and `unit` exist only when the panel bit
is set, so a machine-alone build reads `4`. The gauge bit says the wiring is there, not that the part
answered — `batt` is where that shows.

## Reading and writing

Reading is open: every characteristic with a read answers without pairing. Writing requires an
encrypted link. The device pairs Just Works and keeps the bond in NVS, so reads stay open to a
scanner while nothing can write until it pairs.

There is no claim to make. A bonded central may write whenever it is connected, whatever the serial
line or the panel is doing, and a disconnect stops nothing: a run continues to its hold and the
machine's own deadline.

## Writes

A write is applied on the loop's next pass, not in the radio's callback, so a read immediately
after a write may still show the previous value. The value the characteristic next reports, and the
notification that carries it, is what the machine holds, and that read-back is the answer to a
refused write: the value is not echoed, and nothing is notified unless it changed.

| Characteristic | Accepted | Refused |
| --- | --- | --- |
| `target` | `0` releases; anything from `0` to `floor`, in kPa | deeper than `floor`, negative, or not finite |
| `hold_s` | `0` to `max_run_s`, whole seconds | above `max_run_s` |
| `unit` | `0` to `3` | above `3` |

`floor`, `flow`, `volume`, `pump_max`, `deadband` and `valve` have no write: they are commissioning
and tuning values, set with `device set` on the serial line. A payload of the wrong length is
ignored rather than truncated.

A refused `target` or `hold_s` is also notified on `write_result`: the field's suffix, least
significant byte first, then a reason byte — `1` out of range, `2` not a number. The read-back stays
the authority; the notification is how a client tells a refusal from a lost write.

## Commands

`command` carries one byte and has no read: a write is an opcode, and the notification that follows
is the result, sent after every write whatever it is.

| Opcode | Meaning |
| --- | --- |
| `0x01` | start: close the valve, then run the stored target for `hold_s` |
| `0x02` | stop and vent |
| `0x03` | reset a latched fault |
| `0x04` | reboot |

| Result | Meaning |
| --- | --- |
| `0x00` | applied |
| `0x02` | no valid reading |
| `0x03` | fault latched |
| `0xFF` | unknown opcode |

`0x00`, `0x02` and `0x03` are the machine answering; `0xFF` is the protocol refusing the request
itself. `0x01` was "no target to pull to" and is retired: a start is valid at a target of `0`, so it
is never produced.

`start` refuses for the same two reasons the serial line reports — no valid reading and a latched
fault — because both transports ask the same check, so the two surfaces cannot disagree about why a
run will not start. The run's hold is the last `hold_s` written, or the maximum when it was `0`.

## Notifications

A notification goes only to a central that has subscribed to that characteristic. After a
subscription the link sends the current value of every on-change field once, so a client starts from
the machine's state rather than from the first thing that moves.

`pressure_kpa`, `duty` and `uptime_ms` are notified at 5 Hz while a central is connected, and `batt`
every 10 s. Every other notifying characteristic is sent when its value changes, with `command` sent
after every opcode and `write_result` after every refused target or hold write.

`uptime_ms` is how a client places a notification in time and tells a reconnect apart from a
restart.

The periodic fields are not part of the subscription baseline, so a client that wants one of them
before its next tick reads it once after subscribing.

## Build

The radio is a build fact, not a preference. A full-device image may be compiled with
`GARDENER_BLUETOOTH=0` (`python scripts/arduino.py --task build --no-enable-bluetooth`), which
leaves this GATT table, the NimBLE stack and the `bluetooth` transport out of the image entirely:
such a build has one transport. The machine-alone build always keeps the radio.
