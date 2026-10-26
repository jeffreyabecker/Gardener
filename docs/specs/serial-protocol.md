# Gardener serial protocol

Pressures are **kPa** everywhere. `pressure_kpa` is the vacuum depth below ambient — 0 at rest, rising as
the chamber is pumped down — and a **target** is a depth on the same ruler. The ambient reference
belongs to the sensor: the gauge part's output is analog, so its driver takes one from its first
reading when it comes up, and nothing on the wire sets or restates one.

The wire is **unsettled**. `proto` is `1` and does not move with each revision yet; a controller
must not treat it as a compatibility guarantee. `fw` is the build.

The device is a serial peripheral in the strict sense: it never initiates, keeps no session,
and every state it has is either a command it was given or something it had to do for
safety. Every link may command the machine; a write applies on the control task's next beat and the
last write wins. A run still ends as a release at its deadline whether or not a controller is
talking.

The first token is a **domain**. `device`, `vacuum`, `buzzer` and `fs` are the domains, and the
panel's `routine` domain is in [routine links](./routine-links.md). There is no `help` domain: this
document is the interface.

## 1. The port

| Setting | Value |
| --- | --- |
| Transport | USB CDC (native USB, the USB-C socket) by default; UART0 on GPIO43/44 is a one line change in `App` |
| Baud | 115200, 8N1 — only meaningful on a real UART: USB CDC ignores it |
| Terminator | `LF` only. A `\r` immediately before the `LF` is ignored, so `\r\n` works |
| Header limit | 128 bytes on the domain token, and 128 on the command after it. A longer command is answered `err bad_argument` and discarded, never truncated |
| Body | A domain may define a command body that follows the header; the header's count is authoritative. The 128-byte limit applies to the header only |

There is no command that changes the terminator, and no `link` domain. The line ends at `LF`, always.

`USBMode=hwcdc,CDCOnBoot=cdc` is part of the build, not a preference: `CDCOnBoot` is what
maps `Serial` to the chip's native USB CDC. Building without it (`esp32:esp32:esp32s3`
alone) sends the whole interface to UART0 on GPIO43/44, which is not the USB socket, and the
device looks dead to a cable. The FQBN is pinned in `firmware/gardener/sketch.yaml`,
`scripts/arduino.py` and `.vscode/arduino.json` for exactly that reason.

## 2. Connecting

1. Open the port. The device sends nothing until it is asked: there is no greeting and no
   handshake to wait for.
2. Send `device get` and check `proto` and `fw`. `proto=1` is this document.
3. Ask for what you need: `vacuum get` is the readout, and it is polled (§12). Nothing is pushed.

Connecting has no handshake and grants nothing extra: any link with the port open may read and write,
and another link may command the machine at the same time. There is no claim, no session and no
hand-back to make.

The device can only notice a drop when the stream reports host presence, which USB CDC can and a
plain UART cannot; there is no command that sets or reports presence. A drop is logged and nothing
else: a run continues to its hold, and the panel and the radio are unaffected.

## 3. Grammar

```
command = header [ body ]
header  = domain SP verb *( SP arg ) LF
arg     = token
token   = printable except SP
body    = %x00-FF{ N }        ; N is declared by the domain's grammar
```

* Every command starts with a **domain**, then a **verb** owned by that domain, then positional
  arguments. There is no universal `field=value` grammar and no `get`/`set` imposed by the
  dispatcher.
* The dispatcher reads only the domain token. Everything after it belongs to that domain, which
  tokenizes its own verb and arguments — and, where it defines one, its body.
* There is no quoting and no escaping. An argument is one token: printable, no spaces.
* A verb that takes free-form text takes the rest of the line as its final argument. The only one
  is `buzzer play`: `buzzer play a4:1000 c6:500`.
* Domain and verb are case-insensitive. Values stay case-sensitive unless the domain parses them
  as a keyword (`on`/`off`, unit tokens).
* `-` means unset or not applicable. `off` means explicitly disabled.
* A `#` at the start of a line is a comment, and a blank line gets no reply.
* A command may have a body after its header. Most domains do not; `fs write` does, with the
  header `fs write <path> <bytes>` followed by exactly `<bytes>` raw bytes. The body may contain
  any byte, including `LF`; the header's count is authoritative.

## 4. Replies

Every reply starts with one line. A body follows only when the reply defines one: a count on the
line for `fs read`, or the rows and blank line for `fs list`.

