# Gardener Bluetooth

The radio is a second contract rather than the line protocol over a pipe, because a phone is a GATT
client and not a terminal: the surface is a table of characteristics rather than lines of text. It
shares the ownership stack with the serial line, so one link drives the machine at a time whichever
transport it arrived on, with the panel beneath both when a panel is fitted.

The device advertises as `gardener`, with the name in the scan response beside the service
`6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e0a10`. Advertising stops on connect and resumes when the central
leaves. The last 16 bits of a characteristic's UUID select the field, so `state` is
`6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e0001`.

## Characteristics

| Characteristic | Properties | Payload |
| --- | --- | --- |
| `state` | read, notify | uint8: 0 idle, 1 pull, 2 hold, 3 vent, 4 fault |
| `kpa` | read, notify | float32 LE, depth below ambient |
| `target` | read, write, notify | float32 LE, depth to pull to; 0 clears; a shallower value while running vents to it |
| `duty` | read, notify | float32 LE, pump duty in percent |
| `fault` | read, notify | UTF-8 code, empty when none, at most 12 bytes |
| `batt` | read, notify | uint8 percent, `0xFF` when no gauge is fitted |
| `command` | write, notify | write an opcode; notify the result |
| `fw` | read | UTF-8 version, at most 16 bytes |
| `ble_proto` | read | uint8, this contract's revision: `1` |
| `unit` | read, write | uint8, 0 kPa, 1 psi, 2 inHg, 3 mmHg |
| `control` | read, write, notify | uint8: 0 none, 1 serial, 2 bluetooth, 3 panel — who is on top of the stack |
| `sensor` | read, notify | uint8: 1 when a reading can be trusted |
| `floor` | read, write, notify | float32 LE, the pump's measured reach, deepest target worth offering |
| `caps` | read | uint8 bitmask: 1 panel, 2 battery |
| `flow` | read, write, notify | float32 LE, the pump's projected free flow in L/min; the gains follow |
| `volume` | read, write, notify | float32 LE, the chamber volume in litres; the gains follow |
| `valve` | read, write | uint8: 0 normally open, non-zero normally closed — the fitted part's direction; a write applies on the next boot |
| `pump_max` | read, write, notify | float32 LE, the duty cap the loop clamps to, at most 90 |
| `deadband` | read, write, notify | float32 LE, the target-reached tolerance |

`batt` and `unit` are the panel's: `batt` is the gauge the panel carries and `unit` is the
preference its pages print in. A build without a panel has neither characteristic, so a client
finds `caps` with the panel bit clear and no `batt` or `unit` to read. Every other characteristic is
the machine's and is always present.

`kpa`, `target` and `floor` are always kPa: `unit` says what the panel prints, not what the numbers
mean.

## Reading, writing and owning

Connecting is read-only: every value can be read without pairing, and the page does not change.
Writing anything other than `0` to `control` acquires the machine — the point where commands are
accepted and the panel switches to the Bluetooth rune — and writing `0` is the graceful exit, which
stops and vents and gives the page back. A claim always succeeds, and it vents: the claimant takes
the top of the stack and the link beneath is set aside. The write is answered with the owner that
resulted, so a client does not have to read `control` back to learn whether it took.

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

`command` takes `0x01` start, `0x02` stop and vent, `0x03` reset a latched fault, and `0x04`
reboot, and notifies `0x00` applied, `0x01` no target, `0x02` no valid reading, `0x03` fault
latched, `0x04` unknown opcode or `0x05` not controlling. The result is notified after every write,
whatever it is, and is the only value `command` carries: the characteristic has no read.

`start` refuses for the same three reasons the serial line reports — no target, no valid reading
and a latched fault — because both transports ask the same check, so the two surfaces cannot
disagree about why a run will not start.

## Notifications

A notification goes only to a central that has subscribed to that characteristic. After a
subscription the link sends the current value of every on-change field once, so a client starts from
the machine's state rather than from the first thing that moves.

`kpa` and `duty` are notified at 5 Hz while a central is connected, and `batt` every 10 s. Every
other notifying characteristic is sent when its value changes, with `command` sent after every
opcode and `control` after every write to it.

## Build

The radio is a build fact, not a preference. A full-device image may be compiled with
`GARDENER_BLUETOOTH=0` (`python scripts/arduino.py --task build --no-enable-bluetooth`), which
leaves this GATT table, the NimBLE stack and the `bluetooth` link out of the image entirely: such a
build has one transport, and `control` never reports `bluetooth`. The machine-alone build always
keeps the radio.
