# Gardener serial protocol

Pressures are **kPa** everywhere. `kpa` is the vacuum depth below ambient — 0 at rest, rising as
the chamber is pumped down — and a **target** is a depth on the same ruler. The ambient reference
belongs to the sensor: the MPRLS is absolute, so it takes one from its first reading when it comes
up, and nothing on the wire sets or restates one. The wire revision is `proto`, reported by
`device get all`: it changes when a command's *meaning* changes, not when commands are added. `fw`
is the build.

The device is a serial peripheral in the strict sense: it never initiates, keeps no session,
and every state it has is either a command it was given or something it had to do for
safety. All control comes from the controller at the other end of the line, including how long
a run lasts.

This document is the **proto 4** surface. It replaces the proto 3 wire wholesale: the first token
is a **domain**, each domain owns its own verbs and grammar, and replies echo the domain and verb
they answer. `device`, `vacuum`, `buzzer` and `help` are the domains.

## 1. The port

| Setting | Value |
| --- | --- |
| Transport | USB CDC (native USB, the USB-C socket) by default; UART0 on GPIO43/44 is a one line change in `App` |
| Baud | 115200, 8N1 — only meaningful on a real UART: USB CDC ignores it |
| Terminator | `LF` only. A `\r` immediately before the `LF` is ignored, so `\r\n` works |
| Header limit | 128 bytes on a command header. A longer header is answered `err - - bad_argument` and discarded, never truncated |
| Body | A domain may define a command body that follows the header; the header's count is authoritative. The 128-byte limit applies to the header only |

There is no command that changes the terminator, and no `link` domain. The line ends at `LF`, always.

`USBMode=hwcdc,CDCOnBoot=cdc` is part of the build, not a preference: `CDCOnBoot` is what
maps `Serial` to the chip's native USB CDC. Building without it (`esp32:esp32:esp32s3`
alone) sends the whole interface to UART0 on GPIO43/44, which is not the USB socket, and the
device looks dead to a cable. The FQBN is pinned in `firmware/gardener/sketch.yaml`,
`scripts/arduino.ps1` and `.vscode/arduino.json` for exactly that reason.

## 2. Connecting

1. Open the port. The device sends nothing until it is asked: there is no greeting and no
   handshake to wait for.
2. Send `device get all` and check `proto` and `fw`. `proto=4` is this document; a controller that
   does not recognise the number should stop rather than guess.
3. Ask for what you need: `vacuum get all` is the readout, and it is polled (§11). Nothing is
   pushed.
4. `help` lists the domains. `help <domain>` lists that domain's topics, and
   `help <domain> <topic>` explains one (§8). A controller that reads `help` once at connect has
   no reason to hardcode a value range again.

**Disconnecting stops the run and vents the chamber** (§10), because a controller that goes away
releases the chamber. The device only acts on it when the stream can report host presence, which
USB CDC can and a plain UART cannot. There is no command that sets or reports this; it is a
property of the transport, not a field on the wire.

## 3. Grammar

```
command = header [ body ]
header  = domain SP verb *( SP arg ) LF
arg     = bare / quoted
bare    = printable except space, DQUOTE, backslash
quoted  = DQUOTE *( printable / "\" ( DQUOTE / "\" ) ) DQUOTE
body    = %x00-FF{ N }        ; N is declared by the domain's grammar
```

* Every command starts with a **domain**, then a **verb** owned by that domain, then positional
  arguments. There is no universal `field=value` grammar and no `get`/`set` imposed by the
  dispatcher.
* The dispatcher reads only the domain token. Everything after it belongs to that domain, which
  tokenizes its own verb and arguments — and, where it defines one, its body.
* Domain and verb are case-insensitive; field names are matched case-insensitively too. Values stay
  case-sensitive unless the domain parses them as a keyword (`on`/`off`, unit tokens).
* A quoted argument carries spaces and quotes: `buzzer play "a4:1000 c6:500"`. Inside a quoted
  value `\"` is a quote and `\\` is a backslash.
* `-` means unset or not applicable. `off` means explicitly disabled, where a field has a disabled
  state. `""` is an empty string.
* A `#` at the start of a line is a comment, and a blank line gets no reply.
* `help` is a domain with its own grammar: `help`, `help <domain>`, `help <domain> <topic>`.
* A command may have a body after its header. Most domains do not; the future `fs write` will,
  with the header `fs write <path> <bytes>` followed by exactly `<bytes>` raw bytes. The body may
  contain any byte, including `LF`; the header's count is authoritative.

