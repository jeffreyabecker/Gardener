# Gardener serial protocol

Pressures are **kPa** everywhere. `kpa` is the vacuum depth below ambient — 0 at rest, rising as
the chamber is pumped down — and a **target** is a depth on the same ruler. The ambient reference
belongs to the sensor: the MPRLS is absolute, so it takes one from its first reading when it comes
up, and nothing on the wire sets or restates one. The wire revision is `proto`, reported by
`device get`: it changes when a command's *meaning* changes, not when commands are added. `fw` is
the build.

The device is a serial peripheral in the strict sense: it never initiates, keeps no session,
and every state it has is either a command it was given or something it had to do for
safety. All control comes from the controller at the other end of the line, including how long
a run lasts.

This document is the proto 3 surface: commands are resource-first and every reply has the same
shape (§3). It replaces the 0.5.0 wire wholesale rather than adding to it; §14 lists the old
spellings next to their replacements so a controller can be migrated mechanically.

`vacuum`, `panel`, `link` and `device` are what fw 0.6.0 answers, and `help` lists them with their
fields, actions and bounds (§10).

## 1. The port

| Setting | Value |
| --- | --- |
| Transport | USB CDC (native USB, the USB-C socket) by default; UART0 on GPIO43/44 is a one line change in `App` |
| Baud | 115200, 8N1 — only meaningful on a real UART: USB CDC ignores it |
| Terminator | `\n` by default; `link term` can point it at any string of up to 16 characters (§9). A `\r` immediately before the terminator is ignored, so `\r\n` works with the default. `\r` alone does not end a command — set the terminator to `\r` if a console needs that |
| Line limit | 128 bytes on a command line, or 64 KiB while a custom terminator is set. A longer line is answered `err bad_argument` and swallowed, never truncated |

`USBMode=hwcdc,CDCOnBoot=cdc` is part of the build, not a preference: `CDCOnBoot` is what
maps `Serial` to the chip's native USB CDC. Building without it (`esp32:esp32:esp32s3`
alone) sends the whole interface to UART0 on GPIO43/44, which is not the USB socket, and the
device looks dead to a cable. The FQBN is pinned in `firmware/gardener/sketch.yaml`,
`scripts/arduino.ps1` and `.vscode/arduino.json` for exactly that reason.

## 2. Connecting

1. Open the port. The device sends nothing until it is asked: there is no greeting and no
   handshake to wait for.
2. Send `device get` and check `proto` and `fw`. `proto=3` is this document; a controller that
   does not recognise the number should stop rather than guess, because `proto` only moves when
   a command's meaning has moved.
3. Ask for what you need: `vacuum get` is the readout, and it is polled (§13). Nothing is
   pushed.
4. `help` lists the resources, and `help <resource>` lists that resource's fields and actions
   with their bounds (§10). A controller that reads `help` once at connect has no reason to
   hardcode a value range again.

**Disconnecting stops the run and vents the chamber** (§12), because a controller that goes
away releases the chamber. The device only acts on it when the stream can report host
presence, which USB CDC can and a plain UART cannot. A controller that detaches, like a
`device reboot`, also drops back to the default terminator and an empty buffer, so the next one
is never stranded on framing it did not choose.

## 3. Grammar

```
command  = resource SP op *( SP arg ) TERM
reply    = "ok" SP items LF | "err" SP code *( SP item ) LF
page     = reply-line *( row-line ) end-line
items    = item *( SP item )
item     = key "=" value                ; nothing else is a payload item
row      = items
key      = ALPHA *( ALPHA / DIGIT / "_" )
value    = 1*( %x21-7E )
resource = vacuum / panel / link / device / help
op       = field "=" value *( SP field "=" value )   ; write one or more fields
         | field                                     ; read a field
         | action                                    ; that resource's action list
```

* The first token says **what** is being touched and the second says **what to do with it**.
  There is no verb that covers four unrelated layers, and no actuator that is spelled as a bare
  verb while its neighbour is spelled as a `set`.
