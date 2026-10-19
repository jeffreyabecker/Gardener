# BLE control

Status: implemented. The firmware has two transports, the serial link and this GATT table, and they
live at once and share the ownership record documented in
[serial-protocol.md](../specs/serial-protocol.md) §5. The notes at the end of this document record
where the build differs from the design; the bench checklist in §10 has not been run yet.

A phone should be able to watch and drive the device over Bluetooth Low Energy. This is
deliberately **not** the serial protocol: a phone speaks GATT, so the machine is exposed as
characteristics — typed values with directions and notifications — rather than as command lines.

## 1. What is in scope

* One peripheral, **one central at a time**, advertising while nothing is connected.
* The machine's readout as readable, notifying characteristics, and its commands as writes.
* A **separate contract from the serial protocol**, not a second encoding of it (§2).
* Both transports live at once, and share the control lifecycle: one ownership record, acquire and
  release, and a drop that leaves ownership held.

Not in scope: OTA/DFU, a byte pipe carrying lines (that is what serial is for), push of diagnostic
text in the first version, and more than one simultaneous central.

## 2. Why a different protocol

The serial surface is built for a terminal: a domain-first line grammar, a fixed `LF` terminator,
text `key=value` replies, and this document as the discovery mechanism. A BLE client is an app with
a GATT table: it binds to characteristics, reads typed values, subscribes to the ones it wants, and
writes a value or an opcode.

Carrying the line protocol over GATT would mean framing bytes into characteristics, reassembling
across writes, an MTU story, and a text parser in every client — while giving up the two things
GATT is good at: a typed read per field and a subscription per field.

The cost of a second contract is real, but it is paid once, in the mapping table in §4. Both
surfaces read the same `AppContext`, so there is still one machine and no second copy of state.

## 3. Safety model: connection is not control

The lifecycle is explicit and shared with serial: connect, assume control, command, release. For
BLE that means:

* **Connecting is read-only.** It changes nothing about the machine, the run or the panel.
* **Assuming control is an explicit write** to the `control` characteristic. Only then are commands
  accepted and the panel shows `ConnectedUI`.
* **Disconnecting while controlling does nothing.** No stop, no vent, no reboot, and ownership
  stays held; the run continues under the last commands.
* **A write of `0` to `control` is the graceful exit**: it stops and vents, clears ownership, and
  does not reboot.
* **A reconnected central is read-only.** The held ownership makes its acquire busy until the
  local reset gesture clears it.
* **Disconnecting while only connected changes nothing.** A read-only observer can come and go
  without touching a run.
* **One link owns the device at a time.** A central that connects while serial owns it can read,
  but its acquire is refused as busy.
* **A controller that stays connected but stops responding keeps control.** There is no idle lease
  in this design; the local reset gesture (Back+OK for 5 s) is the escape.

### 3.1 The screen follows ownership

The page is chosen from the one ownership record, not from `serialAttached()` or
`ble_->connected()`. `App::update()` calls `screenFor(ownership.held())`; `ConnectedUI` is what the
owner sees — including a dropped owner, whose record stays held — while `DisconnectedUI` is what
everyone else sees, including a central that is connected but has not acquired control.

The overlay's serial and BLE indicators stay presence-only: they answer “is a link there?”, not
“is a link driving?”. A connection or disconnection that does not change ownership does not repaint
the page.

Acquiring control mid-run does not change the machine: the controller is taking authority, not
starting a run, and it should read `vacuum get` before issuing commands. The panel switching to
that link's icon is then a truthful statement that someone now owns the machine.

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
| `fault` | `...0005` | read, notify | UTF-8: the latched fault code — `sensor` today — or empty for none |
| `batt` | `...0006` | read, notify | uint8: percent, or `0xFF` when no gauge is fitted |
| `command` | `...0007` | write, notify | write uint8 opcode; notify uint8 result |
| `fw` | `...0008` | read | UTF-8: firmware version |
| `ble_proto` | `...0009` | read | uint8: this contract's revision, `1` |
| `unit` | `...000A` | read, write | uint8: what the panel prints pressure in — 0 kPa, 1 psi, 2 inHg, 3 mmHg |
| `control` | `...000B` | read, write, notify | uint8: 0 not owned by this link, 1 owned by this link. Write 1 to acquire, 0 to release |
| `sensor` | `...000C` | read, notify | uint8: 1 when a plausible pressure reading is available, 0 otherwise |
| `floor` | `...000D` | read, notify | float32 LE: deepest reachable target, kPa |

