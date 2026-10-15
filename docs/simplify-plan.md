# Plan: remove programmability, simplify the instrument

Status: **ready to execute** · revision 4 · owner: unassigned · scope: `libraries/VacuumCore`, `docs/`, `tools/`

Revision 2 folded in Q1–Q9, revision 3 added the remote-only control model, and revision 4
closes Q10–Q17, so **there are no open questions left** (§11). Three of the last answers
changed the design rather than confirming it: the valve is **normally open** (Q10), which
makes the fail-safe physical and inverts the coil logic; **raw duty is gone** (Q15), which
removes the `MANUAL` state; and **nothing is persisted** (Q14), which removes
`hal/Settings` from the plan entirely.

Annotation guide: decisions taken here are marked `DECISION:`. Everything decided is
tabulated in §10. Edit in place and the plan will be picked up from your edits; delete
this file when the work is done.

## 1. Goal

The instrument is a vacuum controller: pull to a target, hold it, tell the operator
what is happening. Programmability (uploading Berry programs, a filesystem, a runtime,
an upload protocol, a programs page) has grown to touch every layer, and it is not the
product. Take it out, get the protocol and the screen back down to something that fits
on a bench instrument, and revisit scripting later as an add-on rather than a
foundation.

**The device is a BLE peripheral with no local control surface** (revision 3). Every
decision comes from the central: it sets the target and the timer, starts and stops the
run, and clears faults. The firmware keeps only what must be fast or safe to keep.

| The device owns | Because |
| --- | --- |
| the pressure loop (PID to a target) | a BLE round trip is far too slow for a control loop |
| the duty cap, the slew limit and the max run time | they protect the hardware even when the central is wrong or gone |
| the vent valve and its timing | it is the release path for a stop, a fault and a link loss |
| the run timer and its expiry | it must still expire when the central has disappeared |
| the battery gauge and the display | local readouts, not decisions |

| The central owns | Because |
| --- | --- |
| the target, the timer, and when to start or stop | recipe and schedule live where the operator is |
| fault acknowledgement (`reset`) | the device latches and waits rather than deciding to retry |
| logging, recipes, notifications, history | none of it belongs on a bench instrument |

Two consequences worth stating plainly: on boot the device is **safe** (pump off, valve
de-energised and therefore open, `IDLE`, nothing restored), and when the central goes away
the device **stops and vents** rather than holding a vacuum nobody is watching. Because
the valve is normally open (Q10) that second one is not a promise the firmware has to
keep: it is what the hardware does when the firmware stops.

In the same pass:

* a **clearly documented BLE protocol**: small enough to read in one sitting, with a
  formal grammar and one status payload shape
* a **simplified display**: battery, current kPa, target kPa, countdown timer — and the
  only local feedback the operator has
* a **vent valve**, so a stopped, completed, faulted or disconnected run releases the
  chamber instead of bleeding away through a leak
* **no buttons and no buzzer**: there is no local control surface at all (revision 3), so
  nothing reads the buttons (Q3), the piezo is never added (Q9), and the protocol is the
  only control surface

### Non-goals

* No change to the pump, valve-less control law or the pressure path beyond what the
  run timer needs.
* No new board, no change to the I2C bus or the pressure sensor.
* Scripting is being deferred, not abandoned: §2.3 records where the code lives so
  re-adding it is a revert rather than a rewrite.

## 2. What programmability costs today

### 2.1 To delete