* Only `resource` and `op` are tokens to the framing layer. Everything after them belongs to
  that op: a write carries `field=value` pairs, and an action takes no argument at all.
* A field with no `=` is a read; a field with `=` is a write. `get` is the reserved op that
  reads every field of the resource.
* `resource` and `op` are case insensitive. Values are not, and `link term` is case sensitive on
  purpose — it is a marker, not a word.
* `-` means unset or not applicable, and it is the only spelling for that. `off` means
  explicitly disabled, and only a field whose disabled state has a name (`log`, `term`)
  uses it.
* Values never contain a space, and the first `=` separates the key from the value, so a value
  may contain `=`, `|` or `,` with no quoting. Lists are never packed into a value (§10).
* A line starting with `#` is ignored, so a script can carry comments.
* A command the device does not consume is `bad_argument`: no verb quietly drops a stray
  argument, and no verb accepts one it has no use for.
* Unknown reply keys must be ignored, which is what lets fields be added later.

### 3.1 Reply rules

* The payload is items — `key=value`, space separated — and nothing else. No flags, no bare
  words, no positional payloads, no `key value` pairs.
* An **action** replies with `result=<action>` first, then the state fields it changed, named
  exactly as `get` names them:
  `ok result=start state=PULL`, `ok result=reset state=VENT`.
* A **read or write** replies with field items only, and a write reports the field the way a
  read would: `vacuum target=40` and `vacuum target` both answer `ok target=40.0`. There is no
  field that can be written but not read.
* A write validates every field in the command before applying any of them, and a repeated
  field is `bad_argument`. One command may carry at most four fields, which is more than any
  resource writes.
* Values the device formats from a library string have whitespace written as `_`, so an item is
  still one token.
* A rejected command replies `err <code>` plus items that say what was wrong: the field, the
  value, the bound, the reason (§11). Detail belongs on the reply channel, so
  `device log=off` cannot hide why a command failed.

## 4. Diagnostics

Diagnostics share the stream with replies, distinguished by the first character:

| Prefix | Meaning |
| --- | --- |
| `ok ` / `err ` | a reply to a command |
| `[` | a diagnostic record: `[I]`, `[W]`, `[E]` then text |

A controller reads until it sees a line that is not a `[` record. `device log=off` silences
diagnostics entirely, so a controller that wants a strictly quiet channel can have one, and
`device get` reports `log=on` or `log=off` so it knows what to expect. Diagnostics are commentary: every
reason a command failed is also on the reply line (§11).

## 5. `device` — the machine and this stream

| Field | Access | Values | Notes |
| --- | --- | --- | --- |
| `fw` | r | text | firmware version |
| `proto` | r | int | wire revision: `3` for this document. Moved only when a command's meaning moves |
| `chip` | r | text | chip model |
| `heap` | r | bytes | free heap |
| `batt_v` | r | volts, 2 dp, `-` | pack volts, `-` when no pack is fitted |
| `batt_pct` | r | percent, `-` | pack charge, `-` when no pack is fitted |
| `log` | rw | `on` / `off` | diagnostics on this stream |
| `unit` | rw | `kpa` / `psi` / `inhg` / `mmhg` | the unit the panel prints pressure in. Stored in LittleFS and restored at boot |

| Action | Reply | Notes |
| --- | --- | --- |
| `device reboot` | `ok result=reboot` | boots safe: pump off, valve open, the run is not restored (the display unit is), terminator back to `\n`, panel back to rotation 0, ink normal, splash drawn |

The reply is written before the restart, so `result=reboot` means "restarting", not "restarted":
it is the one reply that describes something that has not happened yet.

```
-> device get
<- ok proto=3 fw=0.6.0 chip=ESP32-S3 heap=298412 batt_v=3.94 batt_pct=78 log=on unit=kpa
-> device log=off
<- ok log=off
-> device unit=mmhg
<- ok unit=mmhg
```