Encodings are little-endian because the part is; a float is IEEE-754, four bytes. `fw` and `fault`
are the only text — one is a version and the other is a small closed set of codes. The `unit`
values are the `DisplayUnit` order in `core/Units.h`, so the characteristic and the serial
`device set unit` command cannot disagree about which number means which unit. The unit is the
device's preference, not the app's: the BLE write and the serial command move the same stored
value, so the panel, `device get` and every client see the same unit. An app that wants different
formatting converts locally.

Reads are open at the GATT layer; only the writes require encryption (§7).

`control` is the explicit toggle from §3: a write of `1` asks to own the device, a write of `0`
releases it, and the notify reports the result. It is the caller's view, so a link that is only
connected reads `0` even when another link owns the device; its acquire is refused as busy. While
it reads `0` the link is a read-only observer, and every write other than `control` is refused;
`control` and the read-only characteristics are the only things it may touch.

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
| `0x05` | not controlling; acquire first |

`start` reports the same three refusals the serial link reports, from the same
`VacuumSystem::startRefusal()`, so the two surfaces cannot disagree about why a run will not start.

### 4.1 Rates

| Characteristic | Notified |
| --- | --- |
| `state`, `fault`, `target` | on change |
| `kpa`, `duty` | every 200 ms while subscribed (5 Hz) |
| `batt` | every 10 s while subscribed |
| `command` | on every opcode, with its result |
| `control` | on acquire or release, including a refused acquire |
| `sensor`, `floor` | on change |

`unit` is read and written but never notified: one central at a time means the client that writes
it already knows, and a notify would only echo its own write back.

5 Hz is the radio's main cost and the only rate worth arguing about; it is also about as fast as a
phone screen needs. A client that wants less simply does not subscribe.

### 4.2 What is deliberately absent

`chip`, `heap`, `up`, `batt_v`, `display-size` and `log` are bench and protocol concerns, not
things a phone app shows or does. They stay on serial. In particular there is no `link` domain and
no terminator control, because there are no lines, and no log notify characteristic: diagnostics
are a bench tool, and `fault` carries the operational truth.

The battery crosses over as a percentage only: `batt` is `percent()`, and `batt_v` stays on serial
because a phone showing charge does not need a second number for the same pack.

`sensor` and `floor` are characteristics rather than `command` results because a client needs them
before it offers a control: `sensor` says whether a reading can be trusted, and `floor` is the
deepest target worth offering. Both are read and notified so a control can grey out when the
situation changes.

The display unit is the one device setting that crosses over, because it is not a bench concern:
an app that shows a reading wants to show it the way the panel does. It still does not move the
values — see §4.

## 5. Where it sits in the code

`app/ble/BleLink.*` — app layer, not `hal/`, in its own folder beside `app/serial/`. BLE is not a
device the instrument commands; it is another way to command the instrument, which is what
`SerialLink` is. A transport is a family of files — the link, its domain or GATT table, its
payloads — so it gets a folder the way serial has one (`SerialLink`, `Domains/`), rather than
sitting loose in `app/` next to the machine.

```cpp
// App.h
#include "app/ble/BleLink.h"

AppContext context_;
SerialLink* link_;      // serial, unchanged, keeps Q4
BleLink* ble_;          // own contract; no link-loss rule
UserInterface* ui_;     // the screen for the ownership record
bool hostAttached_;     // serial presence as the loop last saw it
```

* `BleLink` holds the `AppContext*`, the NimBLE server, the characteristics, and the pending slots
  a write lands in.
* `BleLink::begin()` starts NimBLE, builds the service and advertises.
* `BleLink::poll(nowMs)` applies whatever is pending, then sends the notifications that are due. It
  is called from `App::update()` beside `link_->poll(now)`.
* `App::update()` feeds `ble_->connected()` to `overlay().setConnections()` for the presence
  indicator, and chooses the page from the ownership record. It does not feed `consumeLinkLost()`
  directly: a BLE disconnect while controlling leaves that record held (§3), and only the local
  reset gesture clears it.
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
| `sensor` | `vacuum().sensor().present() && vacuum().sensor().readingValid()` as 0/1 |
| `floor` | `cfg::CONTROL_FLOOR_KPA` |
| `unit` | `settings().unit()` / `settings().setUnit()` — the same pair the serial `device set unit` command uses |
| `control` | the ownership record's `owned()`, `acquire()` and `release()` |
| `command` | `startRefusal()`, `start()`, `stop()`, `resetFault()`, the safe reboot |

`unit`, `target` and `command` are gated on ownership just like the serial non-read verbs: while
`control` reads `0`, their writes are refused and the value does not move.

`Settings` stores the unit as `unit=<token>` in `/settings.ini` and restores it at boot, so a `unit`
write from BLE persists exactly as `device set unit` does. A filesystem that will not take the
write still leaves the unit in force for the session; that behaviour is the `Settings` file's, not
this transport's.