| Item | Why it goes |
| --- | --- |
| `hosts/script/` (`ScriptRuntime`, `ScriptApi`, `NullRuntime`) | the host exists only to run uploaded programs |
| `services/ProgramStore.*`, `services/ProgramUploader.*` | filesystem storage and the B/E/D/X frame grammar |
| `hal/Storage.*` | LittleFS wrapper; without programs nothing needs a filesystem |
| `hosts/ble`: the `prog` characteristic | the only reason the transport had a second write path |
| `ui/pages/ProgramsPage.cpp` | no program list to show |
| `programs/*.be` | sample programs |
| `core/Snapshot.h`: `ProgramInfo`, `ProgramChecksum`, `ScriptOverlay`, and the `scriptRunning`, `program`, `runtime`, `programs`, `programCount`, `storeUsedKb`, `storeTotalKb`, `autostart` fields | the machine view shrinks to what the instrument actually has |
| `core/Config.h`: `EGGPLAN_WITH_BERRY` | there is only one runtime: none |
| `hosts/commands`: `run`, `stop`→keep (now means the run), `eval`, `list`, `cat`, `rm`, `autostart` | command surface shrinks by half |
| `ui/Renderer`: the overlay path, `UiController`: `RunSelected` intent and cursor state | the overlay only existed for scripts; with one page there is no cursor |
| `tools/vacuum_cli.py`: `upload`, `list`, `cat`, `rm`, `run` | nothing to transfer |
| `hal/Buttons.*` | no local control surface (revision 3), so the driver goes; Q16 keeps the pins for a future override |
| `ui/UiController.*`, and `UiSettings` (page step, toast) in the snapshot | with no buttons there are no events, so no intents, no cursor and no page navigation |
| `ui/pages/{Graph,Info,Programs}Page.cpp` | one page (§6) |
| `core/Log`: the ring buffer, `logTail()` and the `log` command | nothing to debug remotely now (Q8); the sink stays |
| `hal/Settings` (never written) | boot safe, remember nothing (Q14): a device whose intent lives in the central has nothing of its own to remember |
| README's Scripting section, `docs/ble-protocol.md`'s transfer section | replaced by a roadmap note |

`scripts/check-layers.ps1` also loses `script` from the allowed `hosts/` order, so the
guardrail keeps describing the design rather than the plan.

### 2.2 What stays

Everything that makes it an instrument: MPRLS sensing with filtering, the PWM pump
driver with ramp/duty/run-time protection, the PID hold, the SH1106 display, the BLE
command/reply/telemetry transport, the layered layout with its enforced dependency rule,
and `core/ReplyQueue` (the transport still needs to hand command lines to the main
loop). New rather than kept: a vent valve output and a battery gauge.

### 2.3 Where the removed code lives

It is all in git at commit `f35a3bb` (`hosts/script`, `services/ProgramStore`,
`services/ProgramUploader`, `hal/Storage`, the `prog` characteristic and the page).
The plan is that revisiting scripting is a selective revert plus a re-add of
`hosts/script/`: the layering, the command grammar (`set …` is extensible) and the
snapshot are all still there to hang it from.

## 3. Shape after removal

```
core/      Config, Snapshot, Math, Pressure, Format, Log (sink only), Pid, ReplyQueue
hal/       Pins, Device, Battery, Pump, VentValve, Tachometer, PressureSensor, Display
services/  VacuumControl (target + timer + vent + safe stop), SnapshotBuilder, SnapshotText
hosts/     commands/CommandRouter, ble/BleService
ui/        Renderer, pages/StatusPage.cpp
app/       App
```

Two additions rather than removals (`hal/Battery`, `hal/VentValve`) and one collapse:
`ui/` becomes a renderer, because with no local control surface there is no interaction
state left to own. `hal/Settings` was planned to take over the filesystem's one remaining
job, remembering a few settings across a reboot, but Q14 has the device boot safe and
remember nothing, so it is not written at all.

## 4. New hardware

### 4.1 Pin map

| Signal | GPIO | Notes |
| --- | --- | --- |
| Button UP / DOWN / OK / BACK | 0 / 1 / 3 / 10 | **reserved, not read** (Q3) |
| I2C SDA / SCL | 8 / 9 | unchanged, MPRLS + OLED |
| Pump PWM | 6 | unchanged, MOSFET gate |
| Pump tach | 7 | optional, still disabled by default |
| **Battery sense** | **4** | ADC1 channel 4, ÷2 divider for a 1S Li-ion cell |
| **Vent valve** | **5** | digital output, MOSFET gate; **energise to close** (Q10) |
| spare | 2 | strapping pin, leave alone |

