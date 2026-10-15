# Gardener serial protocol

Pressures are **kPa** everywhere. There is no wire version: `info` reports `fw`, and that is
the only marker for which firmware a controller is talking to.

The device is a serial peripheral in the strict sense: it never initiates, keeps no session,
and every state it has is either a command it was given or something it had to do for
safety. All control comes from the controller at the other end of the line, including how long
a run lasts.

## 1. The port

| Setting | Value |
| --- | --- |
| Transport | USB CDC (native USB, the USB-C socket) by default; UART0 on GPIO20/21 is a one line change in `App` |
| Baud | 115200, 8N1 — only meaningful on a real UART: USB CDC ignores it |
| Terminator | `\n`, or `\r` alone; `\r\n` is accepted and yields one empty line |
| Line limit | 128 bytes. A longer line is answered `err bad_argument` and swallowed, never truncated |

`CDCOnBoot=cdc` is part of the build, not a preference: it is what maps `Serial` to the
native USB CDC. Building without it (`esp32:esp32:esp32c3` alone) sends the whole interface
to UART0 on GPIO20/21, which is not the USB socket, and the device looks dead to a cable.
The FQBN is pinned in `firmware/gardener/sketch.yaml`, `scripts/arduino.ps1` and
`.vscode/arduino.json` for exactly that reason.

## 2. Connecting

1. Open the port. The device sends nothing until it is asked: there is no greeting and no
   handshake to wait for.
2. Send `info` and check `fw`.
3. Subscribe to nothing: `status` is polled (§10).

**Disconnecting stops the run and vents the chamber** (§9), because a controller that goes
away releases the chamber. The device only acts on it when the stream can report host
presence, which USB CDC can and a plain UART cannot.

## 3. Grammar

```
command  = verb [ SP argument ] LF
reply    = "ok" SP payload LF | "err" SP code [ SP detail ] LF
payload  = item *( SP item )
item     = key [ "=" value ]
key      = ALPHA *( ALPHA / DIGIT / "_" )
value    = 1*( ALPHA / DIGIT / "_" / "-" / "." / ":" / "%" / "|" )
```

* An item with no value is a flag: `ok started`, `ok stopped`, `err not_ready`.
* Values never contain a space and are never empty, so nothing needs quoting or escaping.
  `-` means "not applicable". The device enforces this: whitespace inside a value is written
  as `_`.
* `draw` is the one exception to that shape. It takes an operator, a fixed number of integer
  coordinates, and — for `draw text` alone — a string that runs to the end of the line with its
  spaces intact. It is recognised before anything is split for exactly that reason (§8).
* A line starting with `#` is ignored, so a script can carry comments.
* Commands are handled in arrival order and produce **exactly one** reply line each, which is
  what lets a controller match replies to commands by counting. Nothing is multi-line.
* Unknown reply keys must be ignored, which is what lets fields be added later.
* `help` is the one list-valued payload, with `|` between items.

## 4. Diagnostics

Diagnostics share the stream with replies, distinguished by the first character:

| Prefix | Meaning |
| --- | --- |
| `ok ` / `err ` | a reply to a command |
| `[` | a diagnostic record: `[I]`, `[W]`, `[E]` then text |

A controller reads until it sees a line that is not a `[` record. `set log off` silences
diagnostics entirely, so a controller that wants a strictly quiet channel can have one, and
`info` reports `log=on|off` so it knows what to expect.

## 5. Status payload

`status` replies carry this payload, with a stable key order:

| Key | Unit | Notes |
| --- | --- | --- |
| `kpa` | kPa | filtered absolute pressure, 2 dp |
| `vac` | kPa | `ambient - kpa`, clamped at 0, 2 dp |
| `pct` | % | `vac / ambient * 100`, 1 dp |
| `target` | kPa | `-` when unset |
| `floor` | kPa | deepest reachable target, `ambient - 60 kPa` |
| `duty` | % | pump duty, 1 dp |
| `state` | — | `IDLE`, `PULL`, `HOLD`, `VENT`, `FAULT` |
| `batt` | % | `-` when no battery is fitted |
| `sensor` | — | `1` when the MPRLS is answering |
| `fault` | — | `none`, `sensor` |
| `up` | ms | uptime |

```text
ok kpa=41.23 vac=60.10 pct=59.3 target=40.0 floor=41.3 duty=72.0 state=HOLD batt=78 sensor=1 fault=none up=123456
```

## 6. Info payload

`fw=0.5.0 chip=ESP32-C3 heap=298412 batt_v=3.94 batt_pct=78 log=on`