The radio preference is already stored, as `bluetooth=on|off` (default `on`) in the same section,
read through `settings().bluetoothEnabled()` and edited on the panel's `system` page. Nothing reads
it in this build; `BleLink::begin()` is what has to honour it and stay silent when it is off.

## 6. Concurrency

NimBLE runs its host on **core 0**; the Arduino `loop()` runs on core 1. The model is deliberately
loose: the alternative is a lock or a queue, and neither buys anything at this size.

**Status reads are assumed safe.** A GATT read callback answers from the machine as it is —
`currentKpa()`, `runState()`, `dutyPercent()`, `battery().percent()`, `settings().unit()` and the
ownership record — with no snapshot and no second copy to keep in step. A float or an enum is one aligned load on this part, so
a reader that overlaps an update sees one side of it or the other. No reading in this contract means
anything finer than a loop iteration, so that is enough.

**Writes set a flag and the loop applies them.** A GATT write callback stores the value in the slot
for its characteristic and returns without touching the machine. `BleLink::poll()` applies whatever
is pending at the top of its service call, before it reads anything for a notification, so a
notified value is always one the machine actually has.

What that costs, stated plainly:

* A write takes effect on the next loop iteration, not inside the callback.
* Two writes to one characteristic before the loop runs leave the last one standing; there is no
  queue and no promise to apply the first.
* A refused write — a target below the floor, a unit outside the closed set, an opcode that cannot
  run now, a write while not owning, or an acquire while another link owns the device — is refused
  by the loop. The answer is the characteristic's unchanged value plus, for `command`, a result
  notification and, for `control`, a notification of the value still in force.

| Shared thing | Writer | Reader | Mechanism |
| --- | --- | --- | --- |
| Machine status (`state`, `kpa`, `duty`, `fault`, `batt`), the display unit and ownership | `loop()` | GATT read callbacks | read live; no snapshot |
| Pending target, unit, opcode or control | GATT write callbacks | `loop()` at the top of `poll()` | one slot per characteristic, last write wins |
| Connection state | NimBLE | both | the server's connected count |

There is no mutex, no ring and no mailbox queue. A slot is a plain value and a `has` flag, written on
one core and cleared on the other; the failure mode is a write landing one iteration late, which the
contract already allows. If a later characteristic needs read-modify-write consistency, that is the
point to reach for an atomic — not before.

`setValue` and `notify` are called from the loop task, which NimBLE serialises onto its host task.
Nothing is notified from inside a write callback, and no write callback calls into `VacuumSystem`; a
read callback only ever reads.

## 7. Security

A serial cable is physical presence; a radio is not. Starting a pump is a physical act, so an open
GATT server is not the shipped default.