`DECISION:` battery on GPIO4 and vent valve on GPIO5 (Q3, Q9). ESP32-C3 ADC1 is
GPIO0..4 and three of those are button pins — a fitted button pulls its pin to ground
when pressed, so the battery sense must not inherit a freed button pin even though the
buttons are unread. GPIO4 is clear of both the buttons and the strapping pin. GPIO5 is
free now that the buzzer is dropped, and it drives a valve as happily as a piezo.

`hal/Pins.h` changes are therefore: add `BATTERY_ADC` and `VENT_VALVE`, drop the buzzer
before it is ever added, and keep the button pins with a comment that nothing reads
them yet (Q16).

### 4.2 Battery

```
BAT+ ── 100k ──┬── GPIO4
               │
              100k ── GND           (÷2, 1S Li-ion, 3.2–4.2 V)
```

* `hal/Battery.h`: `Battery(const BatteryConfig&)` with `adcPin`, `dividerRatio`,
  `emptyVolts`, `fullVolts`, `samples`, then `void update(uint32_t nowMs)`,
  `float volts()`, `uint8_t percent()`, `bool low()`, `bool present()`.
* Reads with `analogReadMilliVolts()` (the calibrated path in the ESP32 core) and
  averages, because a single ADC sample on this part is noisy.
* Percent from a straight line between the configured empty and full volts:
  **1S Li-ion, 3.2 V empty and 4.2 V full** (Q6), so 3.2 V reads 0 % and 4.2 V reads
  100 %. The ESP32 ADC is not lab grade; expect ±3–5 % and a `BATTERY_CALIBRATION`
  factor for trimming. What a low battery should *do* beyond being visible is Q11.
* `present()` is false when the reading is below a plausible minimum, so a
  mains-powered build does not show a permanent 0 % warning.

### 4.3 Vent valve

**The valve is normally open (Q10).** De-energised means open, so the chamber vents, and
*running a vacuum means energising the coil to close the valve*. That makes the fail-safe
physical rather than logical: a crash, a watchdog reset, a brownout or a cut wire all
de-energise the coil, and the chamber vents whether or not the firmware noticed.

Wiring matches the pump: a logic level MOSFET with a flyback diode across the coil (or a
relay module if the coil is hungry).

* `hal/VentValve.h`: `VentValve(const VentValveConfig&)` with `pin` and `activeHigh`,
  then `void open()`, `void close()`, `bool isOpen()`, `void update(uint32_t nowMs)`.
  `close()` energises the coil and `open()` releases it, and the header says so out loud,
  because this is exactly the inversion that gets wired backwards.
* **Sequencing**: close the valve before the pump starts and open it after the pump stops,
  so the pump never pulls against an open port.
* **Hold current**: a solenoid draws continuously while energised, so a long hold is the
  dominant battery cost on a 1S cell — the battery plan has to budget for the valve and
  not just the pump (R7).

## 5. Run model

The central drives it; the device owns safety. A countdown timer and a vent valve both
need the controller to know about a *run*:

```
        start(target, timer)
IDLE ────────────────────────> PULL ──(target reached)──> HOLD ──(timer 0)──┐
  ^                              │                          │                │
  │                              │  stop / link loss / fault                 │
  │                              v                          v                v
  └────(vent complete)───────── VENT <───────────────────────────────────────┘
```

* `VacuumControl` gains: `startRun()`, `stopRun()`, `setTarget(kpa)`,
  `setDurationSeconds(uint32_t)`, `remainingSeconds()`, `RunState state()`,
  `bool consumeCompleted()`.
