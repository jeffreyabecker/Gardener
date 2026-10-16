# Gardener serial protocol

Pressures are **kPa** everywhere. `kpa` is the sensor's absolute reading, `ambient` is the
reference the machine captured, and a **target** is a depth below that reference; the device
captures the reference rather than accepting one over the wire (`plant capture`, §6). The wire
revision is `proto`, reported by `device get`: it changes when a command's *meaning* changes,
not when commands are added. `fw` is the build.

The device is a serial peripheral in the strict sense: it never initiates, keeps no session,
and every state it has is either a command it was given or something it had to do for
safety. All control comes from the controller at the other end of the line, including how long
a run lasts.

This document is the proto 2 surface: commands are resource-first and every reply has the same
shape (§3). It replaces the 0.5.0 wire wholesale rather than adding to it; §18 lists the old
spellings next to their replacements so a controller can be migrated mechanically.

## 1. The port

| Setting | Value |
| --- | --- |
| Transport | USB CDC (native USB, the USB-C socket) by default; UART0 on GPIO43/44 is a one line change in `App` |
| Baud | 115200, 8N1 — only meaningful on a real UART: USB CDC ignores it |
| Terminator | `\n` by default; `link term` can point it at any string of up to 16 characters (§12). A `\r` immediately before the terminator is ignored, so `\r\n` works with the default. `\r` alone does not end a command — set the terminator to `\r` if a console needs that |
| Line limit | 128 bytes on a command line, or 64 KiB while a custom terminator is set. A longer line is answered `err bad_argument` and swallowed, never truncated |

`USBMode=hwcdc,CDCOnBoot=cdc` is part of the build, not a preference: `CDCOnBoot` is what
maps `Serial` to the chip's native USB CDC. Building without it (`esp32:esp32:esp32s3`
alone) sends the whole interface to UART0 on GPIO43/44, which is not the USB socket, and the
device looks dead to a cable. The FQBN is pinned in `firmware/gardener/sketch.yaml`,
`scripts/arduino.ps1` and `.vscode/arduino.json` for exactly that reason.

## 2. Connecting

1. Open the port. The device sends nothing until it is asked: there is no greeting and no
   handshake to wait for.
2. Send `device get` and check `proto` and `fw`. `proto=2` is this document; a controller that
   does not recognise the number should stop rather than guess, because `proto` only moves when
   a command's meaning has moved.
3. Ask for what you need: `plant get` is the readout, and it is polled (§17). Nothing is
   pushed.
4. `help` lists the resources, and `help <resource>` lists that resource's fields and actions
   with their bounds (§13). A controller that reads `help` once at connect has no reason to
   hardcode a value range again.

**Disconnecting stops the run and vents the chamber** (§15), because a controller that goes
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
resource = plant / panel / link / vm / store / device / help
op       = field "=" value *( SP field "=" value )   ; write one or more fields
         | field                                     ; read a field
         | action                                    ; that resource's action list
```

* The first token says **what** is being touched and the second says **what to do with it**.
  There is no verb that covers four unrelated layers, and no actuator that is spelled as a bare
  verb while its neighbour is spelled as a `set`.
* Only `resource` and `op` are tokens to the framing layer. Everything after them belongs to
  that op, which is what lets `panel draw text 0 0 2 0 41.3` carry a string with spaces in it and
  `store run pulse cycles=5 mode=pulse` carry a parameter list, without the grammar having a
  fixed arity or a special case for either.
* A field with no `=` is a read; a field with `=` is a write. `get` is the reserved op that
  reads every field of the resource.
* `resource` and `op` are case insensitive. Values are not, and `link term` is case sensitive on
  purpose — it is a marker, not a word.
* `-` means unset or not applicable, and it is the only spelling for that. `off` means
  explicitly disabled, and only a field whose disabled state has a name (`log`, `invert`, `term`)
  uses it.
* Values never contain a space, and the first `=` separates the key from the value, so a value
  may contain `=`, `|` or `,` with no quoting. Lists are never packed into a value (§13).
* A line starting with `#` is ignored, so a script can carry comments.
* A command the device does not consume is `bad_argument`: no verb quietly drops a stray
  argument, and no verb accepts one it has no use for.
* Unknown reply keys must be ignored, which is what lets fields be added later.

### 3.1 Reply rules