`device unit=` is the one write that survives a reset: the value is written to the `[device]`
section of `/settings.ini` and read back at boot. A filesystem that will not take the write still answers with the unit in force
for this session, and the boot log reports the failed mount. The machine and this wire stay in kPa
either way — a display unit is a preference about the glass, not a second way to state a pressure.

## 6. `vacuum` — the machine that holds pressure

| Field | Access | Values | Notes |
| --- | --- | --- | --- |
| `kpa` | r | kPa, 2 dp | filtered vacuum depth below ambient: 0 at rest, rising as the chamber is pumped down |
| `target` | rw | kPa below ambient, or `-` | `vacuum target=40` pulls to 40 kPa below the reference; `vacuum target=-` releases. Shallower than `CONTROL_MIN_TARGET_KPA` is `err out_of_range field=target min=…`; deeper than `floor` is accepted, and only pins the duty at the cap |
| `floor` | r | kPa, 1 dp | deepest reachable target, `PUMP_MAX_SUCTION_KPA` below the reference |
| `duty` | r | %, 1 dp | pump duty |
| `state` | r | `IDLE` / `PULL` / `HOLD` / `VENT` / `FAULT` | |
| `batt` | r | percent, `-` | `-` when no battery is fitted |
| `sensor` | r | `1` / `0` | `1` when the MPRLS is answering |
| `fault` | r | `none` / `sensor` | |
| `up` | r | ms | uptime |

| Action | Reply | Notes |
| --- | --- | --- |
| `vacuum start` | `ok result=start state=PULL` / `err not_ready reason=…` | closes the valve, then pulls to the target. Reasons: `no_target`, `no_reading`, `latch` — three causes, three reasons, where `not_ready` alone used to be one opaque code |
| `vacuum stop` | `ok result=stop state=VENT` | pump off, then vent. The normal way a run ends |
| `vacuum reset` | `ok result=reset state=VENT` | clears the latched fault, then vents |

```
-> vacuum get
<- ok kpa=40.07 target=40.0 floor=60.0 duty=72.0 state=HOLD batt=78 sensor=1 fault=none up=45000
-> vacuum target=40
<- ok target=40.0
-> vacuum start
<- ok result=start state=PULL
-> vacuum target=-
<- ok target=-
-> vacuum stop
<- ok result=stop state=VENT
```

`target` is the only actuator field. There is no `duty` write: a second way to drive the pump
would mean a second control path to reason about. Characterizing the pump therefore needs a
temporary build. That trade was made deliberately, so nobody should go looking for a raw duty
command that is not there.

Vent margin and timeout, the PID gains, the deadband, the duty cap, the slew limit and the
battery thresholds are all in `core/Config.h`. They are not commands, so the command surface
stays this table. The deadband matters to a controller: it is what decides that the target has
been reached, and it is not on the wire. `CONTROL_MIN_TARGET_KPA` is the other one worth
knowing: it is the shallowest target the loop will accept, and it is reported as `min=` on
`help vacuum` as well as in the `out_of_range` reply.

## 7. `panel` — the glass

| Field | Access | Values | Notes |
| --- | --- | --- | --- |
| `w` / `h` | r | pixels | panel geometry, so a host reads it instead of hardcoding the panel |

The panel is a peripheral and the device holds the display, but the wire only reports its
geometry: there is no drawing command, and no panel setting to write.

```
-> panel get
<- ok w=64 h=128
```

With no host attached the page is a **Berry routine** from `/routines/`, and the routine registered
as `default` is the one drawn; with none, the panel shows the device's mark. Attaching a host takes
the glass back for the readout, and detaching returns it to the routine, which is why `device unit=`
changes what a routine prints even though the wire never draws.

## 8. `link` — how commands arrive

| Field | Access | Values | Notes |
| --- | --- | --- | --- |
| `term` | rw | `off`, or a marker of up to 16 decoded characters | point the end of a command at a marker rather than newline (§9). `off` is newline framing |
| `attached` | r | `1` / `0` / `-` | a host has the port open; `-` when the transport cannot tell (a plain UART) |

