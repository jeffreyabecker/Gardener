# A GATT link to the system control surface

A phone or a laptop beside the rig may have nothing the device can be reached over. Bluetooth needs
neither a network nor an address to find, and every phone is already the client.

What it asks is the surface, because the surface is one object. `SystemControl` in
[SystemControl.h](../../libraries/Gardener/src/machine/SystemControl.h) already answers a target, a
release, a latch's reset and the machine's reading, and already says why a set would be refused, so
this link is another way of asking rather than another way of deciding. This document is how that
surface goes over GATT with NimBLE: what the service is made of, how a value is encoded, who may
write it, where the code sits, and what would show it working.

Setting a network is not part of this. The link asks the machine and never configures the radio.

## What is there now, and where the edges are

`SystemControl` is the whole of the machine's surface: `status()` copies one instant, `set()`,
`vent()` and `reset()` post to the `CommandQueue` the control tick drains, and `refusal()` is why a
set would answer `NotReady`. It names outcomes and not words — `ServeResult`, `SystemStatus` — and
leaves the saying to the link.

What matters about where code sits is the build. The sketch's build takes `libraries/` from the
tree and nothing else ([arduino.py](../../scripts/arduino.py)), so a library it uses and the tree
does not hold comes out of the machine's own Arduino libraries: NimBLE-Arduino 2.5.1 is installed
there today, and nothing in the tree names it yet.

The host suite compiles every source under `libraries/Gardener/src` that does not mention `<FS.h>`,
`<Wire.h>`, `<SD.h>`, `freertos/` or `esp32-hal-`, and takes the sketch's headers on its include
path so a platform-free header beside the sketch can have cases of its own — which is how
[Form.h](../../firmware/gardener/Form.h) is tested by [form.cpp](../../tools/unit/cases/form.cpp).

## The shape of the change

Three files and a few call sites:

```
firmware/gardener/GattValues.h    the three packets, platform-free, inline
firmware/gardener/BleLink.h/.cpp  the service, its callbacks, the advertisement, the notify beat
tools/unit/cases/gatt.cpp         the packets, the code tables and the malformed writes
```

`GattValues.h` holds the encoding and nothing else: a status into a byte array, a byte array into a
command, a fault word into a code, a refusal word into a code. It includes `machine/SystemControl.h`
and names nothing of the board, so the suite compiles it and the cases can hold a table that a new
fault or a new state breaks. `BleLink` is in the sketch because every line of it that matters is
NimBLE — `NimBLEDevice`, `NimBLEServer`, `NimBLECharacteristic` — and the library stays board-free.

### The service

One primary service, three characteristics, all on one 128-bit base. The UUIDs are not SIG
assignments, and a self-chosen short one collides with the first phone that ships a service of the
same number, so every one here is 128 bits.

| what | UUID | properties | who writes it |
| --- | --- | --- | --- |
| service | `6d3a0000-6a3f-4d1c-9d5b-9e0f4c7a2b31` | — | — |
| status | `6d3a0001-6a3f-4d1c-9d5b-9e0f4c7a2b31` | read, notify | the link |
| command | `6d3a0002-6a3f-4d1c-9d5b-9e0f4c7a2b31` | write | the client |
| reply | `6d3a0003-6a3f-4d1c-9d5b-9e0f4c7a2b31` | read, notify | the link |

A command is a write with a response: the ATT acknowledgement is the only receipt the client gets
that a write landed, and a write without a response would leave the client guessing whether the
board heard it.

`get` is not a command. The status characteristic is readable: a read of it is the machine's
reading at the instant the stack answers.

### The values

Everything is little-endian, floats are IEEE-754 binary32, and the bytes are written field by field
rather than by reinterpreting a C struct, whose padding is the compiler's.

**Status — 18 bytes.** The largest value the link ever sends, so the whole service fits the default
20-byte ATT payload: no client has to negotiate an MTU, and nothing is ever truncated.