```
reply  = "ok"  [ SP items ] LF
       | "err" SP code [ SP detail ] LF
items  = item *( SP item )
item   = key "=" value
value  = token
detail = token / items
```

* One reply per command, in arrival order. There is no domain or verb echo: a controller that
  sends one command at a time reads one reply, and a controller that pipelines counts replies in
  the order it sent the commands.
* `ok` means accepted. It carries the state the verb changed, using the keys `get` reports:
  `ok state=PULL`, `ok target=40.0`, `ok tone=2000,500`, `ok playing=on`, `ok log=WE`.
* `err` carries a code and the detail that code defines. A code naming one thing takes it bare
  (`err unknown_verb frobnicate`); a code with several values uses items
  (`err out_of_range field=target value=101 max=60.0`).
* A body is announced by a count on the `ok` line. `bytes=N` is followed by exactly N raw bytes;
  `fs read` is the only reply that uses it, and a zero count sends no body. `fs list`, `routine list`
  and `routine settings` are the bodies without a count: their rows follow `ok` and a blank line ends
  them.
* Values never contain a space, so a reply line splits on spaces. The one free-form command
  argument, `buzzer play`'s sequence, is never echoed back.
* Diagnostics stay `[I]`/`[W]`/`[E]` records, distinct from replies; the `log` switch chooses which
  of the three are written, and a change applies to the next record. Replies are never silenced.

## 5. `device` — the machine and this stream

`device get` reports every field, in this order:

| Field | Values | Notes |
| --- | --- | --- |
| `proto` | int | wire revision: `1` for this document |
| `fw` | text | firmware version |
| `chip` | text | chip model |
| `heap` | bytes | free heap |
| `batt_v` | volts, 2 dp, `-` | pack volts, `-` when no pack is fitted |
| `batt_pct` | percent, `-` | pack charge, `-` when no pack is fitted |
| `log` | set of `I`/`W`/`E`, or `-` | the diagnostics levels written, in canonical order; `IWE` at boot, not stored |
| `unit` | `kpa` / `psi` / `inhg` / `mmhg` | the unit the panel prints pressure in. Stored in `/cfg/settings.ini`, under `[device]`, and restored at boot |
| `caps` | int, bitmask | which assemblies are fitted: `1` panel, `2` battery, `4` a fail-safe, normally open valve |

| Verb | Reply | Notes |
| --- | --- | --- |
| `get` | every field | the readout of the machine; takes no argument |
| `set log <levels>` | `ok log=<levels>` | the levels to write, in any case and order; the reply is canonical |
| `set unit <kpa\|psi\|inhg\|mmhg>` | `ok unit=<token>` | the panel's unit; stored across reset |
| `reboot` | `ok` | safe reset: stop and vent, wait up to 2 s for `VENT`, then restart. The reply is written before the restart |

```
-> device get
<- ok proto=1 fw=0.7.0 chip=ESP32-S3 heap=298412 batt_v=3.94 batt_pct=78 log=IWE unit=kpa caps=3
-> device set log WE
<- ok log=WE
-> device set unit mmhg
<- ok unit=mmhg
-> device reboot
<- ok
```

`unit` is the panel's: it is a preference the panel persists in `/cfg/settings.ini` and shows on its
glass, so a build without a panel neither reports it in `device get` nor accepts `device set unit`.
`log` is not a preference — every build owns a logger — so both builds report it and accept `device
set log`. The machine-alone build reads
`ok proto=1 fw=... chip=... heap=... log=IWE caps=0`.

The machine's tuning numbers are not on this wire at all. The pump's flow, the chamber volume, the
floor, the duty cap, the deadband and the valve's direction describe the rig the firmware was built
for, not a preference a device holds, so they are build symbols in
`libraries/Gardener/src/Machine.h` — `PUMP_FLOW_LPM`, `CHAMBER_VOLUME_L`, `CONTROL_FLOOR_KPA`,
`PUMP_DUTY_MAX_PERCENT`, `CONTROL_DEADBAND_KPA` and `VALVE_NORMALLY_OPEN` — and a build for another
pump, chamber or valve overrides them (`scripts/arduino.py --define NAME=VALUE`, or `-D` on any
`arduino-cli` line). Nothing on a running device changes one. `floor` is still readable where it is
needed: `vacuum get` reports it as `floor=`, and it is the `max=` of a target's `out_of_range`
reply. `caps`' bit `4` is how a host learns the fitted valve fails safe.

