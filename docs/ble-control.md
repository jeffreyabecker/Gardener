# BLE control

Status: design. Nothing here is implemented; the firmware today has one transport, the serial link.

A phone should be able to watch and drive the device over Bluetooth Low Energy. This is
deliberately **not** the serial protocol: a phone speaks GATT, so the machine is exposed as
characteristics — typed values with directions and notifications — rather than as command lines.

## 1. What is in scope

* One peripheral, **one central at a time**, advertising while nothing is connected.
* The machine's readout as readable, notifying characteristics, and its commands as writes.
* A **separate contract from the serial protocol**, not a second encoding of it (§2).
* Both transports live at once. USB serial stays the bench interface and keeps its own rules.

Not in scope: OTA/DFU, a byte pipe carrying lines (that is what serial is for), push of diagnostic
text in the first version, and more than one simultaneous central.

## 2. Why a different protocol

The serial surface is built for a terminal: a line grammar, a terminator a host can move, text
items, and `help` as the discovery mechanism. A BLE client is an app with a GATT table: it binds to
characteristics, reads typed values, subscribes to the ones it wants, and writes a value or an
opcode.

Carrying the line protocol over GATT would mean framing bytes into characteristics, reassembling
across writes, an MTU story, and a text parser in every client — while giving up the two things
GATT is good at: a typed read per field and a subscription per field.

The cost of a second contract is real, but it is paid once, in the mapping table in §4. Both
surfaces read the same `AppContext`, so there is still one machine and no second copy of state.

## 3. Safety model: presence is not a session

The serial rule is that a controller that goes away releases the chamber (Q4): USB CDC presence is
a session, and the port is the controller.

**BLE has no such rule. A central can connect without venting, and can disconnect without venting.**

* Connecting changes nothing about the machine. Connecting during a run leaves the run alone.
* Disconnecting changes nothing either. A run started over BLE continues while the phone is out of
  range or backgrounded, which is the normal life of a phone app.
* The serial controller's rule is untouched: if a USB host is attached and then leaves, the device
  still stops and vents, whether or not a phone is connected.
* What ends a run is an explicit `stop`, a fault, or the serial controller leaving. That is the same
  set as today, minus the BLE radio.

The trade is deliberate: a phone is a viewer and a control surface, not the owner of the run. If a
lease is ever wanted — "stop if nobody has refreshed me in N seconds" — it should be an explicit
characteristic the app writes, not a property of the connection, so the behaviour is visible in the
contract instead of implied by it.

### 3.1 The screen, which is a different question

The firmware chooses its page from the link: `App::screenFor()` returns `ConnectedUI` while a
controller is attached and `DisconnectedUI` while none is, and today "attached" means
`serialAttached()`.

BLE makes that a two-source question. **A connected central is a viewer even though it is not an
owner**, so the switch has to become `serialAttached() || ble_->connected()` — otherwise a phone can
subscribe to every value and watch a live run while the panel in front of the operator still shows
the boot mark, and the device looks dead while it is being driven.

The two rules must not be conflated, because they answer different questions:

| Question | Sources | Effect |
| --- | --- | --- |
| Is anyone watching? | serial attached **or** a central connected | which page is drawn |
| Has the controller gone away? | serial presence only | whether the run is released (Q4) |
| Does a BLE disconnect stop a run? | — | no (§3) |

So `BleLink` owes the loop a `connected()` the screen can be chosen from, and `screenFor()` takes
both flags rather than reaching for `Serial` itself. A change on either transport repaints; both
pages are cheap to repaint and neither carries state that a swap would lose.

There is a subtle failure worth naming: because BLE connects without venting, a central can appear
during an unattended run. The screen switching to the readout is correct — something is now looking
at a machine that is already working — and it must not be mistaken for a session starting, because
nothing about the run changed when the radio connected.

## 4. The GATT contract

One primary service with a project-owned 128-bit base UUID. The last 16 bits select the
characteristic, so the table below is the whole contract.

Base: `6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e0a10`