## 4. Replies

Every reply begins with a header line. The header is the same for every domain; what follows it
depends on the body the header announces.

```
header = "ok"  SP domain SP verb *( SP item ) LF
       | "err" SP domain SP verb SP code *( SP item ) LF
item   = key "=" value
key    = ALPHA *( ALPHA / DIGIT / "_" / "-" )

reply  = header [ body ]
body   = rows | bytes
rows   = row{ N } end
bytes  = %x00-FF{ N } end
row    = item *( SP item ) LF
end    = "end" SP domain SP verb *( SP item ) LF
```

* Every header echoes the domain and verb it answers, so a controller can match a reply without
  counting. `ok` means accepted; `err` carries a code and never has a body.
* An `ok` header announces its body with `body=rows rows=N` or `body=bytes bytes=N`. No `body=`
  item means the header is the whole reply, which is what most commands use.
* `body` is a reserved key; a domain may not use it for data. `rows` and `bytes` are the body's
  count, not ordinary fields, when `body=` is present.
* The `end` line repeats the domain, verb and `body=`; its count is what was actually sent, so a
  body that stops early is detectable. A short body also carries `error=<code>`.
* An `err` is a complete reply. Once an `ok` header has announced a body, the body and its `end`
  line follow; a failure goes in the `end` line, not in a second header.
* The body shapes above are the ones the protocol defines so far; a domain's help page names the
  shape it uses.
* A row is items. `help` uses a rows body; the future `fs list` will too. A bytes body may contain
  any byte, including `LF`; a controller reads exactly `N` bytes, then the `end` line. `body=bytes`
  is reserved for the future `fs read`; no current domain uses it.