`device set unit` survives a reset: the value is written to `/cfg/settings.ini` as the `unit` key of
the `[device]` section and read back at boot. A filesystem that will not take the write still
answers with the value in force for this session, and the boot log reports the failed mount. The machine and this wire stay in
kPa either way — a display unit is a preference about the glass, not a second way to state a
pressure. `log` is not stored at all: it is the set of levels written
for the running device, `IWE` at boot, and `device set log -` silences all three from the next
record until a link names levels again or the device resets. Narrowing the set keeps the levels it
names: `device set log E` writes errors and drops the rest.

Battery state belongs to the device, not the vacuum system: `batt_v` and `batt_pct` are device
fields. They are answered by the panel's gauge, so a build without a panel reports neither — it has
no sense pin and no battery bit in `caps`. A panel build whose pack is not fitted reads `-` for
both.

`panel` is gone as a domain, and so is `display-size`: whether a panel is fitted is the `caps`
panel bit, one record served by both links. The wire still does not draw on the panel.

## 6. `vacuum` — the machine that holds pressure

`vacuum get` reports every field, in this order:

| Field | Values | Notes |
| --- | --- | --- |
| `pressure_kpa` | kPa, 2 dp | filtered vacuum depth below ambient: 0 at rest or while no reading can be trusted, rising as the chamber is pumped down |
| `target` | kPa below ambient, 1 dp | the current target; `0.0` is the ambient target, which holds the chamber released |
| `floor` | kPa, 1 dp | deepest reachable target, `CONTROL_FLOOR_KPA` below the reference |
| `duty` | %, 1 dp | pump duty |
| `state` | `IDLE` / `PULL` / `HOLD` / `VENT` / `FAULT` | |
| `sensor` | `1` / `0` | `1` when the sensor is producing a plausible reading |
| `fault` | `none` / a code | `sensor`, `vent_timeout`, `no_progress` or `overshoot` while one is latched |
| `hold` | s | the hold granted to the run, `0` when nothing is running |
| `max_run` | s | the longest hold a start may ask for, `CONTROL_MAX_RUN_MS` |
| `up` | ms | uptime |

| Verb | Reply | Notes |
| --- | --- | --- |
| `get` | every field | the vacuum readout; takes no argument |
| `target <kPa\|->` | `ok target=<kPa>` | `vacuum target 40` pulls to 40 kPa below ambient. `0` is the ambient target: it releases the chamber and stops, and `-` is a synonym for it. A target deeper than the reading keeps pulling; a shallower one while `PULL` or `HOLD` stops the pump, opens the valve and runs `VENT` until it is reached. Deeper than `floor` is `err out_of_range field=target … max=<floor>`, and a negative or unparsed value is `err out_of_range field=target … min=0.0` |
| `start [<seconds>]` | `ok state=PULL` / `err not_ready <reason>` / `err out_of_range field=hold …` | closes the valve, then runs the stored target for the optional hold, in whole seconds; without one the run gets `max_run`. Valid at any target, `0` included. A start while running is a no-op. Reasons: `no_reading`, `latch` |
| `stop` | `ok state=VENT` | pump off, then vent to ambient. The target is kept |
| `reset` | `ok state=<state>` | clears the latched fault, then vents |

Every verb here is available to any link: there is no controlling state to be in. A stop and a
target from another link are both taken, and the last one to reach the control task wins.

```
-> vacuum get
<- ok pressure_kpa=40.07 target=40.0 floor=60.0 duty=72.0 state=HOLD sensor=1 fault=none hold=0 max_run=900 up=45000
-> vacuum target 40
<- ok target=40.0
-> vacuum start 300
<- ok state=PULL
-> vacuum target 0
<- ok target=0.0
-> vacuum stop
<- ok state=VENT
```

`target` is the only actuator verb. There is no `duty` write: a second way to drive the pump
would mean a second control path to reason about. Characterizing the pump therefore needs a
temporary build. That trade was made deliberately, so nobody should go looking for a raw duty
command that is not there.