| Characteristic | UUID suffix | Properties | Payload |
| --- | --- | --- | --- |
| `state` | `...0001` | read, notify | uint8: 0 idle, 1 pull, 2 hold, 3 vent, 4 fault |
| `kpa` | `...0002` | read, notify | float32 LE: vacuum depth below ambient |
| `target` | `...0003` | read, write, notify | float32 LE: depth to pull to; 0 means no target |
| `duty` | `...0004` | read, notify | float32 LE: pump duty, percent |
| `fault` | `...0005` | read, notify | UTF-8: `latch`, `sensor`, or empty for none |
| `batt` | `...0006` | read, notify | uint8: percent, or `0xFF` when no gauge is fitted |
| `command` | `...0007` | write, notify | write uint8 opcode; notify uint8 result |
| `fw` | `...0008` | read | UTF-8: firmware version |
| `ble_proto` | `...0009` | read | uint8: this contract's revision, `1` |
| `unit` | `...000A` | read, write | uint8: what the panel prints pressure in — 0 kPa, 1 psi, 2 inHg, 3 mmHg |

Encodings are little-endian because the part is; a float is IEEE-754, four bytes. `fw` and `fault`
are the only text — one is a version and the other is a small closed set of codes. The `unit`
values are the `DisplayUnit` order in `core/Units.h`, so the characteristic and the serial
`device unit=` cannot disagree about which number means which unit.

**The pressures stay in kPa whatever `unit` says.** `kpa` and `target` are the machine's own
number, exactly as they are on the serial link; `unit` is what the glass prints, which is a
preference about the display and not a second encoding of the same reading. A client that wants to
show PSI converts locally with the same table, or reads `unit` so its readout matches the panel.

Opcodes written to `command`:

| Opcode | Action | Result on success |
| --- | --- | --- |
| `0x01` | start | `0x00` |
| `0x02` | stop (stop and vent, as `vacuum stop`) | `0x00` |
| `0x03` | reset a latched fault | `0x00` |
| `0x04` | reboot | `0x00`, then the link drops |

Results notified on `command`:

| Result | Meaning |
| --- | --- |
| `0x00` | applied |
| `0x01` | no target set |
| `0x02` | no valid pressure reading |
| `0x03` | a fault is latched; reset first |
| `0x04` | unknown opcode |

`start` reports the same three refusals the serial link reports, from the same
`VacuumSystem::startRefusal()`, so the two surfaces cannot disagree about why a run will not start.

### 4.1 Rates

| Characteristic | Notified |
| --- | --- |
| `state`, `fault`, `target` | on change |
| `kpa`, `duty` | every 200 ms while subscribed (5 Hz) |
| `batt` | every 10 s while subscribed |
| `command` | on every opcode, with its result |

`unit` is read and written but never notified: one central at a time means the client that writes
it already knows, and a notify would only echo its own write back.

5 Hz is the radio's main cost and the only rate worth arguing about; it is also about as fast as a
phone screen needs. A client that wants less simply does not subscribe.

### 4.2 What is deliberately absent

`heap`, `chip`, `up`, `sensor`, `floor`, `panel w/h`, `log`, `term` and `attached` are bench and
protocol concerns, not things a phone app shows or does. They stay on serial. In particular there
is no `link` resource and no terminator, because there are no lines.

The display unit is the one device setting that crosses over, because it is not a bench concern:
an app that shows a reading wants to show it the way the panel does. It still does not move the
values — see §4.

## 5. Where it sits in the code

`app/BleLink.*` — app layer, not `hal/`. BLE is not a device the instrument commands; it is another
way to command the instrument, which is what `SerialLink` is.

```cpp
// App.h
AppContext context_;
SerialLink* link_;    // serial, unchanged, keeps Q4
BleLink* ble_;        // own contract; no link-loss rule
UserInterface* ui_;   // screenFor(serialAttached() || ble_->connected())
```

* `BleLink` holds the `AppContext*`, the NimBLE server, the characteristics, and a snapshot.
* `BleLink::begin()` starts NimBLE, builds the service, sets the initial snapshot, and advertises.
* `BleLink::poll(nowMs)` applies a pending command, refreshes the snapshot, and sends the
  notifications that are due. It is called from `App::update()` beside `link_->poll(now)`.
* `App::begin()` calls `ble_->begin()` after `Serial.begin`; a BLE failure is a log line, never an
  abort, because serial still works.