* The payload is items — `key=value`, space separated — and nothing else. No flags, no bare
  words, no positional payloads, no `key value` pairs.
* An **action** replies with `result=<action>` first, then the state fields it changed, named
  exactly as `get` names them:
  `ok result=start state=PULL`, `ok result=draw op=line`, `ok result=rm program=pulse`.
* A **read or write** replies with field items only, and a write reports the field the way a
  read would: `plant target=40` and `plant target` both answer `ok target=40.0`. There is no
  field that can be written but not read.
* A write validates every field in the command before applying any of them, and a repeated
  field is `bad_argument`. `panel rotation=1 invert=on` is one command that either sets both or
  neither.
* Values the device formats from a library string have whitespace written as `_`, so an item is
  still one token.
* A rejected command replies `err <code>` plus items that say what was wrong: the field, the
  value, the bound, the reason, the mode (§14). Detail belongs on the reply channel, so
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
reason a command failed is also on the reply line (§14).

## 5. `device` — the machine and this stream

| Field | Access | Values | Notes |
| --- | --- | --- | --- |
| `fw` | r | text | firmware version |
| `proto` | r | int | wire revision: `2` for this document. Moved only when a command's meaning moves |
| `chip` | r | text | chip model |
| `heap` | r | bytes | free heap |
| `batt_v` | r | volts, 2 dp, `-` | pack volts, `-` when no pack is fitted |
| `batt_pct` | r | percent, `-` | pack charge, `-` when no pack is fitted |
| `log` | rw | `on` / `off` | diagnostics on this stream |

| Action | Reply | Notes |
| --- | --- | --- |
| `device reboot` | `ok result=reboot` | boots safe: pump off, valve open, nothing restored, terminator back to `\n`, panel back to rotation 0, ink normal, splash drawn |

The reply is written before the restart, so `result=reboot` means "restarting", not "restarted":
it is the one reply that describes something that has not happened yet.

```
-> device get
<- ok proto=2 fw=0.6.0 chip=ESP32-S3 heap=298412 batt_v=3.94 batt_pct=78 log=on
-> device log=off
<- ok log=off
```

## 6. `plant` — the machine that holds pressure

| Field | Access | Values | Notes |
| --- | --- | --- | --- |
| `ambient` | r | kPa, 2 dp | the captured reference. `plant capture` is the only way one is ever set: `plant ambient=40` is `err unsupported field=ambient`, because the rig should not be holding against a number nobody measured |
| `kpa` | r | kPa, 2 dp | filtered absolute pressure |
| `vac` | r | kPa, 2 dp | `ambient - kpa`, clamped at 0 |
| `pct` | r | %, 1 dp | `vac / ambient * 100` |
| `target` | rw | kPa below ambient, or `-` | `plant target=40` pulls to 40 kPa below the reference; `plant target=-` releases. Shallower than `CONTROL_MIN_TARGET_KPA` is `err out_of_range field=target min=…` |
| `floor` | r | kPa, 1 dp | deepest reachable target, `PUMP_MAX_SUCTION_KPA` below the reference |
| `duty` | r | %, 1 dp | pump duty |
| `state` | r | `IDLE` / `PULL` / `HOLD` / `VENT` / `FAULT` | |
| `batt` | r | percent, `-` | `-` when no battery is fitted |
| `sensor` | r | `1` / `0` | `1` when the MPRLS is answering |
| `fault` | r | `none` / `sensor` | |
| `up` | r | ms | uptime |

| Action | Reply | Notes |
| --- | --- | --- |
| `plant start` | `ok result=start state=PULL` / `err not_ready reason=…` | closes the valve, then pulls to the target. Reasons: `no_target`, `no_reading`, `latch` — three causes, three reasons, where `not_ready` alone used to be one opaque code |
| `plant stop` | `ok result=stop state=VENT` | pump off, then vent. The normal way a run ends |
| `plant reset` | `ok result=reset state=VENT` | clears the latched fault, then vents |
| `plant capture` | `ok ambient=101.33` | capture the current reading as the reference, and report the field it set |