**Just Works, bonded, nothing on the panel.** The device pairs without a passkey and keeps the bond
in NVS (NimBLE's `CONFIG_BT_NIMBLE_NVS_PERSIST` is on by default). This authenticates against a
casual neighbour at the bench, not against an attacker with a radio — that is the price of a pairing
flow someone will actually complete.

The writable characteristics — `control`, `target`, `command` and `unit` — require encryption
(`NIMBLE_PROPERTY::WRITE_ENC`), so an unencrypted write is rejected by the stack and never reaches
the slot.

**Reads stay open.** None of the read characteristics is a secret: pressure, state, duty, battery,
fault, unit, sensor and floor are all bench-visible values. Leaving reads unencrypted lets a phone
or a scanner observe without pairing, while control stays behind the encrypted write set. A client
that wants privacy can still pair; the link is encrypted then, but pairing is not a precondition
for reading.

A passkey was the earlier shape and is deliberately gone: it is the uncommon flow on embedded
devices, it needs a screen to show digits and a person to type them, and it would put six of them on
a panel whose only job is showing a pressure. If stronger pairing is ever wanted, the display-only
flow is the thing to add — a pairing callback and one page, not a change to this contract.

## 8. What a controller sees

* The GATT connection is presence; `control` is ownership. There is no `attached` characteristic
  to read.
* Advertising stops on connect and restarts on disconnect.
* `ble_proto` and `fw` are the handshake. A client that does not recognise `ble_proto` should stop
  rather than guess, exactly as a serial controller does with `proto`.
* Every value is readable at any time, with or without pairing, so a freshly connected app builds
  its whole view from reads before it subscribes to anything. In particular, `sensor` and `floor`
  are readable before control is acquired, so a client can grey out start and target controls
  instead of discovering the limits by rejection.
* A pressure is always kPa, whether the panel is showing kPa, psi, inHg or mmHg. `unit` says what
  the glass prints; it never changes what `kpa` or `target` mean.
* Acquiring control puts the link's icon on the panel; releasing stops and vents and returns it to
  the mark. A drop while owning changes nothing, so the run continues and the panel stays on that
  icon until the local reset gesture. Connecting alone does not change the page (§3.1).

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

* Flash, static RAM and boot heap against today's 574 kB / 20.0 kB / the `heap` line at boot.
* `BleLink::poll()` cost on the loop when idle and at 5 Hz notification.
* Write-to-notify latency for `target` and `command`, including the one-iteration deferral the
  concurrency model adds.
* The safety claim: start a run while controlling, disconnect the phone, confirm the run continues
  unchanged and the panel stays on the link's icon; then use the local reset gesture and confirm
  it stops, vents and reboots. Repeat with an explicit `control=0` write and confirm the release stops
  and vents without a reboot. Then run a chamber from serial or the local page and confirm a
  connected-but-not-controlling phone leaves it untouched.
* The screen switch: with USB unplugged, a BLE connection alone leaves the disconnected page while
  lighting the overlay's radio indicator. Acquiring control puts the Bluetooth rune on the panel;
  releasing stops and vents and returns both. A drop while owning changes neither the run nor the
  page; the local reset gesture is what returns to the mark.
* Read every characteristic without pairing. Then attempt `control=1` and a `target` write and
  confirm the stack rejects both until the link is encrypted.
* Pair, reboot, reconnect without re-pairing.
* A serial session and a BLE session at once: neither disturbs the other, and a serial drop while
  owning leaves the run and ownership held until the local reset gesture.

## 11. Failure modes and what the device does

| Failure | Behaviour |
| --- | --- |
| NimBLE init or server start fails | log it, keep serial working; BLE is a bonus transport, not a boot dependency |
| Unknown opcode | result `0x04`, no state change |
| `start` refused | the matching result code, no state change |
| Target written below the loop's floor | latched, then refused when the loop applies it; the target does not move, so the next read is the answer. The bound is `CONTROL_MIN_TARGET_KPA` in `core/Config.h`, and 0 clears the target |
| `unit` written outside 0–3 | latched, then refused when the loop applies it; the value is a closed set and the unit does not move |
| Central disconnects mid-run | if it was controlling, nothing changes and ownership stays held; if it was only connected, nothing changes (§3); advertising restarts, and a reconnected central is read-only until the local reset gesture |
| Central not subscribed | values stay readable; nothing is queued |
| Second central tries to connect | rejected by `MAX_CONNECTIONS=1` while the first is connected |
| Unpaired write to any writable characteristic | rejected by the stack; the slot never sees it |
| `reboot` opcode | result notified, then the device restarts and the central sees a disconnect |

## 12. Phases

All four phases are in this build.

1. **P0 — read-only.** Service, `state`/`kpa`/`duty`/`batt`/`sensor`/`floor`/`unit`/`fw`/
   `ble_proto`, the notification rates, and `BleLink::connected()` feeding the overlay's radio
   indicator. Prove a phone shows live values and that a serial session is unaffected; the panel
   stays on the disconnected page because P0 does not acquire control.
2. **P1 — control.** The ownership record, `control` writes, the pending slots, `target` and `unit`
   writes, opcodes and their result notifications, the `startRefusal()` mapping,
   `screenFor(control.owned())`, and the drop path that leaves ownership held.
3. **P2 — security.** Just Works bonding and encryption on the writable characteristics
   (`control`, `target`, `command`, `unit`).
4. **P3 — documentation and budget.** README gains the interface bullet and the measured numbers;
   `sketch.yaml` and the library list pin the version.

## 13. Where the build differs from this design

* The characteristic UUIDs are the service UUID with its last 16 bits replaced, so `state` is
  `6b1d0000-9a3e-4c6f-8f2a-1b7c5d9e0001` and the base `...9e0a10` is the service alone.
* The compile options are `--build-property build.defines=...` in `scripts/arduino.ps1`, not a
  `nimconfig.h` in the sketch: the library's own `nimconfig.h` is `#pragma once` and cannot be
  shadowed from the sketch folder, while `build.defines` reaches every compiled file including the
  stack's own sources. `-Task libs` pins NimBLE-Arduino 2.5.1.
* A central's subscriptions are tracked, and the first pass after a subscribe sends the current
  value of every on-change field, so a client starts from the machine's state rather than waiting
  for the first thing that moves. The rates are unchanged.
* `describe()` is the one place a characteristic's value is built, so a read and a notification of
  the same field cannot disagree.
* A central's departure is applied by `BleLink::poll()` calling `Ownership::drop()` rather than by
  `App`, so the loop stays the only place that touches the record and the panel still needs no
  link-loss rule of its own.