Vent floor, margin and timeout, the pump ramp and the loop's shape constants are in `Machine.h`,
and so are the rig's own numbers — the pump's flow, the chamber volume, the floor, the duty cap, the
deadband and the valve's direction. The battery thresholds are in the panel's
`hal/impl/AdcBattery.cpp`. The PID gains are derived from `PUMP_FLOW_LPM` and `CHAMBER_VOLUME_L`,
the loop clamps to `PUMP_DUTY_MAX_PERCENT`, and `CONTROL_DEADBAND_KPA` and `CONTROL_FLOOR_KPA` are
build symbols besides. None of these are commands, so the command surface stays this table. The
deadband matters to a controller: it is what decides that the target has been reached, and it is
not on the wire. `floor` is the other one worth knowing: it is the deepest target accepted, it is
reported in `vacuum get`, and it is the `max=` of the target's `out_of_range` reply.
`CONTROL_MAX_RUN_MS` is the deadline every run has, published as `max_run`.

## 7. `buzzer` — the piezo

The piezo is the panel's part, so this domain is served only by a build with a panel. A build
without one answers `err unknown_domain buzzer`, the same as any token it does not serve.

`buzzer get` reports `tone` then `playing`:

| Field | Values | Notes |
| --- | --- | --- |
| `tone` | `<hz>,<ms>` or `-` | the current frequency and remaining ms, or `-` when silent |
| `playing` | `on` / `off` | a tone or sequence is still running; a rest still counts |

| Verb | Reply | Notes |
| --- | --- | --- |
| `get` | `tone`, `playing` | takes no argument |
| `tone <hz> <ms>` | `ok tone=<hz>,<ms>` | `ms=0` runs until `buzzer stop` |
| `play <sequence>` | `ok playing=on` | note/duration pairs separated by spaces; the sequence is the rest of the line |
| `stop` | `ok playing=off` | silence and stop |

```
-> buzzer get
<- ok tone=- playing=off
-> buzzer tone 2000 500
<- ok tone=2000,500
-> buzzer get
<- ok tone=2000,412 playing=on
-> buzzer play a4:1000 c6:500
<- ok playing=on
-> buzzer get
<- ok tone=440,610 playing=on
-> buzzer stop
<- ok playing=off
```

`play` takes note names with an optional `#` or `b`, then one octave digit (`a4`, `c#5`, `bb3`);
`r` is a rest. The notation is `note:duration` pairs separated by spaces, and nothing else — a
comma, a missing colon, or a space between a note and its duration is refused. The sequence is
the rest of the line, so spaces survive without quoting. Durations are milliseconds and must be
positive. The frequency uses equal temperament with A4 = 440 Hz; a note outside the buzzer's
100–10000 Hz range is a bad argument, not a clamp. The sequence is not read back.

A tone or sequence continues until it is stopped or its own time runs out; the controller times a
long alert by choosing `ms`, and `ms=0` is the explicit way to say "until I stop it".

## 8. `fs` — the device's files

The device's LittleFS tree: `/system`, `/modules` and `/routines` for the files the interpreter
compiles, and `/cfg` for the documents: `/cfg/settings.ini`, `/cfg/jingles.ini` and one
`/cfg/routines/<name>.ini` per routine. `fs` is a bench interface for changing what is on the device
without reflashing its data image. Writing under `/system`, `/modules`, `/routines` or `/cfg` does
not reload anything; the change takes effect at the next boot.

The files are the panel's — it is the panel that mounts the partition and runs the routines — so
this domain is served only by a build with a panel. A build without one answers `err unknown_domain
fs` and mounts no filesystem at all.

| Verb | Reply | Notes |
| --- | --- | --- |
| `list <path>` | `ok`, then entries, then a blank line | one `file <name>` or `dir <name>` per entry |
| `read <path>` | `ok bytes=N`, then exactly N raw bytes | a zero-byte file sends only the header |
| `write <path> <bytes>` | `ok` | the header is followed by exactly `<bytes>` raw bytes |
| `mkdir <path>` | `ok` | creates one directory; the parent must exist |
| `delete <path>` | `ok` | removes a file, or a directory and everything under it |

All arguments are positional and unquoted. A path is one token and contains no spaces.

```
-> fs list /cfg/routines
<- ok
<- file interval.ini
<- file ramped_interval.ini
<- file ramped_static.ini
<- (blank line)
-> fs read /cfg/routines/interval.ini
<- ok bytes=12
<- [12 raw bytes]
-> fs write /cfg/routines/new.ini 5
<- ok
-> fs mkdir /assets/icons
<- ok
-> fs delete /assets/icons
<- ok
```