| Key | Notes |
| --- | --- |
| `fw` | firmware version, and the only version marker there is: `0.4.0` added the drawing commands (§8), `0.5.0` dropped `set hold` |
| `chip` | chip model |
| `heap` | free heap in bytes |
| `batt_v` | pack volts, 2 dp, `-` when no pack is fitted |
| `batt_pct` | pack percent, `-` when none |
| `log` | `on`/`off`, whether diagnostics are being written |

## 7. Commands

| Command | Reply | Notes |
| --- | --- | --- |
| `help` | `ok help=info\|status\|help\|start\|stop\|reset\|reboot\|ambient\|set\|draw\|flush` | the one list-valued payload |
| `info` | `ok fw=0.5.0 …` | device identity |
| `status` | `ok <status payload>` | one shot; poll it for a readout |
| `start` | `ok started` / `err not_ready` | closes the valve, then pulls to the target |
| `stop` | `ok stopped` | pump off, then vent. The normal way a run ends |
| `set target <kPa\|off>` | `ok target=40.0` / `ok target=off` | the only actuator command |
| `set ambient <kPa>` | `ok ambient=101.33` | rejected when outside what the part can measure |
| `set log <on\|off>` | `ok log=on` / `ok log=off` | diagnostics on this stream |
| `set rotation <0-3>` | `ok rotation=1` | panel orientation; `out_of_range` outside 0-3 |
| `set invert <on\|off>` | `ok invert=on` / `ok invert=off` | swap ink and paper on the whole panel |
| `ambient` | `ok ambient=101.33` | capture the current reading as the reference |
| `draw <op> <arguments…>` | `ok drawn` | one drawing operation into the panel's buffer (§8) |
| `draw help` | `ok ops=clear\|pixel\|…` | the drawing operators, since `help` has no room for them |
| `flush` | `ok flushed` | the frame boundary: buffer to glass, battery composited |
| `reset` | `ok reset` | clears the latched fault, then vents |
| `reboot` | `ok rebooting` | boots safe: pump off, valve open, nothing restored |

```text
-> set target 40
<- ok target=40.0
-> start
<- ok started
-> status
<- ok kpa=40.21 vac=61.07 pct=60.3 target=40.0 floor=41.3 duty=72.0 state=HOLD batt=78 sensor=1 fault=none up=45000
-> stop
<- ok stopped
```

**`set target` is the only actuator command.** There is no `set duty`: a second way to drive
the pump would mean a second control path to reason about. Characterizing the pump therefore
needs a temporary build. That trade was made deliberately, so nobody should go looking for a
raw duty command that is not there.

Vent margin and timeout, the PID gains, the deadband, the duty cap, the slew limit and the
battery thresholds are all in `core/Config.h`. They are not commands, so the command surface
stays this table. The deadband matters to a controller: it is what decides that the target has
been reached, and it is not on the wire.

### Error codes

Every `err` is one of these five, so a controller can switch on the code instead of parsing
prose:

| Code | Meaning |
| --- | --- |
| `unknown_command` | not a command, an unknown `set` field, or an unknown `draw` operator |
| `bad_argument` | an argument is missing, does not parse, is outside a coordinate's range, or is left over; or the line was too long |
| `out_of_range` | the value parses but is not acceptable: `set rotation` outside 0-3, `draw text` size outside 1-8, a target below the loop's floor, an ambient the part cannot measure |
| `not_ready` | valid request that cannot be honoured now: `start` with no target, no reading, or a latched fault |
| `unsupported` | the feature does not exist in this firmware: `set duty`, `set period`, `telemetry` |

## 8. Drawing on the panel

The display is a 128x64 monochrome OLED, and from this side of the cable it is a buffer and a
frame boundary. `draw` writes into the buffer; nothing reaches the glass until `flush`, which
pushes the whole buffer at once. So a frame is however many operations the host wants, then one
`flush`, and a host can redraw at whatever rate it likes without a partial frame appearing.