| Characteristic | Read from |
| --- | --- |
| `state` | `vacuum().runState()` mapped to the byte in §4 |
| `kpa` | `vacuum().currentKpa()` |
| `target` | `vacuum().targetKpa()` / `vacuum().setTarget()` / `clearTarget()` on 0 |
| `duty` | `vacuum().pump().dutyPercent()` |
| `fault` | `vacuum().faultCode()` |
| `batt` | `battery().percent()`, `0xFF` when `!present()` |
| `unit` | `settings().unit()` / `settings().setUnit()` — the same pair the serial `device unit=` uses |
| `command` | `startRefusal()`, `start()`, `stop()`, `resetFault()`, `ESP.restart()` |

While `Settings` has no store, a `unit` write is accepted and dropped exactly as it is on serial,
so a read after the write reports what the panel is still printing. That behaviour is the
`Settings` file's, not this transport's, and it changes in one place when a store arrives.

## 6. Concurrency

NimBLE runs its host on **core 0**; the Arduino `loop()` runs on core 1. GATT callbacks are on the
host task, so they must not touch the machine: `VacuumSystem` is loop-owned and a read callback has
to answer immediately.

| Shared thing | Writer | Reader | Mechanism |
| --- | --- | --- | --- |
| Value snapshot (`state`, `kpa`, `duty`, `fault`, `batt`) | `loop()` in `poll()` | GATT read callbacks | struct of plain fields, replaced whole |
| Display unit (`unit`) | `loop()` in `poll()`, when a write arrived | GATT read callback | one enum byte, read as a plain load; it is a setting, so it lives outside the snapshot |
| Pending command (opcode or target) | GATT write callbacks | `loop()` in `poll()` | one-slot mailbox with a flag |
| Connection state | NimBLE | both | the server's connected count |

This is simpler than the serial case because the contract is message-shaped: there is no byte ring,
no framing state and no re-assembly. A write lands in the mailbox; the loop drains it next pass; the
read callback serves the last snapshot the loop published.

`setValue` and `notify` are called from the loop task, which NimBLE serialises onto its host task.
Nothing is notified from inside a write callback, and no callback calls into `VacuumSystem`.

## 7. Security

A serial cable is physical presence; a radio is not. Starting a pump is a physical act, so an open
GATT server is not the shipped default.

Recommended shape:

1. **Pairing with a passkey from the panel.** The device already draws a splash and a battery;
   while pairing it draws six digits and the controller types them. That is
   `BLE_HS_IO_DISPLAY_ONLY`, LE Secure Connections, bonded, with the bond stored in NVS (NimBLE's
   `CONFIG_BT_NIMBLE_NVS_PERSIST` is on by default).
2. The writable characteristics — `target`, `command` and `unit` — require encryption
   (`NIMBLE_PROPERTY::WRITE_ENC`), so an unencrypted write is rejected by the stack and never
   reaches the mailbox. Reads may stay open: pressure and state are not secrets.
3. Just Works bonding is the fallback if a passkey proves annoying on a bench. It authenticates
   against a casual neighbour, not against an attacker with a radio.

The panel is device-owned for the seconds a passkey is on it, the same exception as the splash and
the battery.

## 8. What a controller sees

* The GATT connection is the presence; there is no `attached` characteristic to read.
* Advertising stops on connect and restarts on disconnect.
* `ble_proto` and `fw` are the handshake. A client that does not recognise `ble_proto` should stop
  rather than guess, exactly as a serial controller does with `proto`.
* Every value is readable at any time, so a freshly connected app builds its whole view from reads
  before it subscribes to anything.
* A pressure is always kPa, whether the panel is showing kPa, psi, inHg or mmHg. `unit` says what
  the glass prints; it never changes what `kpa` or `target` mean.
* Connecting puts the readout on the panel and disconnecting returns it to the mark (§3.1), even
  though neither touches the run.

## 9. Build and configuration

`nimconfig.h` in the sketch (or the equivalent `--build-property` flags) tightens the defaults:

| Define | Value | Why |
| --- | --- | --- |
| `CONFIG_BT_NIMBLE_ROLE_CENTRAL` | `0` | peripheral only; saves flash and RAM |
| `CONFIG_BT_NIMBLE_ROLE_OBSERVER` | `0` | no scanning |
| `CONFIG_BT_NIMBLE_ROLE_BROADCASTER` | `1` | advertising is how a controller finds us |
| `CONFIG_BT_NIMBLE_ROLE_PERIPHERAL` | `1` | the connection |
| `CONFIG_BT_NIMBLE_MAX_CONNECTIONS` | `1` | one controller, less RAM |
| `CONFIG_BT_NIMBLE_NVS_PERSIST` | `1` | bonds survive a reboot (already the default) |
| `CONFIG_BT_NIMBLE_PINNED_TO_CORE` | `0` | stays off the loop's core (already the default) |

`CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU` can stay at its 255 default: every characteristic here is at
most nine bytes, so nothing depends on a negotiated MTU.

`scripts/arduino.ps1 -Task libs` gains `'NimBLE-Arduino'`, and `sketch.yaml` names the version so a
build is reproducible. NimBLE-Arduino 2.5.1 is the version this design was written against.

## 10. What to measure before believing any of it

* Flash, static RAM and boot heap against today's 384 kB / 19.8 kB / the `heap` line at boot.
* `BleLink::poll()` cost on the loop when idle and at 5 Hz notification.
* Write-to-notify latency for `target` and `command`.
* The safety claim: start a run from BLE, disconnect the phone, confirm the run continues and the
  pump state is unchanged.
* The screen switch: with USB unplugged, a BLE connection puts the readout on the panel, and the
  panel goes back to the mark when the phone disconnects — including when that lands mid-run, where
  the run must keep going while the page changes.
* Pair, reboot, reconnect without re-pairing.
* A serial session and a BLE session at once: neither disturbs the other, and a serial detach still
  stops and vents.

## 11. Failure modes and what the device does

| Failure | Behaviour |
| --- | --- |
| NimBLE init or server start fails | log it, keep serial working; BLE is a bonus transport, not a boot dependency |
| Unknown opcode | result `0x04`, no state change |
| `start` refused | the matching result code, no state change |
| Target written below the loop's floor | rejected at the GATT layer; the bound is the `CONTROL_MIN_TARGET_KPA` in `core/Config.h`, and 0 clears the target |
| `unit` written outside 0–3 | rejected at the GATT layer; the value is a closed set, so nothing is guessed |
| Central disconnects mid-run | the run continues (§3); advertising restarts |
| Central not subscribed | values stay readable; nothing is queued |
| Second central tries to connect | rejected by `MAX_CONNECTIONS=1` while the first is connected |
| Unpaired write to `target` or `command` | rejected by the stack; the mailbox never sees it |
| `reboot` opcode | result notified, then the device restarts and the central sees a disconnect |

## 12. Phases

1. **P0 — read-only.** Service, `state`/`kpa`/`duty`/`batt`/`unit`/`fw`/`ble_proto`, the snapshot
   and the notification rates, and `BleLink::connected()` feeding `screenFor()` so the panel shows
   the readout for a phone with no USB attached (§3.1). Prove a phone shows live values and that a
   serial session is unaffected.
2. **P1 — control.** `target` writes, `unit` writes, the command mailbox and opcodes, result
   notifications, and the `startRefusal()` mapping.
3. **P2 — security.** Passkey on the panel, bonding, encryption on the three writable
   characteristics (`target`, `command`, `unit`).
4. **P3 — documentation and budget.** README gains the interface bullet and the measured numbers;
   `sketch.yaml` and the library list pin the version.

## 13. Open questions

* Is `batt` as a percentage enough, or does an app want pack volts too (`batt_mv`, uint16)?
* Should `sensor` presence and the `floor` bound be characteristics, so a client can grey out
  controls instead of discovering limits by rejection? That argues for a small `limits` set.
* Do we want a `log` notify characteristic, or does that reintroduce the line protocol through the
  back door? The failure codes in §4 already carry the operational truth.
* Is a lease worth having for BLE-started runs (§3), or is an unattended run over BLE acceptable
  because the pump's own limits and the fault latch still apply?
* Should `unit` be the device's preference or the app's own formatting choice? Today it is the
  device's, because the panel has no settings UI of its own and a host is the only thing that can
  change it — but an app that already formats numbers may not want to move the glass.
* Should reads also require encryption, for privacy rather than control?