| offset | size | field |
| --- | --- | --- |
| 0 | 4 | `uptime_ms` — milliseconds since boot when the reading was copied |
| 4 | 4 | `pressure_kpa` — depth below ambient, 0 at ambient, larger as the chamber pulls down |
| 8 | 4 | `target` — depth below ambient, 0 is the ambient target |
| 12 | 4 | `duty_percent` |
| 16 | 1 | `state` — `RunState`: 0 Idle, 1 Pull, 2 Hold, 3 Vent, 4 Fault |
| 17 | 1 | `fault` — the code table below, 0 when nothing is latched |

**Command — 1 or 5 bytes.** Exactly one byte for the two commands that name nothing, exactly five
for the one that does, and a five-byte write naming vent or reset is `BadCommand` too, so each
command has one spelling. A write of any other length is the link's own `BadCommand` and nothing is
queued.

| offset | size | field |
| --- | --- | --- |
| 0 | 1 | `op` — 1 set, 2 vent, 3 reset |
| 1 | 4 | `target_kpa` — depth to pull to, only when `op` is 1 |

`set` calls `SystemControl::set` rather than a bare setpoint: the target and the run it asks for
are one command, a target of 0 releases and answers `Released` rather than starting anything, and a
refusal queues nothing.

**Reply — 2 bytes.** The outcome of the last command, notified once when the write is served and
readable afterwards.

| offset | size | field |
| --- | --- | --- |
| 0 | 1 | `result` — 0 Ok, 1 Released, 2 BelowRange, 3 AboveRange, 4 NotReady, 5 BadCommand |
| 1 | 1 | `refusal` — 0 none, 1 latch, 2 no_reading, 3 unknown |

The first five results are `ServeResult`'s enumerators written as their value, so a client that has
one table has both; `BadCommand` is the link's own and shares the byte because a client asking what
happened should not have to consult two tables to find out whether the machine or the link answered.

Two code tables, because the machine speaks words and this wire does not:

| fault | code | | refusal | code |
| --- | --- | --- | --- | --- |
| nothing latched | 0 | | nothing | 0 |
| `sensor` | 1 | | `latch` | 1 |
| `vent_timeout` | 2 | | `no_reading` | 2 |
| `overshoot` | 3 | | anything else | 3 |
| `no_progress` | 4 | | | |
| anything else | 5 | | | |

The words stay as they are, because the run log names them; the codes are this wire's, and both
tables live in `GattValues.h` beside the encoder so a case can walk every code.

### The advertisement

The device advertises as `Gardener-<suffix>`, the suffix being the low two bytes of the board's own
address, so two boards on a bench differ. The 128-bit service UUID goes in the advertising data and
the name in the scan response: flags, the UUID and the name together are 34 bytes and the
advertising packet holds 31.

One client at a time: the stack's connection capacity is one, so a second client is refused while
the first holds the link, and the server advertises again on disconnect — the board is findable
whenever nothing is attached and never while something is.

### The flow

1. The client connects, pairs, and — once the link is encrypted — reads the status, or subscribes to
   it.
2. A command write is decoded by `GattValues.h`; a malformed one is answered `BadCommand` and
   queued nothing. A well-formed one goes to `SystemControl`, on the NimBLE host task, which is a
   read of the machine and a post into the queue — never a touch of the vacuum system.
3. The reply characteristic is set and notified with the outcome, in the write callback.
4. The status is notified on the link's own beat: every `GattStatusPeriodMs` while a client is
   subscribed, and at once when the state or the target differs from what was last published — so a
   set, a vent or a fault is visible within a control period rather than within the beat.

Two characteristics, two writers: the write callback owns the reply, and the beat owns the status.
The callback never touches the status — the beat is a reader of `SystemStatus`, comparing the state
and the target against the last packet it published, so the link needs to know nothing about the
commands it carries. Two tasks setting the same characteristic's value is the one thing BLE's stack
does not offer.