`link term` is the only command whose *value* is case sensitive, and the only one that changes
how the device reads bytes rather than what it does with them. It is not a mode: it is a
receive-side setting and works the same whatever the device is doing.

## 9. Framing, terminators and payloads

`link term` points the end of a command at a marker rather than at newline:

```
-> link term=<<<END>>>
<- ok term=<<<END>>>
-> vacuum start<<<END>>>
<- ok result=start state=PULL
-> link term=off
<- ok term=off
```

* `\n`, `\r`, `\t` and `\\` stand for the characters a command line cannot carry. Up to 16
  characters decoded, and the token is echoed as it was written, so the marker is round
  trippable: `ok term=\n\n` names the marker that was set.
* Case sensitive, and `off` is reserved whatever its case — `link term=off` puts newline framing
  back, so a marker cannot be the literal word `off`. Any other marker of up to 16 characters is available, and
  `link term` reads back which one is in force. `-` is not an alias: `-` means unset and there is
  no unset marker.
* A marker turns newline framing off: a line ends at the marker rather than at `\n`. The custom
  terminator is a receive-side setting: replies stay `LF` terminated, whatever the marker is, so
  a controller that counts them keeps counting them.
* The line ceiling is 128 bytes, or `COMMAND_PAYLOAD_MAX` while a marker is set.
* The framing is reset by link loss and by `device reboot`. A controller that dies mid-line
  therefore does not leave a marker behind for the next one.

## 10. Pages

One op answers with a page: `help`. A page is one reply line, then
any number of rows, then an `end` line:

```
page = ("ok" SP items LF) row* ("end" SP items LF)
```

* A row is items, like every other payload: `resource=vacuum fields=9 actions=3`.
* The `end` line repeats the op and counts the rows, so a truncated page is detectable:
  `end result=help rows=4`.
* An error replaces the whole page with a single `err` line and no rows. A controller reads rows
  until `end` or `err`.
* This is why no value is a packed list. Lists used to be `|`-separated inside one value, which
  made `|` ambiguous the moment a real value contained one; now a list is a page, and a value can
  hold anything printable but a space.

`help` is the discovery mechanism, and it names things that are actually commands. The pages below
are abridged: a real one carries a row per field and a row per action.

```
-> help
<- ok result=help rows=4
<- resource=vacuum fields=9 actions=3
<- resource=panel fields=2 actions=0
<- resource=link fields=2 actions=0
<- resource=device fields=7 actions=1
<- end result=help rows=4
-> help vacuum
<- ok result=help resource=vacuum rows=12
<- field=target type=float access=rw unit=kPa min=5.0
<- field=state type=choice access=r choices=IDLE,PULL,HOLD,VENT,FAULT
<- action=start
<- end result=help resource=vacuum rows=12
-> help panel
<- ok result=help resource=panel rows=2
<- field=w type=int access=r unit=px
<- field=h type=int access=r unit=px
<- end result=help resource=panel rows=2
```

Three row shapes, and the field one carries the bounds a write is held to:

| Row | Shape |
| --- | --- |
| resource | `resource=<name> fields=<n> actions=<n>` |
| field | `field=<name> type=<int\|float\|bool\|choice\|text> access=<r\|rw>` and, where the field has one, `min=` / `max=` / `choices=` / `unit=` |
| action | `action=<name>` |

The firmware answers four resources, and `help` names every field and action it can be asked to
do.

`help` is the only multi-line reply a controller can get without asking for one, and every
resource, field and action a page names is something the device knows — enumerating `help` and
replaying what it names can never produce `err unknown_command`.

## 11. Errors

Every `err` is one of these five, so a controller can switch on the code instead of parsing
prose, and each carries items naming what was involved, where there is something to name:

| Code | Meaning |
| --- | --- |
| `unknown_command` | not a resource, not an op for that resource, or a removed spelling: `err unknown_command token=set` |
| `bad_argument` | an argument is missing, does not parse, is left over or is empty; or the line was too long |
| `out_of_range` | the value parses but is not acceptable: `err out_of_range field=target value=999 min=5.0` |
| `not_ready` | valid, and cannot be honoured now: `reason=no_target`, `reason=no_reading`, or `reason=latch` |
| `unsupported` | the protocol defines it and this build cannot do it: writing a read-only field (`err unsupported field=kpa`), or a feature that is not compiled in |

```
-> vacuum start
<- err not_ready reason=no_target
-> vacuum kpa=40
<- err unsupported field=kpa
-> set target 40
<- err unknown_command token=set
-> vacuum target=1
<- err out_of_range field=target value=1 min=5.0
-> vacuum start extra
<- err bad_argument token=extra
```

The rule that separates the first two is the one the old protocol could not state: a token the
device does not know at all is `unknown_command`, and a token it knows but has no use for in this
place is `bad_argument` or `unsupported`. A spelling that used to work in 0.5.0 is neither — it
is `unknown_command token=<what it was>`, so a controller written against the old wire fails on
its first command instead of misreading a reply. A field named with no value to give it is
`bad_argument token=<the field>`, and a command with no op at all is `bad_argument` with nothing to
name.

## 12. State model

The controller owns the target, the duration and when to start and stop. The device owns what
happens when nobody is watching, and nothing else.

```
       vacuum start                              vacuum stop / link loss / fault / back hold
IDLE ──────────────> PULL ──(target reached)──> HOLD ───────────────────────> VENT ──(complete)──> IDLE
  ^                                                 │                           │
  │                                                 │                           v
  └───────────────── vacuum reset ──────────────────┴─────────────────────── FAULT
```

| Event | From | To |
| --- | --- | --- |
| `vacuum start` | anything but `FAULT` | closes the valve, then `PULL`; `err not_ready` with `reason=no_target`, `reason=no_reading` or `reason=latch`. From `VENT` it abandons the release and pulls again |
| `vacuum stop` | anything | opens the valve and stops the pump; from `PULL`, `HOLD` or `VENT` that is `VENT` then `IDLE`, and from `IDLE` or `FAULT` the state does not move |
| Back held for 3 s (local key) | any | the same as `vacuum stop`, with no command and no reply |
| `vacuum target=<kPa\|->` | anything but `FAULT` | the target changes; `-` while pulling or holding releases the chamber |
| target reached | `PULL` | `HOLD`, and the controller's countdown should start here |
| link loss, sensor loss, pump latch | any | `VENT`, then `IDLE` or `FAULT` |
| `vacuum reset` | `FAULT` | `VENT` then `IDLE` |
| `device reboot` | any | safe boot, i.e. `IDLE` with the valve open |

What a controller has to know:

* **The hold's duration is yours.** The device has no timer, no timeout and no maximum run
  time, so a run continues until you send `vacuum stop`. `HOLD` is the cue, not a countdown: it
  means the device's own deadband has decided the target is reached, which is the moment to start
  timing and the one thing you cannot work out for yourself, because the deadband is the
  device's loop. Timing it is yours: the device is not told how long a hold is, and has no
  command that takes a duration.
* **A Back hold is a local `vacuum stop`.** Holding the Back key for 3 s stops the pump and
  vents, even while a controller is attached. The state moves exactly as a `vacuum stop` moves it.
* **Faults latch.** Only `vacuum reset` clears one; the device never retries on its own.
* Venting finishes when the pressure is within 2 kPa of ambient, or after 30 s, whichever
  comes first. A vent timeout is logged, not an error, and the state still returns to `IDLE`.
* **The vent valve is normally open by default**: de-energised is open, so running a vacuum means
  energising the coil to close it. A crash, a brownout or a cut wire therefore vents the chamber
  whether or not the firmware noticed. `NormallyClosedValve` in `app/Build.cpp` is the alternative and
  inverts all of that — de-energised is sealed.