* Values in headers and rows are bare unless they contain a space, `"` or `\`, in which case they
  are quoted and escaped. There is no whitespace-to-`_` substitution.
* An action in the current domains replies with the fields it changed:
  `ok vacuum start state=PULL`. That is a convention of those domains, not a rule of the framing; a
  verb answers with whatever its own help page says.
* Unknown domain: `err - - unknown_domain token=<token>`. Missing verb:
  `err <domain> - bad_argument`. Unknown verb: `err <domain> <verb> unknown_verb`.
* Diagnostics stay `[I]`/`[W]`/`[E]` records, distinct from replies; `device set log off` silences
  them. Replies are never silenced.

## 5. `device` — the machine and this stream

| Field | Access | Values | Notes |
| --- | --- | --- | --- |
| `proto` | r | int | wire revision: `4` for this document |
| `fw` | r | text | firmware version |
| `chip` | r | text | chip model |
| `heap` | r | bytes | free heap |
| `batt_v` | r | volts, 2 dp, `-` | pack volts, `-` when no pack is fitted |
| `batt_pct` | r | percent, `-` | pack charge, `-` when no pack is fitted |
| `log` | r | `on` / `off` | diagnostics on this stream |
| `unit` | r | `kpa` / `psi` / `inhg` / `mmhg` | the unit the panel prints pressure in. Stored in `/settings.ini` and restored at boot |
| `display-size` | r | `widthxheight` | panel geometry in pixels |

| Verb | Reply | Notes |
| --- | --- | --- |
| `get all` | every field | the readout of the machine |
| `get <field>` | that field | e.g. `device get display-size` |
| `set log <on\|off>` | `ok device set log=<on\|off>` | diagnostics on this stream |
| `set unit <kpa\|psi\|inhg\|mmhg>` | `ok device set unit=<token>` | the panel's unit; stored across reset |
| `reboot` | `ok device reboot` | safe boot, then restart. The reply is written before the restart |

```
-> device get all
<- ok device get proto=4 fw=0.7.0 chip=ESP32-S3 heap=298412 batt_v=3.94 batt_pct=78 log=on unit=kpa display-size=128x64
-> device get display-size
<- ok device get display-size=128x64
-> device set log off
<- ok device set log=off
-> device set unit mmhg
<- ok device set unit=mmhg
-> device reboot
<- ok device reboot
```

`device set unit` is the one write that survives a reset: the value is written to the `[device]`
section of `/settings.ini` and read back at boot. A filesystem that will not take the write still
answers with the unit in force for this session, and the boot log reports the failed mount. The
machine and this wire stay in kPa either way — a display unit is a preference about the glass, not
a second way to state a pressure.

Battery state belongs to the device, not the vacuum system: `batt_v` and `batt_pct` are device
fields. A pack that is not fitted reads `-` for both.

`panel` is gone as a domain. Its one wire fact is `device get display-size`. The wire still does
not draw on the panel.

## 6. `vacuum` — the machine that holds pressure

| Field | Access | Values | Notes |
| --- | --- | --- | --- |
| `kpa` | r | kPa, 2 dp | filtered vacuum depth below ambient: 0 at rest, rising as the chamber is pumped down |
| `target` | r | kPa below ambient, or `-` | the current target; `-` when none is set |
| `floor` | r | kPa, 1 dp | deepest reachable target, `PUMP_MAX_SUCTION_KPA` below the reference |
| `duty` | r | %, 1 dp | pump duty |
| `state` | r | `IDLE` / `PULL` / `HOLD` / `VENT` / `FAULT` | |
| `sensor` | r | `1` / `0` | `1` when the MPRLS is answering |
| `fault` | r | `none` / `sensor` | |
| `up` | r | ms | uptime |

| Verb | Reply | Notes |
| --- | --- | --- |
| `get all` | every field | the vacuum readout |
| `get <field>` | that field | e.g. `vacuum get state` |
| `target <kPa\|->` | `ok vacuum target target=<kPa>` | `vacuum target 40` pulls to 40 kPa below ambient; `vacuum target -` releases. Shallower than `CONTROL_MIN_TARGET_KPA` is `err vacuum target out_of_range …`; deeper than `floor` is accepted, and only pins the duty at the cap |
| `start` | `ok vacuum start state=PULL` / `err vacuum start not_ready reason=…` | closes the valve, then pulls to the target. Reasons: `no_target`, `no_reading`, `latch` |
| `stop` | `ok vacuum stop state=VENT` | pump off, then vent. The normal way a run ends |
| `reset` | `ok vacuum reset state=VENT` | clears the latched fault, then vents |

```
-> vacuum get all
<- ok vacuum get kpa=40.07 target=40.0 floor=60.0 duty=72.0 state=HOLD sensor=1 fault=none up=45000
-> vacuum target 40
<- ok vacuum target target=40.0
-> vacuum start
<- ok vacuum start state=PULL
-> vacuum target -
<- ok vacuum target target=-
-> vacuum stop
<- ok vacuum stop state=VENT
```

`target` is the only actuator verb. There is no `duty` write: a second way to drive the pump
would mean a second control path to reason about. Characterizing the pump therefore needs a
temporary build. That trade was made deliberately, so nobody should go looking for a raw duty
command that is not there.

Vent margin and timeout, the PID gains, the deadband, the duty cap, the slew limit and the
battery thresholds are all in `core/Config.h`. They are not commands, so the command surface
stays this table. The deadband matters to a controller: it is what decides that the target has
been reached, and it is not on the wire. `CONTROL_MIN_TARGET_KPA` is the other one worth
knowing: it is the shallowest target the loop will accept, and it is reported as `min=` on
`help vacuum target` as well as in the `out_of_range` reply.

## 7. `buzzer` — the piezo

| Field | Access | Values | Notes |
| --- | --- | --- | --- |
| `tone` | r | `<hz>,<ms>` or `-` | the current frequency and remaining ms, or `-` when silent |
| `play` | r | a quoted sequence or `-` | the canonical sequence while it runs |
| `playing` | r | `on` / `off` | a tone or sequence is still running; a rest still counts |

| Verb | Reply | Notes |
| --- | --- | --- |
| `get all` | every field | |
| `get <field>` | that field | |
| `tone <hz> <ms>` | `ok buzzer tone tone=<hz>,<ms>` | `ms=0` runs until `buzzer stop` |
| `play "<note>:<ms> …"` | `ok buzzer play play="<sequence>"` | note/duration pairs separated by spaces; the value is quoted |
| `stop` | `ok buzzer stop playing=off` | silence and stop |

```
-> buzzer get all
<- ok buzzer get tone=- play=- playing=off
-> buzzer tone 2000 500
<- ok buzzer tone tone=2000,500
-> buzzer get all
<- ok buzzer get tone=2000,412 play=- playing=on
-> buzzer play "a4:1000 c6:500"
<- ok buzzer play play="a4:1000 c6:500"
-> buzzer get all
<- ok buzzer get tone=440,610 play="a4:1000 c6:500" playing=on
-> buzzer stop
<- ok buzzer stop playing=off
```

`play` takes note names with an optional `#` or `b`, then one octave digit (`a4`, `c#5`, `bb3`);
`r` is a rest. The notation is `note:duration` pairs separated by spaces, and nothing else — a
comma, a missing colon, or a space between a note and its duration is refused. On the wire the
whole sequence is one quoted argument, so the spaces survive. Durations are milliseconds and must
be positive. The frequency uses equal temperament with A4 = 440 Hz; a note outside the buzzer's
100–10000 Hz range is a bad argument, not a clamp.