* **States**: `IDLE`, `PULL`, `HOLD`, `VENT`, `FAULT`. Revision 3 dropped `DONE` (after
  venting the device is `IDLE`, and "did it finish" is already observable as `remain`
  reaching 0 on the way through), and Q15 dropped `MANUAL`: with raw duty gone,
  `set target` is the only actuator surface.
* `DECISION:` the countdown starts when the pressure first reaches the target (Q1), so
  the number on the screen is "hold time remaining". `target off` means pull down and
  stay there with no timer.
* **Boot is safe**: pump off, valve de-energised and therefore open, `IDLE`, nothing
  restored. The central re-establishes whatever it wants (Q14).
* **The coil is energised through `PULL` and `HOLD`** and released in `IDLE` and `VENT`
  (Q10), so hold current is a run cost, not an idle one.
* **Vent closes** when `kpa >= ambient - VENT_MARGIN_KPA` or after `VENT_TIMEOUT_MS`,
  whichever comes first. A timeout is not an error on its own (a leaky rig takes longer),
  but it is logged and the state still returns to `IDLE`.
* `DECISION:` stop, abort, link loss (Q4) and fault all funnel through `VENT` rather than
  stopping in place, so there is exactly one "release the chamber" path to reason about.
  With a normally open valve (Q10) a fault is safe by construction: de-energising the
  coil opens the chamber whether the firmware decides to vent or simply stops running.
* **Faults latch** and only the central clears them. The device does not decide to retry.
* Existing protections remain, and act without consulting the central: duty cap, slew
  limit, max run time, sensor-loss latch.

### 5.1 Command to state map

The sentence a client needs is "what happens if I send this", so the protocol document
will carry this table:

| Command | From | To |
| --- | --- | --- |
| `start` | `IDLE` | closes the valve, then `PULL`; `err not_ready` when venting or faulted |
| `stop` | `PULL`, `HOLD`, `VENT` | opens the valve, stops the pump, `VENT` then `IDLE` |
| `set target <kPa\|off>` | anything but `FAULT` | unchanged; the loop pulls to the new target |
| `set timer <seconds\|off>` | any | unchanged; used by the countdown when it starts |
| target reached | `PULL` | `HOLD`, and the countdown starts |
| timer expires | `HOLD` | `VENT` then `IDLE` |
| link loss, sensor loss, pump latch | any | `VENT`, then `IDLE` or `FAULT` |
| `reset` | `FAULT` | `VENT` then `IDLE` |
| `reboot` | any | safe boot, i.e. `IDLE` with the valve open |

## 6. Simplified display

One page, and no interaction at all: nothing reads the buttons, so the display is purely
informational and every control is a BLE command. No cursor, no graphs, no info page —
diagnostics come over BLE.

```
  x=0                                                            x=127
  y=0    HOLD                                       ▐███ ▐  78%
  y=10                   41.3                    <- size 2, current kPa
  y=26                    kPa
  y=30   target 40.0 kPa                      duty 72%
  y=42                  09:32                    <- size 2, hold countdown
  y=58   [############-------------------]        <- duty bar
```

| Element | Source | Notes |
| --- | --- | --- |
| state token | `RunState` | `IDLE`, `PULL`, `HOLD`, `VENT`, `FAULT`; `FAULT` is inverted |
| battery | `hal/Battery` | 12x7 icon, fill by percent, `%` text; blinks below the low threshold |
| current kPa | `PressureSensor` | size 2, one decimal, `kPa` in size 1 beside it |
| target kPa | `VacuumControl` | `target 40.0 kPa`, or `target --` when unset |
| duty | `Pump` | `duty 72%` on the same row as the target |
| countdown | `VacuumControl::remainingSeconds()` | `MM:SS`; `--:--` when no timer is set; inverts under 60 s |

`Renderer` gains three helpers: `battery(int16_t x, int16_t y, uint8_t percent)`,
`textRight(const char* text, int16_t y, uint8_t size)` for right-aligned values, and
`banner(const char* text)` for the fault row. Layout constants live in the page.