```
-> plant get
<- ok ambient=101.33 kpa=61.26 vac=40.07 pct=39.5 target=40.0 floor=60.0 duty=72.0 state=HOLD batt=78 sensor=1 fault=none up=45000
-> plant target=40
<- ok target=40.0
-> plant start
<- ok result=start state=PULL
-> plant target=-
<- ok target=-
-> plant stop
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
`help plant` as well as in the `out_of_range` reply.

## 7. `panel` — the glass

| Field | Access | Values | Notes |
| --- | --- | --- | --- |
| `rotation` | rw | `0`-`3` | panel orientation; outside 0-3 is `err out_of_range field=rotation min=0 max=3` |
| `invert` | rw | `on` / `off` | swap ink and paper on the whole panel |
| `w` / `h` | r | pixels | geometry, rotation aware, so a host reads it instead of hardcoding the panel |

| Action | Reply | Notes |
| --- | --- | --- |
| `panel draw <op> …` | `ok result=draw op=<op>` | one drawing operation into the panel's buffer (§11) |
| `panel flush` | `ok result=flush` | the frame boundary: buffer to glass, battery composited |

`rotation` and `invert` are properties of the glass rather than of a frame, so neither needs a
`panel flush` and both survive one. They are also readable, which they were not before: a controller
that attaches mid-session can ask instead of assuming.

```
-> panel get
<- ok rotation=1 invert=off w=64 h=128
-> panel rotation=0
<- ok rotation=0
-> panel invert=on
<- ok invert=on
```

## 8. `link` — how commands arrive

| Field | Access | Values | Notes |
| --- | --- | --- | --- |
| `term` | rw | `off`, or a marker of up to 16 decoded characters | point the end of a command at a marker, so one command can carry a whole multi-line payload (§12). `off` is newline framing |
| `attached` | r | `1` / `0` / `-` | a host has the port open; `-` when the transport cannot tell (a plain UART) |

`link term` is the only command whose *value* is case sensitive, and the only one that changes
how the device reads bytes rather than what it does with them. It is not a mode: framing works
in every mode, which is why `store put` can upload a program while the VM is running or not.

## 9. `vm` — the program runtime

| Field | Access | Values | Notes |
| --- | --- | --- | --- |
| `mode` | rw | `standalone` / `peripheral` | who owns the plant and the panel (§16) |
| `program` | r | name, or `-` | the program that owns the plant, `-` when there is none |
| `running` | r | `0` / `1` | |
| `host` | r | `0` / `1` | a host is attached |
| `s0` … `s7`, `s0v` … `s7v` | r | text | the program's state table: up to 8 key then value pairs, key and value each ≤ 11 characters. `-` when unused (spec 07 §2) |

```
-> vm get
<- ok mode=standalone program=default running=1 host=1 s0=phase s0v=hold s1=cycle s1v=3
-> vm mode=peripheral
<- ok mode=peripheral
-> vm get
<- ok mode=peripheral program=- running=0 host=1
```

## 10. `store` — the programs on the device

| Op | Reply | Notes |
| --- | --- | --- |
| `store list` | `ok result=list programs=3` + one row per program + `end result=list rows=3` | rows: `program=<name> size=<n> params=<n> default=<0\|1> running=<0\|1>`, from the store index (spec 02 §7). `default` is first |
| `store cat <name> [offset] [len]` | `ok result=cat program=<name> offset=<n> len=<n> total=<n>` + source lines + `end result=cat rows=<n>` | a page, so the source arrives in one command and any number of lines |
| `store rm <name>` | `ok result=rm program=<name>` | `store rm default` re-seeds the passive source |
| `store run <name> [k=v …]` | `ok result=run program=<name> state=<state>` | stop current → vent → start; switches to stand-alone if it has to. Parameters are validated against the manifest (spec 06 §3) |
| `store params <name>` | `ok program=<name> k=v …` | effective values: persisted over manifest defaults |
| `store params <name> k=v …` | `ok program=<name> k=v …` | persist without running. A read and a write answer with the same payload, so one parser covers both |
| `store put <name>` | `ok result=put program=<name> size=<n> crc=<n>` | one shot: set a terminator first, then the source follows the first newline and runs to that marker (§12, spec 06 §6). Not chunked and not hex encoded |

Reads (`store list`, `store cat`, `store params`) are answered where they arrive; `store run`,
`store rm`, `store params <name> k=v …` and `store put` change what is running or what is
stored, so they are applied at the program's yield point rather than inside its stack (spec 01
§5). A `store list` therefore cannot land in the middle of an upload: an upload is one command
and runs to its terminator (spec 02 §7).

## 11. Drawing on the panel

The display is a 128x64 monochrome OLED, and from this side of the cable it is a buffer and a
frame boundary. `panel draw` writes into the buffer; nothing reaches the glass until
`panel flush`, which pushes the whole buffer at once. So a frame is however many operations the
host wants, then one `panel flush`, and a host can redraw at whatever rate it likes without a
partial frame appearing.

```
-> panel draw clear
<- ok result=draw op=clear
-> panel draw text 0 0 2 0 41.3
<- ok result=draw op=text
-> panel draw rect 0 40 100 8
<- ok result=draw op=rect
-> panel draw fillrect 1 41 72 6
<- ok result=draw op=fillrect
-> panel flush
<- ok result=flush
```

| Operator | Arguments |
| --- | --- |
| `clear` | — |
| `pixel` | `x y` |
| `line` | `x0 y0 x1 y1` |
| `hline` | `x y w` |
| `vline` | `x y h` |
| `rect` | `x y w h` |
| `fillrect` | `x y w h` |
| `roundrect` | `x y w h r` |
| `fillroundrect` | `x y w h r` |
| `circle` | `x y r` |
| `fillcircle` | `x y r` |
| `ellipse` | `x y rx ry` |
| `fillellipse` | `x y rx ry` |
| `triangle` | `x0 y0 x1 y1 x2 y2` |
| `filltriangle` | `x0 y0 x1 y1 x2 y2` |
| `rotatedrect` | `cx cy w h angle` |
| `fillrotatedrect` | `cx cy w h angle` |
| `text` | `x y size inverted string` |

There are eighteen of them, and they are positional on purpose: a frame is twenty lines or
more, and making the geometry `key=value` would multiply the bytes on the one command where
volume matters. This is the only op whose arguments stray from the item shape, and only the
last one does.

Every operator answers `ok result=draw op=<op>`, including `clear`. An operator that is not in
the list is `err unknown_command token=<op>`.

**Coordinates** are panel pixels, origin top left, signed 16-bit. They are checked against that
range and nothing else: a coordinate off the panel is not an error, and what becomes of it is the
panel library's business. `panel get` reports `w` and `h`, and orientations 0 and 2 are 128 wide
by 64 high while 1 and 3 swap that. Nothing on the wire reports the size of a string — the
font is a known font, so layout is the host's arithmetic to do.

**Text** is the one operator whose last argument is a string: everything after `inverted` is the
string, verbatim, spaces and all, to the end of the line. That is the reason `panel draw` is
handed the rest of the line rather than a token list, and the reason a string cannot be followed
by anything. `size` scales the device's built-in font and is 1 to 8; anything else is
`err out_of_range field=size min=1 max=8`. There is no font choice — the device ships one font
and `size` is a scale — and `inverted` is `0` for ink glyphs on whatever is already there, `1`
for paper glyphs on an ink block, which is how a label gets a filled background.

**Two things on the panel are not the host's.** The device draws the boot splash before any host
has spoken, and it composites the battery on every `panel flush`, after the host's frame.
Drawing over either does not remove it: the battery is put back on the way out, and the splash
only goes when the host draws a frame over it.

**The buffer is not cleared for you.** A frame is whatever is in the buffer when `panel flush`
arrives, so a redraw starts with `panel draw clear` or deliberately draws over the last frame.
`device reboot` resets all of it: empty buffer, rotation 0, ink normal, and the splash drawn
again.

**Three things are absent on purpose.** A host should know they are not coming:

* **No bitmap blit.** Icons are composed from the shape operators, not sent as images.
* **No text measurement.** The host owns layout, so it knows or reserves the width of its own
  strings.
* **No `clearRect`.** The device's own drawing interface has one; the wire does not, because a
  frame is redrawn whole and a partial erase would need a second way to express the frame.

## 12. Framing, terminators and payloads

`link term` points the end of a command at a marker, which is how a whole multi-line payload —
a program's source — arrives as **one** command rather than in chunks:

```
-> link term=<<<END>>>
<- ok term=<<<END>>>
-> store put pulse
   ... program source, free form, any number of lines ...