A tone or sequence continues until it is stopped or its own time runs out; the controller times a
long alert by choosing `ms`, and `ms=0` is the explicit way to say "until I stop it".

## 8. `help` — discovery

`help` is a domain like every other. It tokenizes its own optional domain and topic arguments,
looks the target up in the domain list, and calls that domain's `describe()` or `help(topic)`.
`HelpDomain` writes the top-level page; a target domain's `help(topic)` writes its own page.

- `help` -> `ok help domains body=rows rows=N`, rows `domain=<name> topics=<n> about="<text>"`,
  `end help domains body=rows rows=N`.
- `help <domain>` -> `ok help <domain> body=rows rows=N`, rows
  `topic=<name> args="<syntax>" about="<text>"`, `end help <domain> body=rows rows=N`.
- `help <domain> <topic>` -> `ok help <domain> topic=<topic> body=rows rows=N`, then `arg=`,
  `reply=`, `error=` or `field=` rows, then
  `end help <domain> topic=<topic> body=rows rows=N`.
- `help <domain>` for a domain that does not exist is
  `err help <domain> unknown_domain token=<domain>`; an unknown topic under a real domain is
  `err help <domain> unknown_topic token=<topic>`.
- Every topic a page names is a topic the domain accepts, so a topic taken from `help` never
  produces `unknown_topic`.

```
-> help
<- ok help domains body=rows rows=4
<- domain=device topics=3 about="the machine and this stream"
<- domain=vacuum topics=5 about="pressure and the run"
<- domain=buzzer topics=4 about="the piezo"
<- domain=help topics=1 about="this help"
<- end help domains body=rows rows=4
-> help vacuum
<- ok help vacuum body=rows rows=5
<- topic=get args=all|<field> about="read every field or one"
<- topic=target args=<kPa|-> about="set or clear the target"
<- topic=start args="" about="close the valve and pull"
<- topic=stop args="" about="stop the pump and vent"
<- topic=reset args="" about="clear a latched fault"
<- end help vacuum body=rows rows=5
-> help vacuum start
<- ok help vacuum topic=start body=rows rows=2
<- reply=state
<- error=not_ready
<- end help vacuum topic=start body=rows rows=2
```

## 9. Errors

Every `err` is one of these codes, so a controller can switch on the code instead of parsing
prose, and each carries items naming what was involved, where there is something to name:

| Code | Meaning |
| --- | --- |
| `unknown_domain` | the first token is not a domain: `err - - unknown_domain token=set` |
| `unknown_verb` | the domain does not accept that verb: `err vacuum frobnicate unknown_verb` |
| `unknown_topic` | the help domain does not have that topic: `err help vacuum frobnicate unknown_topic token=frobnicate` |
| `bad_argument` | an argument is missing, does not parse, is left over or is empty; or the header was too long |
| `out_of_range` | the value parses but is not acceptable: `err vacuum target out_of_range field=target value=1 min=5.0` |
| `not_ready` | valid, and cannot be honoured now: `reason=no_target`, `reason=no_reading`, `reason=latch` |
| `unsupported` | the protocol defines it and this build cannot do it |

`not_found` is reserved for the future `fs` domain.

```
-> vacuum start
<- err vacuum start not_ready reason=no_target
-> vacuum target 1
<- err vacuum target out_of_range field=target value=1 min=5.0
-> vacuum frobnicate
<- err vacuum frobnicate unknown_verb
-> frobnicate get all
<- err - - unknown_domain token=frobnicate
-> device get frobnicate
<- err device get bad_argument token=frobnicate
-> help vacuum frobnicate
<- err help vacuum unknown_topic token=frobnicate
```