A command is queued rather than applied, so the packet notified at the moment of the reply still
describes the machine as the tick last left it. What the command caused — a new target, a state —
reaches the client on the first beat pass after the tick drains the queue, within about one control
period.

### Who may connect

Every characteristic requires an encrypted link, and the pair is remembered: NimBLE's bonding with
Just Works, no passkey, no man-in-the-middle protection, because the board has no display to show a
number on and no keypad to type one into. The properties carry the requirement, so an unpaired
client's first read is answered with "insufficient encryption" and its own stack starts the pair;
the board does not need a pairing mode.

What that buys and what it does not, said plainly: a stranger with a scanner application can no
longer read the machine or start the pump, and the phone that was paired once reconnects without
asking. It does not authenticate anybody — the first client to pair gets the bond, whoever it was,
and the board has no way to withdraw it, because nothing on the device is a button; what it does buy
is an encrypted wire and a pair that is remembered. A pairing gate is a later decision with a button
behind it; it is not this plan's.

Bonds live in NVS and are the stack's to keep, so they survive a power cut. A board handed to
somebody else is a board somebody else can pair with, and a client that has forgotten its half of a
bond has a new bond to make.

## The code, and what it touches

`BleLink` is one class: a constructor taking the control and the log, `begin()` to start the stack
and the service, and `poll(nowMs)` for the beat. What is new around it is small.

`MachineTasks` gains the link on the interface task: the task's state holds a `BleLink*` and its
5 ms pass asks it for the beat. A five-second notify is nothing on a pass that runs two hundred
times in it.

`gardener.ino` builds the link and calls `begin()` before the tasks are spawned. A `begin()` that
fails logs a warning and leaves everything else running; a missing Bluetooth stack is a lost
feature, not an unbootable board.

The build gains a dependency it did not have. `sketch.yaml` grows a profile naming the platform and
`NimBLE-Arduino` at a version, and `arduino.py` compiles with `--profile`, so the Bluetooth stack
is the tree's choice rather than whatever the machine's library manager was left holding — the same
reason the FQBN is pinned there. Two knobs come with the link, `GattNamePrefix` — the `Gardener`
the device advertises under — and `GattStatusPeriodMs`, five seconds, in the style the rest of the
tree declares its numbers.

The one thing it has to hold on is the surface itself: `SystemControl`'s four calls,
`ServeResult`, the refusal words and the cases that hold them. If the Bluetooth link needs a change
in `VacuumSystem`, the surface is being asked for something it does not have and the change belongs
in the surface first.

The README gains a Bluetooth section, and it is the document a client author reads: the UUIDs, the
three packets, the four code tables, the name the board advertises under, and the paragraph above
about what a bond is and is not worth. Nothing else in it moves, because nothing else changes.

## The decisions, and the ones set aside

**A packet, not text.** A command characteristic could carry a text line and a reply characteristic
its text answer. It was set aside on size: a status line is about 90 bytes, notifications carry at
most `MTU − 3`, and the default MTU is 23, so the link's correctness would rest on a negotiation
the client controls. A client that does not negotiate would get a truncated notification with
nothing in it to say so. The packet is 18 bytes and never needs the question asked.

**The sketch, not the library.** A `BleLink` in `libraries/Gardener/src` would be compiled by the
host suite, which knows to skip `<FS.h>`, `freertos/` and `esp32-hal-` and nothing about
`<NimBLEDevice.h>`; it would either break the suite or need the suite taught a fifth exception, for
a file the suite could not run anyway. The library stays what a host can compile, the sketch holds
what names the board, and the encoding — the part worth a case — sits in a platform-free header
where both get it.

**128 bits, not 16.** A 16-bit UUID would leave room for the name in the same packet, but the SIG
assigns those and a self-chosen one eventually collides with a phone's idea of some other service.
The cost is the scan response carrying the name, which every scanner reads anyway.