### 6.1 No interaction

Q3: the buttons are unused at this time, so `hal/Buttons` is deleted and nothing reads
GPIO0/1/3/10. The consequences are worth naming, because they change how the instrument
behaves rather than just how it is built:

* the display is the only local feedback, so the state token and the fault banner carry
  more weight than they did in a multi page UI
* a fault, a vent timeout or a low battery can only be *seen*, never acknowledged
* everything an operator can do is a BLE command (§7), which makes the protocol the
  primary interface rather than a debugging aid

Re-adding buttons is a revert of that deletion plus a mapping, once Q16 has decided
whether the pins stay reserved.

## 7. BLE protocol v2

The full spec replaces `docs/ble-protocol.md`. Shape:

| Role | UUID | Properties | Purpose |
| --- | --- | --- | --- |
| Service | `…6f00` | — | unchanged |
| `cmd` | `…6f01` | write, write NR | newline terminated commands |
| `rsp` | `…6f02` | notify, read | one reply per command |
| `state` | `…6f03` | notify, read | periodic telemetry, no prefix needed |
| `info` | `…6f04` | read | device identity, same payload as `info` |

`DECISION:` telemetry moves to its own characteristic, which deletes the `tel ` prefix
and the "filter telemetry out of replies" step every client had to do. `…6f03` was the
program characteristic; `proto=2` in `info` is how a client detects the break.

### 7.1 The document will contain

1. **Roles and flow** — connect, read `info`, write `cmd`, subscribe to `rsp` and
   `state`. The device is a peripheral in the strict sense: it never initiates, keeps no
   session, and every state it has is either a command it was given or something it had
   to do for safety.
   * **Ordering**: commands are handled in arrival order on the main loop and produce
     exactly one reply each, so a client matches replies to commands by counting rather
     than by tagging requests. Request ids are the documented fallback if pipelining ever
     matters.
   * **Link loss**: on disconnect, and after `LINK_TIMEOUT_MS` of no writes while still
     connected, the device stops the run and vents (Q4). The timeout defaults to `0`,
     meaning off (Q17): a one hour run with a connected but quiet central is legitimate,
     and the disconnect path already covers a client that leaves.
   * **Faults latch** until the central sends `reset`. A failed pump or a lost sensor
     stays failed until somebody says otherwise.
   * **State transitions** are the table in §5.1, repeated in the document, because that
     is the part a client actually has to implement.
2. **Grammar**, in EBNF, small enough to be unambiguous:

   ```
   command  = verb [ SP argument ] LF
   reply    = "ok" SP payload LF | "err" SP code [ SP detail ] LF
   payload  = token *( SP token )
   token    = key "=" value
   value    = *( ALPHA / DIGIT / "_" / "-" / "." / ":" / "%" )   ; never SP, never empty
   ```

   The "no spaces in values, `-` means not applicable" rule is what removes quoting and
   escaping from both ends. It is also why the pump fault is a token (`fault=max_run`)
   and the human text stays in the log.
3. **One status payload** used by both `status` replies and `state` telemetry, so a
   client writes one parser:

   | key | unit | notes |
   | --- | --- | --- |
   | `kpa` | kPa | filtered absolute pressure, 2 dp |
   | `vac` | kPa | `ambient - kpa`, clamped at 0 |
   | `pct` | % | `vac / ambient * 100` |
   | `target` | kPa | `-` when unset |
   | `floor` | kPa | deepest reachable target, `ambient - 60 kPa` |
   | `duty` | % | pump duty |
   | `state` | — | `IDLE`, `PULL`, `HOLD`, `VENT`, `FAULT` |
   | `remain` | s | countdown, `-` when no timer |
   | `total` | s | configured timer, `-` when none |
   | `batt` | % | `-` when no battery is fitted |
   | `sensor` | — | `1`/`0` |
   | `fault` | — | `none`, `max_run`, `sensor` |
   | `up` | ms | uptime |