```
-> draw clear
<- ok drawn
-> draw text 0 0 2 0 41.3
<- ok drawn
-> draw rect 0 40 100 8
<- ok drawn
-> draw fillrect 1 41 72 6
<- ok drawn
-> flush
<- ok flushed
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

There are eighteen of them. `draw help` answers with that list of names, and that is the thing to
ask: the device reporting what it can draw, in one line, beats trusting this table from a
distance.

Every operator answers `ok drawn`, including `clear`. An operator that is not in the list is
`unknown_command`.

**Coordinates** are panel pixels, origin top left, signed 16-bit. They are checked against that
range and nothing else: a coordinate off the panel is not an error, and what becomes of it is the
panel library's business. Orientation 0 and 2 are 128 wide by 64 high, orientation 1 and 3 swap
that. Nothing on the wire reports the geometry or the size of a string — the panel is a known
part and the font is a known font, so layout is the host's arithmetic to do.

**Text** is the one operator whose last argument is a string: everything after `inverted` is the
string, verbatim, spaces and all, to the end of the line. That is why `draw` is recognised before
the line is split for tokens, and why a string cannot be followed by anything. `size` scales the
device's built-in font and is 1 to 8; anything else is `out_of_range`. There is no font choice —
the device ships one font and `size` is a scale — and `inverted` is `0` for ink glyphs on
whatever is already there, `1` for paper glyphs on an ink block, which is how a label gets a
filled background.

**Two things on the panel are not the host's.** The device draws the boot splash before any host
has spoken, and it composites the battery on every `flush`, after the host's frame. Drawing over
either does not remove it: the battery is put back on the way out, and the splash only goes when
the host draws a frame over it.

**The buffer is not cleared for you.** A frame is whatever is in the buffer when `flush` arrives,
so a redraw starts with `draw clear` or deliberately draws over the last frame. `set rotation`
and `set invert` are properties of the glass rather than of a frame, so neither needs a `flush`
and both survive one. `reboot` resets all of it: empty buffer, rotation 0, ink normal, and the
splash drawn again.

**Three things are absent on purpose.** They exist on the device's own drawing interface but have
no command, and a host should know they are not coming:

* **No bitmap blit.** Icons are composed from the shape operators, not sent as images.
* **No text measurement.** The host owns layout, so it knows or reserves the width of its own
  strings.
* **No `clearRect`.** There is no operator that puts paper back over part of a frame, because
  every filling operator lays ink. Erasing anything means `clear` and redrawing the frame.

## 9. State model

The controller owns the target, the duration and when to start and stop. The device owns what
happens when nobody is watching, and nothing else.

```
        start                                              stop / link loss / fault
IDLE ──────────> PULL ──(target reached)──> HOLD ───────────────────> VENT ──(complete)──> IDLE
  ^                                              │                      │
  │                                              │                      v
  └──────────────────── reset ───────────────────┴─────────────────── FAULT
```

| Event | From | To |
| --- | --- | --- |
| `start` | anything but `FAULT` | closes the valve, then `PULL`; `err not_ready` with no target, no valid reading, or a latched fault. From `VENT` it abandons the release and pulls again |
| `stop` | anything | opens the valve and stops the pump; from `PULL`, `HOLD` or `VENT` that is `VENT` then `IDLE`, and from `IDLE` or `FAULT` the state does not move |
| `set target <kPa\|off>` | anything but `FAULT` | the target changes; `off` while pulling or holding releases the chamber |
| target reached | `PULL` | `HOLD`, and the controller's countdown should start here |
| link loss, sensor loss, pump latch | any | `VENT`, then `IDLE` or `FAULT` |
| `reset` | `FAULT` | `VENT` then `IDLE` |
| `reboot` | any | safe boot, i.e. `IDLE` with the valve open |

What a controller has to know:

* **The hold's duration is yours.** The device has no timer, no timeout and no maximum run
  time, so a run continues until you send `stop`. `HOLD` is the cue, not a countdown: it means
  the device's own deadband has decided the target is reached, which is the moment to start
  timing and the one thing you cannot work out for yourself, because the deadband is the
  device's loop. Timing it and drawing it are both yours: the device is not told how long a
  hold is, and has no command that takes a duration.
* **Faults latch.** Only `reset` clears one; the device never retries on its own.
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
* Duty is capped and slew limited, and those are the pump's only protections. A target below
  `floor` pins the duty at the cap and nothing stops it, so keep targets above `floor` if you
  want a ceiling.
* **Nothing but the splash and the battery appears on the panel unless you draw it.** The device
  has no page of its own, so a fault, a vent timeout, a low battery and the state itself are only
  on the wire. Poll `status` to see them, and draw them if you want them on the glass.

## 10. Rates

Nothing is pushed: a controller that wants a readout polls `status`. Each poll is a ~120 byte
reply, so polling at 4–10 Hz is a trivial load at 115200 baud and gives a smoother readout
than the device could produce on its own. Each reply describes the machine as of the last
control tick (50 ms), so polling faster than that repeats itself.

Because replies are matched by counting, a controller that pipelines several commands in one
write should read one reply per command before concluding anything about state: `start`
followed immediately by `status` in the same batch may describe the machine as it was before
the `start` was handled. Sending `status` as its own command avoids the question entirely.

Drawing is the one place the volume matters. A frame is twenty lines or more, and the device
services **eight lines per loop iteration**, so a whole frame is not consumed in one pass: the
rest stays in the stream and is handled on later iterations, in order, with nothing dropped and
nothing reordered. That pacing is deliberate — the sensor and the PID share the loop, and a host
that floods the port should not put them behind a burst of drawing. A host that sends a frame
and waits for its `flush` reply sees the frame land normally; a host that counts replies still
reads one per line rather than assuming a batch is drained at once.