<<<END>>>
<- ok result=put program=pulse size=412 crc=3298
-> link term=off
<- ok term=off
```

* `\n`, `\r`, `\t` and `\\` stand for the characters a command line cannot carry. Up to 16
  characters decoded, and the token is echoed as it was written, so the marker is round
  trippable: `ok term=\n\n` names the marker that was set.
* Case sensitive, and `off` is reserved — `link term=off` puts newline framing back, so a marker
  cannot be the literal word `off`. Any other marker of up to 16 characters is available, and
  `link term` reads back which one is in force. `-` is not an alias: `-` means unset and there is
  no unset marker.
* A marker turns newline framing off, so a whole multi-line payload arrives as one command. The
  custom terminator is a receive-side setting: replies stay `LF` terminated, whatever the marker
  is, so a controller that counts them keeps counting them.
* The line ceiling is 128 bytes, or `COMMAND_PAYLOAD_MAX` while a marker is set — the one case
  where a single command is a whole file, and the reason the ceiling moves at all.
* The framing is reset by link loss and by `device reboot`. A controller that dies mid-upload
  therefore does not leave a marker behind for the next one.

## 13. Pages

Three ops answer with a page: `help`, `store list`, `store cat`. A page is one reply line, then
any number of rows, then an `end` line:

```
page = ("ok" SP items LF) row* ("end" SP items LF)
```

* A row is items, like every other payload: `program=pulse size=412 params=3 default=1 running=0`.
* The `end` line repeats the op and counts the rows, so a truncated page is detectable:
  `end result=list rows=3`.
* An error replaces the whole page: `store cat missing` is a single `err not_found program=missing`
  line with no rows and no `end`. A controller reads rows until `end` or `err`.
* This is why no value is a packed list. Lists used to be `|`-separated inside one value, which
  made `|` ambiguous the moment a real value contained one; now a list is a page, and a value can
  hold anything printable but a space.

`help` is the discovery mechanism, and it names things that are actually commands. The pages below
are abridged: a real one carries a row per field and a row per action.

```
-> help
<- ok result=help rows=6
<- resource=plant fields=12 actions=4
<- resource=panel fields=4 actions=2
<- resource=link fields=2 actions=0
<- resource=vm fields=20 actions=0
<- resource=store fields=0 actions=6
<- resource=device fields=7 actions=1
<- end result=help rows=6
-> help plant
<- ok result=help resource=plant rows=16
<- field=ambient type=float access=r unit=kPa
<- field=target type=float access=rw unit=kPa min=5 max=150
<- field=state type=choice access=r choices=IDLE,PULL,HOLD,VENT,FAULT
<- action=start args=0
<- action=capture args=0
<- end result=help resource=plant rows=16
-> help panel draw
<- ok result=help resource=panel op=draw rows=18
<- op=clear args=0
<- op=text args=4 string=1
<- end result=help resource=panel op=draw rows=18
```

Three row shapes, and one of them carries the bounds a write enforces:

| Row | Shape |
| --- | --- |
| resource | `resource=<name> fields=<n> actions=<n>` |
| field | `field=<name> type=<int\|float\|bool\|choice\|text> access=<r\|rw>` and, where the field has one, `min=` / `max=` / `choices=` / `unit=` |
| action or op | `action=<name> args=<n> [payload=<none\|string>]`, or `op=<name> args=<n> [string=1]` for `panel draw` |

`help` is the only multi-line reply a controller can get without asking for one, and every
resource, field, action and op a page names is something the device knows — enumerating `help` and
replaying what it names can never produce `err unknown_command`. `help store` is also what
publishes a program's own manifest bounds, so a controller stops hardcoding them.

## 14. Errors

Every `err` is one of these seven, so a controller can switch on the code instead of parsing
prose, and every one of them carries items that say which field, value, bound, reason or mode was
involved:

| Code | Meaning |
| --- | --- |
| `unknown_command` | not a resource, not an op for that resource, or a removed spelling: `err unknown_command token=set` |
| `bad_argument` | an argument is missing, does not parse, is left over or is empty; or the line was too long |
| `out_of_range` | the value parses but is not acceptable: `err out_of_range field=cycles value=999 min=1 max=100` |
| `not_ready` | valid, and cannot be honoured now: `reason=no_target`, `reason=no_reading`, `reason=latch`, or `mode=standalone` for a command that belongs to the other mode |
| `unsupported` | the protocol defines it and this build cannot do it: writing a read-only field (`err unsupported field=ambient`), or a feature that is not compiled in |
| `not_found` | `store` named a program that does not exist: `err not_found program=pulse` |
| `full` | a `store put` payload over `ProgramStore::kMaxBytes`, or a store with no room left: `err full program=pulse max=8192` |

```
-> plant start
<- err not_ready reason=no_target
-> plant ambient=40
<- err unsupported field=ambient
-> set target 40
<- err unknown_command token=set
-> panel draw text 0 0 9 0 hi
<- err out_of_range field=size value=9 min=1 max=8
-> vm mode=peripheral extra
<- err bad_argument token=extra
```

The rule that separates the first two is the one the old protocol could not state: a token the
device does not know at all is `unknown_command`, and a token it knows but has no use for in this
place is `bad_argument` or `unsupported`. A spelling that used to work in 0.5.0 is neither — it
is `unknown_command token=<what it was>`, so a controller written against the old wire fails on
its first command instead of misreading a reply.

## 15. State model

The controller owns the target, the duration and when to start and stop. The device owns what
happens when nobody is watching, and nothing else.

```
        plant start                                        plant stop / link loss / fault
