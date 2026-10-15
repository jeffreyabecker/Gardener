# Eggplan Vacuum serial protocol

Version **3**. Pressures are **kPa** everywhere.

The device is a serial peripheral in the strict sense: it never initiates, keeps no session,
and every state it has is either a command it was given or something it had to do for
safety. All control comes from the controller at the other end of the line.

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
The FQBN is pinned in `firmware/eggplan_vacuum/sketch.yaml`, `scripts/arduino.ps1` and
`.vscode/arduino.json` for exactly that reason.

## 2. Connecting

1. Open the port. The device sends nothing until it is asked: there is no greeting and no
   handshake to wait for.
2. Send `info` and check `proto=3`.
3. Subscribe to nothing: `status` is polled (§9).

**Disconnecting stops the run and vents the chamber** (§8), because a controller that goes
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
| `remain` | s | hold time remaining, `-` when no timer |
| `total` | s | configured timer, `-` when none |
| `batt` | % | `-` when no battery is fitted |
| `sensor` | — | `1` when the MPRLS is answering |
| `fault` | — | `none`, `max_run`, `sensor` |
| `up` | ms | uptime |

```text
ok kpa=41.23 vac=60.10 pct=59.3 target=40.0 floor=41.3 duty=72.0 state=HOLD remain=572 total=600 batt=78 sensor=1 fault=none up=123456
```

## 6. Info payload

`proto=3 fw=0.2.0 chip=ESP32-C3 heap=298412 batt_v=3.94 batt_pct=78 log=on`

| Key | Notes |
| --- | --- |
| `proto` | wire version; **3** is this document |
| `fw` | firmware version |
| `chip` | chip model |
| `heap` | free heap in bytes |
| `batt_v` | pack volts, 2 dp, `-` when no pack is fitted |
| `batt_pct` | pack percent, `-` when none |
| `log` | `on`/`off`, whether diagnostics are being written |

## 7. Commands

| Command | Reply | Notes |
| --- | --- | --- |
| `help` | `ok help=info\|status\|help\|start\|stop\|reset\|reboot\|ambient\|set` | the one list-valued payload |
| `info` | `ok proto=3 …` | device identity |
| `status` | `ok <status payload>` | one shot; poll it for a readout |
| `start` | `ok started` / `err not_ready` | closes the valve, then pulls to the target |
| `stop` | `ok stopped` | pump off, then vent |
| `set target <kPa\|off>` | `ok target=40.0` / `ok target=off` | the only actuator command |
| `set timer <seconds\|off>` | `ok timer=600` / `ok timer=off` | `0` and `off` are the same thing |
| `set ambient <kPa>` | `ok ambient=101.33` | rejected when outside what the part can measure |
| `set log <on\|off>` | `ok log=on` / `ok log=off` | diagnostics on this stream |
| `ambient` | `ok ambient=101.33` | capture the current reading as the reference |
| `reset` | `ok reset` | clears the latched fault, then vents |
| `reboot` | `ok rebooting` | boots safe: pump off, valve open, nothing restored |

```text
-> set target 40
<- ok target=40.0
-> set timer 600
<- ok timer=600
-> start
<- ok started
-> status
<- ok kpa=40.21 vac=61.07 pct=60.3 target=40.0 floor=41.3 duty=72.0 state=HOLD remain=592 total=600 batt=78 sensor=1 fault=none up=45000
-> stop
<- ok stopped
```

**`set target` is the only actuator command.** There is no `set duty`: a second way to drive
the pump would mean a second control path to reason about. Characterizing the pump therefore
needs a temporary build. That trade was made deliberately, so nobody should go looking for a
raw duty command that is not there.

Vent margin and timeout, the PID gains, the duty cap, the slew limit, the maximum run time
and the battery thresholds are all in `core/Config.h`. They are not commands, so the command
surface stays this table.

### Error codes

Every `err` is one of these five, so a controller can switch on the code instead of parsing
prose:

| Code | Meaning |
| --- | --- |
| `unknown_command` | not a command, or an unknown `set` field |
| `bad_argument` | the value is missing, does not parse, or the line was too long |
| `out_of_range` | the value parses but is not acceptable |
| `not_ready` | valid request that cannot be honoured now: `start` with no target, no reading, or a latched fault |
| `unsupported` | the feature does not exist in this firmware: `set duty`, `set period`, `telemetry` |

## 8. State model

The controller owns the target, the timer and when to start and stop. The device owns what
happens when nobody is watching.

```
        start
IDLE ──────────> PULL ──(target reached)──> HOLD ──(timer 0)──┐
  ^               │        countdown starts                    │
  │               │  stop / link loss / fault                  │
  │               v                    v                       v
  └──(vent complete)──────────────── VENT <─────────────────────┘
```

| Event | From | To |
| --- | --- | --- |
| `start` | `IDLE` | closes the valve, then `PULL`; `err not_ready` while venting or faulted |
| `stop` | `PULL`, `HOLD`, `VENT` | opens the valve, stops the pump, `VENT` then `IDLE` |
| `set target <kPa\|off>` | anything but `FAULT` | the target changes; `off` while pulling or holding releases the chamber |
| `set timer <seconds\|off>` | any | used when the countdown starts |
| target reached | `PULL` | `HOLD`, and the countdown starts |
| timer expires | `HOLD` | `VENT` then `IDLE` |
| link loss, sensor loss, pump latch | any | `VENT`, then `IDLE` or `FAULT` |
| `reset` | `FAULT` | `VENT` then `IDLE` |
| `reboot` | any | safe boot, i.e. `IDLE` with the valve open |

What a controller has to know:

* `remain` is **hold time remaining** and starts when the target is first reached, so it sits
  at `total` for the whole pull.
* **Faults latch.** Only `reset` clears one; the device never retries on its own.
* Venting finishes when the pressure is within 2 kPa of ambient, or after 30 s, whichever
  comes first. A vent timeout is logged, not an error, and the state still returns to `IDLE`.
* **The vent valve is normally open**: de-energised is open, so running a vacuum means
  energising the coil to close it. A crash, a brownout or a cut wire therefore vents the
  chamber whether or not the firmware noticed.
* A normally open solenoid draws current for the whole hold, which is why a long hold is the
  dominant battery cost rather than the pump.
* The display is a readout with no controls, so anything that can only be *seen* — a fault, a
  vent timeout, a low battery — is waiting for a controller to notice it.

## 9. Rates

Nothing is pushed: a controller that wants a readout polls `status`. Each poll is a ~140 byte
reply, so polling at 4–10 Hz is a trivial load at 115200 baud and gives a smoother readout
than the device could produce on its own. Each reply describes the machine as of the last
control tick (50 ms), so polling faster than that repeats itself.

Because replies are matched by counting, a controller that pipelines several commands in one
write should read one reply per command before concluding anything about state: `start`
followed immediately by `status` in the same batch may describe the machine as it was before
the `start` was handled. Sending `status` as its own command avoids the question entirely.