**NimBLE, not Bluedroid.** The Arduino core's Bluetooth is Bluedroid, the larger of the two and the
one the core brings with it; NimBLE is the smaller stack, it is installed, and `sketch.yaml` pins
it.

**The beat, and an edge on the state.** Notifying on every change would put the pressure's every
wobble on the air and the stack in the loop's path; notifying only on the beat would leave a client
showing `PULL` up to five seconds after it was asked for, which reads as a machine that ignored the
command. The state and the target are the two things a client acts on and both change rarely, so
those two are the edge and the beat covers everything else, including the machine being alive.

**Bonded, not open.** Chosen and argued above. The open alternative would have been the smaller
mechanism; it was set aside because this is the first wire into the device that reaches past the
bench — anyone within ten metres rather than anyone who has been given the means to reach it.

## What could go wrong

**One antenna.** Bluetooth shares the 2.4 GHz radio with WiFi when WiFi is up. Coexistence is the
stack's to arbitrate, and what it costs is this board's to measure: a connection interval landing on
a scan, a notification delayed while the radio is busy. Nothing in the firmware can decide this; it
is an observation on hardware, and the five-second beat is deliberately unhurried.

**Heap.** NimBLE brings its own buffers and a host task. The sketch logs free heap at the end of
`setup()` today; the figure after the link starts is the one to look at, and the free-heap entry in
the parking lot is the item that would make it a number the board reports rather than one a log
line happens to hold.

**Flash.** The stack is larger than anything the tree links today. The partition is pinned in
`sketch.yaml`; the build's program-storage figure has to fit inside it, once, and that is a check of
the sketch build rather than of the suite.

**A full queue.** `SystemControl::set` posts and does not read the queue's answer, so a set that
finds the eight-deep queue full is dropped behind a reply that said it was taken. The link inherits
that rather than repairing it: if it is worth repairing, it is the surface that answers `NotReady`,
and every wire gains the answer at once.

**Two writers.** The reply belongs to the write callback and the status to the beat; anything that
has either characteristic written from both tasks is a corruption the stack will not report. The
rule is the ownership, not a lock.

**A bond nobody can withdraw.** The board keeps what it is given. A client that lost its side of a
bond, or a board that has been paired to a phone somebody took away, is a case the firmware can do
nothing about until there is a button that clears the store.

**Optimism.** The reply reports the decision, not the result; the machine applies the decision on
its next tick. A client that reads the status in the same millisecond as the reply sees the state
before the change.

## How it is known to have worked

- A phone reads the status characteristic and sees 18 bytes whose `uptime_ms` advances and whose
  `pressure_kpa` follows the sensor.
- With notifications enabled, a status arrives within five seconds of an idle minute, and one
  arrives within a control period of every command that moves the state or the target.
- `set` to a target inside the range answers `Ok`, and the status that follows reads `PULL` at that
  target; `set` to 0 answers `Released` and the status reads `VENT` and then `IDLE`.
- `vent` answers `Ok` and the machine vents; `reset` answers `Ok` and a latched fault clears.
- A `set` past `ControlFloorKpa` answers `AboveRange`, a `set` while a fault is latched answers
  `NotReady` with `latch`, and `get` afterwards shows the target and the fault unchanged — nothing
  was queued.
- A write of two bytes, of six bytes, of a five-byte vent and of an unknown opcode each answer
  `BadCommand`, and the machine does not move.
- An unpaired client cannot read or write; its first access starts the pair on its own side, and
  after that the read and the write both answer. The same phone reconnects after a power cut without
  pairing again.
- A second client is refused while the first is connected, and the board is findable again within
  seconds of the first disconnecting.
- The existing cases still pass.
- With WiFi joined to a network, a phone holding a Bluetooth link still reads and commands the
  machine.
- The free heap logged at the end of `setup()` is a figure that survives a run, not one that only
  holds at boot.

The steps that get there, in the order they leave the tree working, are the backlog's.
