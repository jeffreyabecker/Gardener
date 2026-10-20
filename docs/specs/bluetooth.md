# Gardener Bluetooth

The radio is a second contract rather than the line protocol over a pipe, because a phone is a GATT
client and not a terminal: the surface is a table of characteristics rather than lines of text. It
shares the ownership record with the serial line, so one link controls the machine at a time
whichever transport it arrived on.

The device advertises as `gardener` with one service, `6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e0a10`. The
last 16 bits of a characteristic's UUID select the field, so `state` is
`6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e0001`.

## Characteristics

| Characteristic | Properties | Payload |
| --- | --- | --- |
| `state` | read, notify | uint8: 0 idle, 1 pull, 2 hold, 3 vent, 4 fault |
| `kpa` | read, notify | float32 LE, depth below ambient |
| `target` | read, write, notify | float32 LE, depth to pull to; 0 clears; a shallower value while running vents to it |
| `duty` | read, notify | float32 LE, pump duty in percent |
| `fault` | read, notify | UTF-8 code, empty when none |
| `batt` | read, notify | uint8 percent, `0xFF` when no gauge is fitted |
| `command` | write, notify | write an opcode; notify the result |
| `fw` | read | UTF-8 version |
| `ble_proto` | read | uint8, this contract's revision |
| `unit` | read, write | uint8, 0 kPa, 1 psi, 2 inHg, 3 mmHg |
| `control` | read, write, notify | uint8: 1 owned by this link, 0 not |
| `sensor` | read, notify | uint8: 1 when a reading can be trusted |
| `floor` | read, notify | float32 LE, deepest target worth offering |

## Reading, writing and owning

Connecting is read-only: every value can be read without pairing, and the page does not change.
Writing `1` to `control` acquires the machine — the point where commands are accepted and the
panel switches to the Bluetooth rune — and writing `0` is the graceful exit, which stops, vents and
gives the page back.

Disconnecting while owning changes nothing: the run continues and the ownership record stays held,
so a reconnected phone is a reader until Back+OK for 5 s clears the record.

The writable characteristics require an encrypted link. The device pairs Just Works and keeps the
bond in NVS, so reads stay open to a scanner while nothing can command the machine until it pairs.

## Commands

`command` takes `0x01` start, `0x02` stop and vent, `0x03` reset a latched fault, and `0x04`
reboot, and notifies `0x00` applied, `0x01` no target, `0x02` no valid reading, `0x03` fault
latched, `0x04` unknown opcode or `0x05` not controlling.

`kpa`, `target` and `floor` are always kPa: `unit` says what the panel prints, not what the numbers
mean.

## Rates

`kpa` and `duty` are notified at 5 Hz, `batt` every 10 s, and the rest on change.

The stored preference in `/settings.ini` (`bluetooth=off`) leaves the radio silent. It is read once
at boot, so a change takes effect on the next `device reboot`.