4. **`info` payload**: `proto=2 fw=… chip=… heap=… tach=0/1 batt_v=… batt_pct=…`.
5. **Command table**, one row per command with its exact reply. `set` is a family so
   the grammar stays learnable as fields are added:

   | Command | Reply |
   | --- | --- |
   | `help` | `ok …` one line per command |
   | `info` | `ok proto=2 …` |
   | `status` | `ok <status payload>` |
   | `start` | `ok started` / `err not_ready` |
   | `stop` | `ok stopped` (pump off, then vent) |
   | `set target <kPa\|off>` | `ok target=40.0` |
   | `set timer <seconds\|off>` | `ok timer=600` |
   | `set ambient <kPa>` | `ok ambient=101.33` |
   | `ambient` | `ok ambient=101.33` (capture now) |
   | `set period <ms>` | `ok period=250` |
   | `reset` | `ok reset` (clear the pump latch and sensor errors, then vent) |
   | `reboot` | `ok rebooting` |

   `set target` is the **only** actuator command (Q15): there is no raw duty, because a
   second way to drive the pump would mean a second control path to reason about. The
   accepted cost is that characterizing the pump needs a temporary build (R8).

   There is no `telemetry on`/`off`: the notifications are driven by the CCCD, so
   subscribing to `state` *is* asking for telemetry and the subscription can never
   disagree with a command. Only the period is configurable.

   Vent timings are `core/Config.h` values rather than commands, to keep the surface to
   this table (Q13).
6. **Error codes**: `unknown_command`, `bad_argument`, `out_of_range`, `not_ready`,
   `unsupported`. Every `err` is one of these five, so clients can switch on it.
7. **Framing**: none needed when the payload fits the negotiated MTU. The 0x01/0x02
   chunking stays as the fallback for a client that never raises MTU above 23 (a
   status line is ~120 bytes), documented as such.
8. **Worked examples**: a complete session per command group, plus a telemetry burst,
   plus what a client sees on connect and on disconnect.
9. **Migration from v1**: the table of what changed (characteristics, telemetry
   prefix, command names, the removed transfer section, the removed `log`).

`tools/vacuum_cli.py` follows: `status`, `start`, `stop`, `set …`, `telemetry`, `watch`,
`info`, `log`, `reset`, `reboot`. Its `--help` text mirrors §7.5.

## 8. Phases

Each phase builds green, passes `check-layers.ps1`, and is its own commit.

| # | Phase | Content | Verify |
| --- | --- | --- | --- |
| 1 | Rip out | §2.1 deletions (programmability, buttons, `UiController`, the log ring, three pages), `check-layers.ps1` hosts order, command router, snapshot, CLI, docs stubs | green, layer rule ok |
| 2 | Run model, vent, link loss | `VacuumControl` target + timer + `VENT` + `RunState`, `hal/VentValve` (energise to close), snapshot fields, `start`/`stop`/`set`, link-loss policy, `LINK_TIMEOUT_MS` | green |
| 3 | Battery | `hal/Battery`, `hal/Pins.h`, snapshot fields, `batt` keys; no action on a low battery beyond the icon and the key (Q11) | green |
| 4 | Simplified UI | single page, `Renderer` helpers for battery and right aligned text, `StatusPage` rewrite, layout matching §6 | green |
| 5 | Protocol & docs | rewrite `docs/ble-protocol.md`, README (wiring, valve polarity, battery), CLI, `proto=2` | green, docs match code |

Phases 3 and 4 are independent of each other once phase 2 lands; phase 5 wants all of
them.

## 9. Risks

