# Eggplan Vacuum BLE protocol

Version **2** (`proto=2` in the `info` payload). Pressures are **kPa** everywhere.

The device is a BLE **peripheral** in the strict sense: it never initiates, keeps no
session, and every state it has is either a command it was given or something it had to
do for safety. All control comes from the central.

## 1. GATT layout

| Role | UUID | Properties | Purpose |
| --- | --- | --- | --- |
| Service | `8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f00` | — | — |
| Command (`cmd`) | `8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f01` | write, write without response | newline terminated command lines |
| Reply (`rsp`) | `8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f02` | read, notify | exactly one reply per command |
| State (`state`) | `8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f03` | read, notify | periodic telemetry |
| Info (`info`) | `8ea7a000-9a3b-4a5e-8c1d-4b1c9d0e6f04` | read | device identity |

The device advertises as `EggplanVac<ID>`, where `<ID>` is derived from the eFuse MAC,
with the service UUID in both the advertisement and the scan response.

### Connecting

1. Connect. There is no handshake and no session: the device sends nothing until it is
   asked, so there is no "hello" to wait for.
2. Subscribe to `rsp`, and to `state` if you want telemetry.
3. Read `info` — or send `info` — and check `proto=2`.

**Subscribing to `state` is what enables telemetry.** There is no `telemetry on`
command, so the subscription can never disagree with what the device is doing; only the
period is configurable (`set period`). A read of `state` returns the current status
payload, for a client that would rather poll than subscribe.

**Disconnecting stops the run and vents the chamber** (§6). So does silence from a
connected client, once `LINK_TIMEOUT_MS` is non-zero — it defaults to `0`, meaning off,
because a long run with a connected but quiet client is legitimate.

## 2. Grammar

```
command  = verb [ SP argument ] LF
reply    = "ok" SP payload LF | "err" SP code [ SP detail ] LF
payload  = item *( SP item )
item     = key [ "=" value ]
key      = ALPHA *( ALPHA / DIGIT / "_" )
value    = 1*( ALPHA / DIGIT / "_" / "-" / "." / ":" / "%" / "|" )
```

* An item with no value is a flag: `ok started`, `ok stopped`, `err not_ready`.
* Values never contain a space and are never empty, so nothing needs quoting or
  escaping. `-` means "not applicable".
* A newline terminates a command; `\r` is ignored, one write may carry several
  commands, and the device buffers up to 512 bytes of a line in progress.
* Commands are handled in arrival order on the main loop and produce **exactly one**
  reply each, so a client matches replies to commands by counting. Ordering is not
  otherwise guaranteed, and request ids are the documented fallback if pipelining ever
  becomes a problem.
* Unknown reply keys must be ignored, which is what lets fields be added later.
* The one list-valued payload is `help`, where `|` separates items.

## 3. Framing

A reply or notification that fits in one ATT notification (`MTU - 3` bytes) is sent
as-is, which is the normal case. Anything longer is split:

* chunk: `0x01`, sequence byte (starting at 0), then up to `MTU - 5` payload bytes
* end: `0x02`, last sequence byte

Concatenate the chunk payloads in sequence order and emit the line when the `0x02`
marker arrives — do not count bytes. A status line is around 120 bytes, so chunking
only happens for a client that leaves the MTU at 23.

## 4. Status payload

`status` replies and `state` telemetry carry the same payload, with a stable key order,
so a client writes one parser and uses it for both.

| Key | Unit | Notes |
| --- | --- | --- |
| `kpa` | kPa | filtered absolute pressure, 2 dp |
| `vac` | kPa | `ambient - kpa`, clamped at 0 |
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

### `info` payload

`proto=2 fw=… chip=… heap=… tach=0|1 batt_v=… batt_pct=…`

| Key | Notes |
| --- | --- |
| `proto` | wire version; **2** is this document |
| `fw` | firmware version |
| `chip` | chip model |
| `heap` | free heap in bytes |
| `tach` | `1` when the pump tach input is enabled |
| `batt_v` | pack volts, 2 dp, `-` when no pack is fitted |
| `batt_pct` | pack percent, `-` when none |

## 5. Commands