IDLE ──────────────> PULL ──(target reached)──> HOLD ───────────────────────> VENT ──(complete)──> IDLE
  ^                                                 │                           │
  │                                                 │                           v
  └───────────────── plant reset ───────────────────┴─────────────────────── FAULT
```

| Event | From | To |
| --- | --- | --- |
| `plant start` | anything but `FAULT` | closes the valve, then `PULL`; `err not_ready` with `reason=no_target`, `reason=no_reading` or `reason=latch`. From `VENT` it abandons the release and pulls again |
| `plant stop` | anything | opens the valve and stops the pump; from `PULL`, `HOLD` or `VENT` that is `VENT` then `IDLE`, and from `IDLE` or `FAULT` the state does not move |
| `plant target=<kPa\|->` | anything but `FAULT` | the target changes; `-` while pulling or holding releases the chamber |
| target reached | `PULL` | `HOLD`, and the controller's countdown should start here |
| link loss, sensor loss, pump latch | any | `VENT`, then `IDLE` or `FAULT` |
| `plant reset` | `FAULT` | `VENT` then `IDLE` |
| `device reboot` | any | safe boot, i.e. `IDLE` with the valve open |

What a controller has to know:

* **The hold's duration is yours.** The device has no timer, no timeout and no maximum run
  time, so a run continues until you send `plant stop`. `HOLD` is the cue, not a countdown: it
  means the device's own deadband has decided the target is reached, which is the moment to start
  timing and the one thing you cannot work out for yourself, because the deadband is the
  device's loop. Timing it and drawing it are both yours: the device is not told how long a
  hold is, and has no command that takes a duration.
* **Faults latch.** Only `plant reset` clears one; the device never retries on its own.
* Venting finishes when the pressure is within 2 kPa of ambient, or after 30 s, whichever
  comes first. A vent timeout is logged, not an error, and the state still returns to `IDLE`.
* **The vent valve is normally open**: de-energised is open, so running a vacuum means
  energising the coil to close it. A crash, a brownout or a cut wire therefore vents the
  chamber whether or not the firmware noticed.
* A normally open solenoid draws current for the whole hold, which is why a long hold is the
  dominant battery cost rather than the pump. With no timer in the device, an unattended run
  holds until something stops it.
* Link loss releases the chamber, but it only detects a controller that leaves. One that hangs
  with the port open holds the run open.
* Duty is capped and slew limited, and those are the pump's only protections. A target deeper
  than `floor` — the pump's own reach, reported in `plant get` — pins the duty at the cap and
  nothing stops it, so read `floor` and keep targets no deeper than it.
* **Nothing but the splash and the battery appears on the panel unless you draw it.** The device
  has no page of its own, so a fault, a vent timeout, a low battery and the state itself are only
  on the wire. Poll `plant get` to see them, and draw them if you want them on the glass.

## 16. Modes

Two things can own the plant and the panel: the VM running a program, or a host with the port
open. `mode` says which, and it is the only thing in the protocol that gates commands rather than
arguments:

| Mode | Who owns the plant and the panel |
| --- | --- |
| `standalone` | the VM: it runs a program, and that program may draw |
| `peripheral` | the host: `plant` and `panel` are the host's |

| Available in | Commands |
| --- | --- |
| both modes | every read (`* get`, a field read), `help`, and everything under `device`, `link`, `vm` and `store` |
| `peripheral` only | `plant` writes and actions (`target`, `start`, `stop`, `reset`, `capture`), and every `panel` op |

A control command that arrives in stand-alone mode is `err not_ready mode=standalone`: it is a
valid command asking for something that mode does not give it, which is different from a bad
argument, and `vm mode=peripheral` is the answer. Reads stay available in both modes on purpose —
monitoring a program should never require taking the plant away from it.

| Command | Effect |
| --- | --- |
| `vm mode=peripheral` | stop the running program, vent, halt the VM, hand the plant and panel to the host |
| `vm mode=standalone` | start `default` (home) if nothing is running, and give the plant and panel back to the VM |
| `store run <name>` | stop the current program → vent → start `<name>`; switches to stand-alone if it has to |

Attaching to the serial port does **not** change the mode. A host that wants control asks for it,
so the device's own behavior never depends on a cable being plugged in.

## 17. Rates

Nothing is pushed: a controller that wants a readout polls `plant get`. Each poll is a ~140 byte
reply, so polling at 4–10 Hz is a trivial load at 115200 baud and gives a smoother readout
than the device could produce on its own. Each reply describes the machine as of the last
control tick (50 ms), so polling faster than that repeats itself.

Because replies are matched by counting, a controller that pipelines several commands in one
write should read one reply per command before concluding anything about state: `plant start`
followed immediately by `plant get` in the same batch may describe the machine as it was before
the `plant start` was handled. Sending `plant get` as its own command avoids the question
entirely.

Drawing is the one place the volume matters. A frame is twenty lines or more. Each loop
iteration drains everything the host has sent and services **every complete line** it received,
in arrival order, with nothing dropped and nothing reordered — so a whole frame written in one go
is handled in one pass. Replies are still exactly one per command, so a host that counts them
reads one per line; it just does not need to assume a batch is spread across iterations.

## 18. Migration from 0.5.0

`proto=2` is a break, not an addition: every actuator moved under a resource, every reply is
items, and `set` and the bare-verb actuators are gone. The move is mechanical.

| 0.5.0 | proto 2 |
| --- | --- |
| `info` | `device get` |
| `set log on` | `device log=on` |
| `set log off` | `device log=off` |
| `reboot` | `device reboot` |
| `status` | `plant get` |
| `set target 40` | `plant target=40` |
| `set target off` | `plant target=-` |
| `start` | `plant start` |
| `stop` | `plant stop` |
| `reset` | `plant reset` |
| `ambient` | `plant capture` |
| `draw <op> …` | `panel draw <op> …` |
| `flush` | `panel flush` |
| `set rotation 1` | `panel rotation=1` |
| `set invert on` | `panel invert=on` |
| `set term <<<END>>>` | `link term=<<<END>>>` |
| `set term off`, `set term -` | `link term=off` |
| `help` | `help`, or `help <resource>` for one resource's fields and actions |
| `draw help` | `help panel draw` |
| spec 06 `set mode <m>` | `vm mode=<m>` |
| spec 06 `list` | `store list` |
| spec 06 `cat <name> …` | `store cat <name> …` |
| spec 06 `rm <name>` | `store rm <name>` |
| spec 06 `run <name> k=v …` | `store run <name> k=v …` |
| spec 06 `params <name>` | `store params <name>` |
| spec 06 `set params <name> k=v …` | `store params <name> k=v …` |
| spec 06 `prog <name>` | `store put <name>` |

Three things changed that are not a rename, and a migrating controller has to know about each:

* **Replies changed shape.** `ok started` is `ok result=start state=PULL`; `ok drawn` is
  `ok result=draw op=line`; `ok target=off` is `ok target=-`. A controller that parsed the old
  payloads has to be rewritten, not re-parameterised.
* **New reads exist.** `device get`, `plant get`, `panel get`, `link get` and `vm get` report
  everything the device can be asked to do. `rotation`, `invert`, `term`, `mode` and the ambient
  reference used to be write-only or missing from the wire; a controller can now rebuild its view
  after reconnecting instead of assuming.
* **Failures say what was wrong.** `out_of_range` carries the field, the value and the bound;
  `not_ready` carries a reason or the mode. Nothing has to go to the diagnostics stream to be
  diagnosable, which matters because `device log=off` is a legitimate thing for a client to do.