`fs list` streams its rows as it traverses: there is no row count and no `end` line. The blank line
is the terminator, so an empty directory is `ok` then a blank line. Each row is `type name`; `type`
is `file` or `dir`, and `name` is one token that can be pasted into `fs read` or `fs delete`. Rows
come in the filesystem's order and are not sorted.

`fs read` is the one reply with a body count: the host reads exactly `bytes=N` bytes, so raw file
content is never scanned for a terminator. `fs write` is length-delimited in the same way: the
header's `<bytes>` is authoritative, the payload may contain any byte including `LF` and NUL, and
the device never searches it for an end. One optional `LF` or `CRLF` after the payload is consumed.
A write that declares more than 1 MiB is
`err out_of_range field=bytes value=… max=1048576`.

A write header that is syntactically valid but cannot be honoured still consumes the declared
payload before replying, so the next command starts on a boundary. A header refused on syntax — a
malformed `<bytes>`, an oversized count, or a malformed path — replies immediately and expects no
payload. A payload that stops arriving for 2 s is abandoned, the partial file
is removed, and the reply is `err bad_argument`.

Paths are absolute, `/`-separated and case-sensitive. `.`, `..`, empty segments and trailing
slashes are refused. `/` is valid only for `fs list`. `/cfg/settings.ini` is reserved: `fs write`
and `fs delete` refuse it with `err unsupported path=/cfg/settings.ini reason=reserved`; `fs read`
and `fs list` may see it. A malformed path is `err bad_argument <token>`; a path that parses but is
absent is `err not_found <path>`.

The transfer blocks the control loop until it completes. Routines and assets are small and moving
them is a bench operation; do not write a large file while a run is active.

## 9. `routine` — the routines

The routine surface is the panel's, so this domain is served only by a build with a panel, the same
as `fs` and `buzzer`. The contract that says what a routine, a state and a command are, and what a
launch means, is [routine links](./routine-links.md); this section is the wire.

| Verb | Reply | Notes |
| --- | --- | --- |
| `list` | `ok`, then rows, then a blank line | one `key=<key> kind=<kind> parent=<parent>` per routine and alias, in registration order, each alias after its routine; a root's `parent` is `-` |
| `states <key>` | `ok states=<s1>,<s2>,…` | the routine's declared states, in order; `-` when it declares none |
| `commands <key>` | `ok commands=<g1>,<g2>,…` | the routine's declared commands — whole gesture strings — in order; `-` when it declares none |
| `settings <key>` | `ok`, then rows, then a blank line | one `key=<key> kind=<kind> default=<default>` per setting, in order |
| `get` | `ok key=<key> state=<state>` | the routine on top and its state; `key=- state=-` when no routine is showing |
| `launch <key> [<setting>=<value> …]` | `ok key=<key>` | open a routine from its stored set, or with the named values in their place |
| `command <gesture>` | `ok gesture=<gesture>` | one whole gesture string into the queue the panel feeds |

`states` and `commands` answer a comma-separated list in one value; a routine that declares none
answers `-`, not an error. `settings` rows are `key=<key> kind=<kind> default=<default>`: `kind` is
`bool`, `choice`, `real`, `int`, `pressure` or `duration`, and `default` is the setting's default in
the wire form — a bool is `on` or `off`, a pressure is kPa, a duration is whole seconds, and a
nullable setting that holds nothing is `-`. What each kind accepts is the routine's declaration,
the kinds in [bindings.md](./ui/bindings.md). An unknown key is `err not_found <key>`.

`launch` vents the machine before it opens the routine, and a launch from named settings is for
this visit only: values are held to their kind and the set repaired by the routine's rule exactly
as a stored set is, and nothing is written to the routine's document. An unknown key is
`err not_found <key>`; a named setting the routine does not declare, or a value its kind refuses,
is `err bad_argument <key>`; a stack that cannot take the push is `err not_ready full`.