| Command | Reply | Notes |
| --- | --- | --- |
| `help` | `ok help=info\|status\|help\|start\|stop\|reset\|reboot\|ambient\|set` | the one list-valued payload |
| `info` | `ok proto=2 …` | same payload as reading the `info` characteristic |
| `status` | `ok <status payload>` | one shot; use `state` to stream |
| `start` | `ok started` / `err not_ready` | closes the valve, then pulls to the target |
| `stop` | `ok stopped` | pump off, then vent |
| `set target <kPa\|off>` | `ok target=40.0` / `ok target=off` | the only actuator command |
| `set timer <seconds\|off>` | `ok timer=600` / `ok timer=off` | `0` and `off` are the same thing |
| `set ambient <kPa>` | `ok ambient=101.33` | rejected when outside what the part can measure |
| `set period <ms>` | `ok period=250` | telemetry period, minimum 50 ms |
| `ambient` | `ok ambient=101.33` | capture the current reading as the reference |
| `reset` | `ok reset` | clears the latch, then vents |
| `reboot` | `ok rebooting` | boots safe: pump off, valve open, nothing restored |

```text
-> set target 40
<- ok target=40.0
-> start
<- ok started
-> status
<- ok kpa=40.21 vac=61.07 pct=60.3 target=40.0 floor=41.3 duty=72.0 state=HOLD remain=592 total=600 batt=78 sensor=1 fault=none up=45000
-> set timer off
<- ok timer=off
```

**`set target` is the only actuator command.** There is no `set duty`: a second way to
drive the pump would mean a second control path to reason about. Characterizing the
pump therefore needs a temporary build. That trade was made deliberately — nobody
should go looking for a raw duty command that is not there.

### Error codes

Every `err` is one of these five, so a client can switch on the code instead of parsing
prose:

| Code | Meaning |
| --- | --- |
| `unknown_command` | not a command, or an unknown `set` field |
| `bad_argument` | the value is missing or does not parse |
| `out_of_range` | the value parses but is not acceptable |
| `not_ready` | valid request that cannot be honoured now: `start` with no target, no reading, or a latched fault |
| `unsupported` | the feature was removed in v2: `set duty`, `telemetry` |

## 6. State model

The central owns the target, the timer and when to start and stop. The device owns what
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

What a client has to know:

* `remain` is **hold time remaining** and starts when the target is first reached, so it
  sits at `total` for the whole pull.
* **Faults latch.** Only `reset` clears one; the device never retries on its own.
* Venting finishes when the pressure is within 2 kPa of ambient, or after 30 s,
  whichever comes first. A vent timeout is logged, not an error, and the state still
  returns to `IDLE`.
* **The vent valve is normally open**: de-energised is open, so running a vacuum means
  energising the coil to close it. A crash, a brownout or a cut wire therefore vents the
  chamber whether or not the firmware noticed.
* A normally open solenoid draws current for the whole hold, which is why a long hold is
  the dominant battery cost rather than the pump.
* The display is a readout with no controls, so anything that can only be *seen* — a
  fault, a vent timeout, a low battery — is waiting for a client to notice it.

## 7. Migration from v1

| v1 | v2 |
| --- | --- |
| `prog` characteristic (`…6f03`) for program upload | **gone**; `…6f03` is `state` telemetry |
| telemetry notified on `rsp` with a `tel ` prefix | telemetry on `state`, no prefix |
| `telemetry on\|off\|ms` | subscribe to `state`; `set period <ms>` |
| multi-line replies terminated by a bare `eof` | one reply line per command, full stop |
| `ping`, `pump`, `manual`, `target`, `hold`, `eval`, `run`, `list`, `cat`, `rm`, `log`, `buttons`, `autostart` | gone; `set target` replaces `target`, `hold` and `pump` |
| `state` values `IDLE`, `PUMP`, `HOLD`, `MAN`, `FAULT` | `IDLE`, `PULL`, `HOLD`, `VENT`, `FAULT` |
| `info` keys `runtime`, `progs`, `store_kb` | gone; `proto`, `batt_v`, `batt_pct` added |
| `err <prose>` | `err <code>` from the table in §5 |

Detect all of this with `proto=2` in `info`: a v1 client that sees `proto=2` should
refuse to continue rather than guess.

## 8. Serial console

The device logs to USB serial at 115200 baud, and nothing else. The command grammar is
not wired to serial: `hosts/` has one transport, the BLE one, so a debug console would
be a second host rather than a second code path.