| id | Risk | Mitigation |
| --- | --- | --- |
| R1 | ~~Persistence~~ **closed by Q14**: boot safe, remember nothing, so `hal/Settings` is never written | — |
| R2 | ADC noise or a wrong divider makes the battery read nonsense | `analogReadMilliVolts` + averaging, `present()` gating, documented ±3–5 % and a calibration factor |
| R3 | ~~Valve polarity~~ **closed by Q10**: normally open, so the fail-safe is physical rather than logical | label the drive as energise-to-close in the README and in the header |
| R4 | Dropping the `prog` characteristic breaks any existing client | `proto=2` in `info`, and the §7.9 migration table |
| R5 | Venting on every disconnect (Q4) surprises a client that reconnects into a vented chamber | document it in §7.1, and expose `state=VENT` and `remain=-` in the status payload so a client can see what happened |
| R6 | Revisiting scripting later re-adds complexity | §2.3: the code is a revert of one commit, and the `set` grammar is additive |
| R7 | A solenoid draws continuously while closed, so a long hold is the dominant battery cost, not the pump | budget the battery for hold current (Q10) and state the number in the README |
| R8 | Q15 removes raw duty, so characterizing the pump needs a temporary build | accepted; say so in the protocol doc, so nobody hunts for a command that is not there |

## 10. Decisions

| # | Question | Answer | Where it lands |
| --- | --- | --- | --- |
| Q1 | what the countdown means | hold time remaining, starting when the target is first reached | §5; `target off` means no timer |
| Q2 | vent valve | plan for one | §4.3 output on GPIO5, §5 `VENT` leg |
| Q3 | buttons | unused at this time | superseded by revision 3: no local control at all (§2.1, §6.1); Q16 keeps the pins |
| Q4 | BLE disconnect | stop and vent | §5 policy, §7.1 |
| Q5 | pages | one page | §6; three page files deleted |
| Q6 | battery | 1S Li-ion, ÷2 divider, 3.2–4.2 V | §4.2; `batt` in the status payload |
| Q7 | persistence | yes | **superseded by Q14**: boot safe, remember nothing |
| Q8 | `log` command and tail | drop | §2.1; `core/Log` keeps the sink, loses the ring and `logTail()` |
| Q9 | buzzer | drop, keep the battery | no `hal/Buzzer`; `hal/Pins.h` changes |
| **rev 3** | control model | the device is a BLE peripheral with no local control surface; the display is a readout | §1 control split, §5 state machine, §6, §7 protocol |
| Q10 | valve fail-safe | **normally open**: energise to close, so any loss of control vents the chamber | §4.3, §5, R3, R7 |
| Q11 | low battery | no specific action; the icon and the `batt` key are the whole story | §4.2, §7.5 |
| Q12 | button pins | *(superseded by Q16)* | — |
| Q13 | vent timing | config only (`VENT_MARGIN_KPA`, `VENT_TIMEOUT_MS`) | §7.5 |
| Q14 | persistence | boot safe and remember nothing; `hal/Settings` is never written | §5 boot, §3, R1 |
| Q15 | raw duty | no: `set target` is the only actuator surface, and `MANUAL` goes with it | §5, §5.1, §7.5, R8 |
| Q16 | button pins | keep them in `hal/Pins.h` for a future local override | §2.1, §4.1 |
| Q17 | link timeout | default off, `LINK_TIMEOUT_MS = 0` | §7.1 |

Revision 3's knock-ons, recorded here so they do not read as oversights:

* the buttons stop being hardware we merely do not read and become hardware this device
  does not have (Q16)
* persistence loses its purpose: if the central owns intent, the device remembers
  nothing and boots safe (Q14)
* `DONE` is dropped from the state list (§5)
* `telemetry on`/`off` is replaced by the CCCD subscription (§7.5)

Net effect: a pressure sensing pump controller with a valve, a battery gauge, a display
and a BLE interface. Everything else is gone.

## 11. Status

No open questions: Q1–Q17 are all answered and tabulated in §10. The plan is ready to
execute phase by phase per §8. Delete this file when the work is done.