A field named with no value to give it is `bad_argument token=<field>`, and a command with no verb
at all is `bad_argument` with nothing to name. A spelling that used to work in proto 3 is
`unknown_domain` or `unknown_verb`, so a controller written against the old wire fails on its
first command instead of misreading a reply.

## 10. State model

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
| `vacuum target <kPa\|->` | anything but `FAULT` | the target changes; `-` while pulling or holding releases the chamber |
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
  whether or not the firmware noticed. `NormallyClosedValve` in `app/Build.cpp` is the alternative
  and inverts all of that — de-energised is sealed.
* A normally open solenoid draws current for the whole hold, which is why a long hold is the
  dominant battery cost rather than the pump. A normally closed part draws it at rest instead,
  holding the valve open to vent. With no timer in the device, an unattended run holds until
  something stops it.
* Link loss releases the chamber, but it only detects a controller that leaves. One that hangs
  with the port open holds the run open.
* Duty is capped and slew limited, and those are the pump's only protections. A target deeper
  than `floor` — the pump's own reach, reported in `vacuum get all` — pins the duty at the cap and
  nothing stops it, so read `floor` and keep targets no deeper than it.
* **The panel is the device's.** It shows the state, its icon, both pressures, the battery and the
  two ways in — serial and Bluetooth — so those are readable at the bench, and it falls back to
  the boot mark when no controller is attached. There is no drawing command, so everything else — a
  vent timeout, the duty, the fault code — is only on the wire. Poll `vacuum get all` to see it.

## 11. Rates

Nothing is pushed: a controller that wants a readout polls `vacuum get all`. Each poll is a
~150 byte reply, so polling at 4–10 Hz is a trivial load at 115200 baud and gives a smoother
readout than the device could produce on its own. Each reply describes the machine as of the last
control tick (50 ms), so polling faster than that repeats itself.

Because replies are matched by counting, a controller that pipelines several commands in one
write should read one reply per command before concluding anything about state: `vacuum start`
followed immediately by `vacuum get all` in the same batch may describe the machine as it was
before the `vacuum start` was handled. Sending `vacuum get all` as its own command avoids the
question entirely.

Each loop iteration drains everything the host has sent and services **every complete header** it
received, in arrival order, with nothing dropped and nothing reordered — so a batch written in one
go is handled in one pass. Replies are still exactly one per command, so a host that counts them
reads one per line; it just does not need to assume a batch is spread across iterations.

## 12. Migration from proto 3

`proto=4` is a deliberate break. The moves are mechanical, but a controller that parsed proto 3
payloads has to be rewritten rather than re-parameterised.

| proto 3 | proto 4 |
| --- | --- |
| `device get` | `device get all` |
| `device log=off` | `device set log off` |
| `device unit=mmhg` | `device set unit mmhg` |
| `vacuum get` | `vacuum get all` |
| `vacuum target=40` | `vacuum target 40` |
| `vacuum target=-` | `vacuum target -` |
| `vacuum kpa` | `vacuum get kpa` |
| `vacuum start`, `vacuum stop`, `vacuum reset` | unchanged |
| `panel get` | `device get display-size` |
| `link term=<<<END>>>`, `link term=off` | no replacement: the line ends at `LF` |
| `link attached` | no replacement: host presence is a transport property |
| `buzzer tone=2000,500` | `buzzer tone 2000 500` |
| `buzzer play=a4:1000_c6:500` | `buzzer play "a4:1000 c6:500"` |
| `buzzer tone=-` | `buzzer stop` |
| `help vacuum` | `help vacuum` (fields and actions are now help topics) |
| `ok result=start state=PULL` | `ok vacuum start state=PULL` |
| `ok target=40.0` | `ok vacuum target target=40.0` |
| `err out_of_range field=target value=1 min=5.0` | `err vacuum target out_of_range field=target value=1 min=5.0` |

Four things changed that are not a rename:

* **Replies changed shape.** Every reply echoes the domain and verb; `result=` is gone, and pages
  use `body=rows rows=N` plus `end`. Values with spaces are quoted instead of written with `_`.
* **`get all` is explicit.** A bare `get` is `bad_argument`; `all` is a keyword, not a field.
* **`link` and `panel` are gone.** There is no custom terminator, no attachment field and no panel
  domain. `device get display-size` is the only panel fact on the wire.
* **Battery moved to `device`.** `vacuum get` used to report `batt`; proto 4 reports `batt_v` and
  `batt_pct` on `device get all`, and the vacuum domain has no battery field.