`command` takes one whole gesture string from [gestures.md](./gestures.md), which enters the same
queue the panel feeds — reserved gestures first, and a gesture the scene showing does not answer is
dropped. `ok` is acceptance into the queue, not an act by the scene. A token that is not a gesture
is `err bad_argument <token>`.

    -> routine list
    <- ok
    <- key=ramped_static kind=root parent=-
    <- key=ramped_interval kind=root parent=-
    <- key=interval kind=root parent=-
    <- (blank line)
    -> routine states ramped_static
    <- ok states=ready,approach,run,paused
    -> routine commands ramped_static
    <- ok commands=up:double,ok:single,cancel:single,cancel:double
    -> routine settings ramped_interval
    <- ok
    <- key=set_count kind=int default=4
    <- key=initial_pressure kind=pressure default=5.0
    <- key=final_pressure kind=pressure default=20.0
    <- key=step_pressure kind=pressure default=-
    <- key=rest_pressure kind=pressure default=-
    <- key=work_duration kind=duration default=300
    <- key=rest_duration kind=duration default=45
    <- key=resume_behavior kind=choice default=immediately
    <- (blank line)
    -> routine get
    <- ok key=- state=-
    -> routine launch ramped_static target_pressure=55.0 total_duration=120
    <- ok key=ramped_static
    -> routine get
    <- ok key=ramped_static state=ready
    -> routine command ok:single
    <- ok gesture=ok:single

## 10. Errors

Every `err` is one of these codes, so a controller can switch on the code instead of parsing
prose. The detail is a bare token when the code names one thing, and items when it names several:

| Code | Detail | Meaning |
| --- | --- | --- |
| `unknown_domain` | bare token | the first token is not a domain: `err unknown_domain set` |
| `unknown_verb` | bare token | the domain does not accept that verb: `err unknown_verb frobnicate` |
| `bad_argument` | bare token, or nothing | an argument is missing, does not parse or is left over; or the header was too long. The token is the first one at fault |
| `out_of_range` | `field=… value=… [min=…] [max=…]` | the value parses but is not acceptable: `err out_of_range field=target value=101 max=60.0` |
| `not_ready` | bare reason | valid, and cannot be honoured now: `no_reading`, `latch`; for `fs`: `is_dir`, `not_dir`, `exists`, `no_parent`, `io`; for `routine`: `full` |
| `not_found` | bare path, or a routine key | an absent path (`fs`), or a key nothing is registered under (`routine`) |
| `unsupported` | `path=… reason=…` | the protocol defines it and this build cannot do it (`fs` only) |

```
-> vacuum start
<- ok state=PULL
-> vacuum target 101
<- err out_of_range field=target value=101 max=60.0
-> vacuum start 99999
<- err out_of_range field=hold value=99999 max=900
-> vacuum frobnicate
<- err unknown_verb frobnicate
-> frobnicate get
<- err unknown_domain frobnicate
-> device get frobnicate
<- err bad_argument frobnicate
```

A command with no verb at all is `err bad_argument` with nothing to name.

Numbers are read with the Arduino `String` parsers, which take the leading number and ignore what
follows: `12abc` is 12, and a token with no digits at all is 0. The range check then decides, so a
malformed number arrives as `out_of_range` — or, for `fs write`, as that many declared bytes —
rather than as `bad_argument`. A missing or leftover argument is still `bad_argument`.

## 11. State model

The machine's states, the transition each command causes and the protections behind them are the
firmware's contract and live in [vacuum control](./vacuum-control.md). `vacuum start`, `vacuum stop`
and `vacuum get` report the state; the transitions themselves are that document's.

## 12. Rates

Nothing is pushed: a controller that wants a readout polls `vacuum get`. Each poll is a ~90 byte
reply, so polling at 4–10 Hz is a trivial load at 115200 baud and gives a smoother readout than
the device could produce on its own. Each reply describes the machine as of the last control tick
(50 ms), so polling faster than that repeats itself.

Replies are matched by counting. A controller that pipelines several commands in one write should
read one reply per command before concluding anything about state: `vacuum start` followed
immediately by `vacuum get` in the same batch may describe the machine as it was before the
`vacuum start` was handled. Sending `vacuum get` as its own command avoids the question entirely.

Each loop iteration drains everything the host has sent and services **every complete header** it
received, in arrival order, with nothing dropped and nothing reordered — so a batch written in one
go is handled in one pass. Replies are still exactly one per command, so a host that counts them
reads one per line.

## 13. Stability

This is the first revision of the domain-first wire. The number is `1` and stays there until the
protocol settles; revisions are not numbered yet, and a controller should be prepared to change
when the firmware does. There is no migration table because there is nothing stable to migrate
from.