* A normally open solenoid draws current for the whole hold, which is why a long hold is the
  dominant battery cost rather than the pump. A normally closed part draws it at rest instead,
  holding the valve open to vent. With no timer in the device, an unattended run holds until
  something stops it.
* Link loss releases the chamber, but it only detects a controller that leaves. One that hangs
  with the port open holds the run open.
* Duty is capped and slew limited, and those are the pump's only protections. A target deeper
  than `floor` — the pump's own reach, reported in `vacuum get` — pins the duty at the cap and
  nothing stops it, so read `floor` and keep targets no deeper than it.
* **The panel is the device's.** It shows the state, its icon, both pressures, the battery and the
  two ways in — serial and Bluetooth — so those are readable at the bench, and it falls back to
  the boot mark when no controller is attached. There is no drawing command, so everything else — a
  vent timeout, the duty, the fault code — is only on the wire. Poll `vacuum get` to see it.

## 13. Rates

Nothing is pushed: a controller that wants a readout polls `vacuum get`. Each poll is a ~140 byte
reply, so polling at 4–10 Hz is a trivial load at 115200 baud and gives a smoother readout
than the device could produce on its own. Each reply describes the machine as of the last
control tick (50 ms), so polling faster than that repeats itself.

Because replies are matched by counting, a controller that pipelines several commands in one
write should read one reply per command before concluding anything about state: `vacuum start`
followed immediately by `vacuum get` in the same batch may describe the machine as it was before
the `vacuum start` was handled. Sending `vacuum get` as its own command avoids the question
entirely.

Each loop iteration drains everything the host has sent and services **every complete line** it
received, in arrival order, with nothing dropped and nothing reordered — so a batch written in one
go is handled in one pass. Replies are still exactly one per command, so a host that counts them
reads one per line; it just does not need to assume a batch is spread across iterations.

## 14. Migration from 0.5.0

`proto=3` is the current surface, and the 0.5.0 wire is two breaks away from it: `proto=2` moved
every actuator under a resource and made every reply items, and `proto=3` gave the ambient
reference to the sensor. The moves are mechanical.

`proto=3` is the pressure simplification: the MPRLS takes its own ambient reference when it comes
up, so `vacuum capture` and the `ambient`, `vac`, and `pct` fields are gone. `kpa` is now the
vacuum depth — it used to be the absolute reading — so a controller that treated it as absolute
has to stop. `target` and `floor` are still depths below the same reference.

| 0.5.0 | proto 2+ |
| --- | --- |
| `info` | `device get` |
| `set log on` | `device log=on` |
| `set log off` | `device log=off` |
| `reboot` | `device reboot` |
| `status` | `vacuum get` |
| `set target 40` | `vacuum target=40` |
| `set target off` | `vacuum target=-` |
| `start` | `vacuum start` |
| `stop` | `vacuum stop` |
| `reset` | `vacuum reset` |
| `ambient` | no replacement: the sensor owns the reference |
| `set term <<<END>>>` | `link term=<<<END>>>` |
| `set term off`, `set term -` | `link term=off` |
| `help` | `help`, or `help <resource>` for one resource's fields and actions |

There is no replacement for `draw <op> …`, `flush`, `set rotation` or `set invert`: the wire no
longer draws on or configures the panel.

Three things changed that are not a rename, and a migrating controller has to know about each:

* **Replies changed shape.** `ok started` is `ok result=start state=PULL`; `ok target=off` is
  `ok target=-`. A controller that parsed the old payloads has to be rewritten, not
  re-parameterised.
* **New reads exist.** `device get`, `vacuum get`, `panel get` and `link get` report
  everything the device can be asked to do. `term` used to be write-only or missing from the
  wire; a controller can now rebuild its view after reconnecting instead of assuming.
* **Failures say what was wrong.** `out_of_range` carries the field, the value and the bound;
  `not_ready` carries a reason. Nothing has to go to the diagnostics stream to be
  diagnosable, which matters because `device log=off` is a legitimate thing for a client to do.
