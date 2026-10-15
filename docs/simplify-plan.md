# Plan: remove programmability, simplify the instrument

Status: **reviewed** · revision 2 · owner: unassigned · scope: `libraries/VacuumCore`, `docs/`, `tools/`

Revision 2 folds in the answers to Q1–Q9, recorded in §10. Three of them changed the
shape of the job: a **vent valve** is now part of the design (Q2), and the **buttons and
the buzzer are gone** (Q3, Q9), which deletes as much code as the programmability rip
out does and rearranges the pin map. New questions raised by those answers are §11.

Annotation guide: decisions taken here are marked `DECISION:`, open questions are
numbered **Q10…Q13** at the end. Edit in place and the plan will be picked up from your
edits. Delete this file when the work is done.

## 1. Goal

The instrument is a vacuum controller: pull to a target, hold it, tell the operator
what is happening. Programmability (uploading Berry programs, a filesystem, a runtime,
an upload protocol, a programs page) has grown to touch every layer, and it is not the
product. Take it out, get the protocol and the screen back down to something that fits
on a bench instrument, and revisit scripting later as an add-on rather than a
foundation.

In the same pass:

* a **clearly documented BLE protocol**: small enough to read in one sitting, with a
  formal grammar and one status payload shape
* a **simplified display**: battery, current kPa, target kPa, countdown timer — and the
  only local feedback the operator has
* a **vent valve**, so a stopped, completed, faulted or disconnected run releases the
  chamber instead of bleeding away through a leak
* **no buttons and no buzzer**: the four buttons are not read at this time (Q3) and the
  piezo is dropped (Q9), which makes the protocol the only control surface

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
| `hal/Buttons.*` | the four buttons are not read at this time (Q3); the pins stay reserved |
| `ui/UiController.*`, and `UiSettings` (page step, toast) in the snapshot | with no buttons there are no events, so no intents, no cursor and no page navigation |
| `ui/pages/{Graph,Info,Programs}Page.cpp` | one page (§6) |
| `core/Log`: the ring buffer, `logTail()` and the `log` command | nothing to debug remotely now (Q8); the sink stays |
| README's Scripting section, `docs/ble-protocol.md`'s transfer section | replaced by a roadmap note |

`scripts/check-layers.ps1` also loses `script` from the allowed `hosts/` order, so the
guardrail keeps describing the design rather than the plan.

### 2.2 What stays

Everything that makes it an instrument: MPRLS sensing with filtering, the PWM pump
driver with ramp/duty/run-time protection, the PID hold, the SH1106 display, the BLE
command/reply/telemetry transport, the layered layout with its enforced dependency rule,
and `core/ReplyQueue` (the transport still needs to hand command lines to the main
loop). New rather than kept: a vent valve output, a battery gauge and NVS settings.

### 2.3 Where the removed code lives

It is all in git at commit `f35a3bb` (`hosts/script`, `services/ProgramStore`,
`services/ProgramUploader`, `hal/Storage`, the `prog` characteristic and the page).
The plan is that revisiting scripting is a selective revert plus a re-add of
`hosts/script/`: the layering, the command grammar (`set …` is extensible) and the
snapshot are all still there to hang it from.

## 3. Shape after removal

```
core/      Config, Snapshot, Math, Pressure, Format, Log (sink only), Pid, ReplyQueue
hal/       Pins, Device, Battery, Pump, VentValve, Tachometer, PressureSensor, Display, Settings
services/  VacuumControl (target + timer + vent + safe stop), SnapshotBuilder, SnapshotText
hosts/     commands/CommandRouter, ble/BleService
ui/        Renderer, pages/StatusPage.cpp
app/       App
```

Three additions rather than removals (`hal/Battery`, `hal/VentValve`, `hal/Settings`),
one replacement and one collapse: `hal/Settings` (NVS via `Preferences`) takes over the
one job the filesystem still had — remembering a few settings across a reboot — without
a filesystem, and `ui/` collapses to a renderer, because with no buttons there is no
interaction state left to own.

## 4. New hardware

### 4.1 Pin map

| Signal | GPIO | Notes |
| --- | --- | --- |
| Button UP / DOWN / OK / BACK | 0 / 1 / 3 / 10 | **reserved, not read** (Q3) |
| I2C SDA / SCL | 8 / 9 | unchanged, MPRLS + OLED |
| Pump PWM | 6 | unchanged, MOSFET gate |
| Pump tach | 7 | optional, still disabled by default |
| **Battery sense** | **4** | ADC1 channel 4, ÷2 divider for a 1S Li-ion cell |
| **Vent valve** | **5** | digital output, MOSFET gate |
| spare | 2 | strapping pin, leave alone |

`DECISION:` battery on GPIO4 and vent valve on GPIO5 (Q3, Q9). ESP32-C3 ADC1 is
GPIO0..4 and three of those are button pins — a fitted button pulls its pin to ground
when pressed, so the battery sense must not inherit a freed button pin even though the
buttons are unread. GPIO4 is clear of both the buttons and the strapping pin. GPIO5 is
free now that the buzzer is dropped, and it drives a valve as happily as a piezo.

`hal/Pins.h` changes are therefore: add `BATTERY_ADC` and `VENT_VALVE`, drop the buzzer
before it is ever added, and keep the button pins with a comment that nothing reads
them yet (Q12).

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

A solenoid valve on the chamber, driven exactly like the pump: a logic level MOSFET with
a flyback diode across the coil (or a relay module if the coil is hungry).

* `hal/VentValve.h`: `VentValve(const VentValveConfig&)` with `pin`, `activeHigh`,
  then `void open()`, `void close()`, `bool isOpen()`, `void update(uint32_t nowMs)`.
* The valve is driven by firmware only. There is no manual override and no button to
  hold it open, so the run model in §5 owns it and every exit from a run funnels through
  it.
* `activeHigh` is an electrical detail; whether a *de-energised* valve is open or closed
  is a fail-safe decision, and it is Q10. The docs have to state which we chose, because
  it decides what the rig does when the power fails.

## 5. Run model

Today the controller only knows a target. A countdown timer and a vent valve both need
it to know about a *run*:

```
        start(target, timer)
IDLE ────────────────────────> PULL ──(target reached)──> HOLD ──(timer 0)──┐
  ^                              │                          │                │
  │                              │  stop / abort / BLE disconnect            │
  │                              v                          v                v
  └────(vent complete)───────── VENT <───────────────────────────────────────┘
                                 ^
        FAULT (sensor lost, pump latch, max run time) ────────┘
```

* `VacuumControl` gains: `startRun()`, `stopRun()`, `setTarget(kpa)`,
  `setDurationSeconds(uint32_t)`, `remainingSeconds()`, `RunState state()`,
  `bool consumeCompleted()`.
* **States**: `IDLE`, `PULL`, `HOLD`, `VENT`, `DONE`, `FAULT`. `DONE` means "the run
  finished and the chamber has been vented"; it clears on the next start. With no buzzer
  and no buttons, `DONE` is how the operator learns the run is over (§6.1).
* `DECISION:` the countdown starts when the pressure first reaches the target (Q1), so
  the number on the screen is "hold time remaining". `target off` means pull down and
  stay there with no timer.
* **Vent closes** when `kpa >= ambient - VENT_MARGIN_KPA` or after `VENT_TIMEOUT_MS`,
  whichever comes first. A timeout is not an error on its own (a leaky rig takes longer),
  but it is logged and the state still returns to `IDLE`/`DONE`.
* `DECISION:` stop, abort, BLE disconnect (Q4) and fault all funnel through `VENT`
  rather than stopping in place, so there is exactly one "release the chamber" path to
  reason about. A fault opens the valve: it is the fail-safe direction.
* `App` fires nothing when a run completes beyond the display change; the buzzer was the
  only consumer of `consumeCompleted()` and it is gone (Q9). The call stays because
  restarting a repeated run or a future notification would want it.
* Existing protections remain: duty cap, slew limit, max run time, sensor-loss latch.

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
| state token | `RunState` | `IDLE`, `PULL`, `HOLD`, `DONE`, `FAULT`; `FAULT` is inverted |
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

Re-adding buttons is a revert of that deletion plus a mapping, and the pins stay
reserved in `hal/Pins.h` for it (Q12).

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
   `state`; and on disconnect the device stops the run and vents (Q4), so a client that
   walks away does not leave a chamber under vacuum.
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
   | `state` | — | `IDLE`, `PULL`, `HOLD`, `VENT`, `DONE`, `FAULT` |
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
   | `telemetry <on\|off\|ms>` | `ok telemetry=on period=250` |
   | `reset` | `ok reset` (clear the pump latch and sensor errors, then vent) |
   | `reboot` | `ok rebooting` |

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
| 2 | Run model, vent, settings | `VacuumControl` target + timer + `VENT` + `RunState`, `hal/VentValve`, `hal/Settings` (NVS), snapshot fields, `start`/`stop`/`set timer`, disconnect and fault policy | green |
| 3 | Battery | `hal/Battery`, `hal/Pins.h`, snapshot fields, `batt` keys, low battery handling per Q11 | green |
| 4 | Simplified UI | single page, `Renderer` helpers for battery and right aligned text, `StatusPage` rewrite, layout matching §6 | green |
| 5 | Protocol & docs | rewrite `docs/ble-protocol.md`, README (wiring, valve polarity, battery), CLI, `proto=2` | green, docs match code |

Phases 3 and 4 are independent of each other once phase 2 lands; phase 5 wants all of
them.

## 9. Risks

| id | Risk | Mitigation |
| --- | --- | --- |
| R1 | ~~Deleting the store loses the only persistence~~ **closed by Q7**: persist, so `hal/Settings` lands with the run model in phase 2 | — |
| R2 | ADC noise or a wrong divider makes the battery read nonsense | `analogReadMilliVolts` + averaging, `present()` gating, documented ±3–5 % and a calibration factor |
| R3 | Valve polarity is a safety decision, not a wiring detail | decide before wiring (Q10) and state the consequence in the README |
| R4 | Dropping the `prog` characteristic breaks any existing client | `proto=2` in `info`, and the §7.9 migration table |
| R5 | Venting on every disconnect (Q4) surprises a client that reconnects into a vented chamber | document it in §7.1, and expose `state=VENT`/`DONE` in the status payload so a client can see it |
| R6 | Revisiting scripting later re-adds complexity | §2.3: the code is a revert of one commit, and the `set` grammar is additive |

## 10. Decisions

| # | Question | Answer | Where it lands |
| --- | --- | --- | --- |
| Q1 | what the countdown means | hold time remaining, starting when the target is first reached | §5; `target off` means no timer |
| Q2 | vent valve | plan for one | §4.3 output on GPIO5, §5 `VENT` leg |
| Q3 | buttons | unused at this time | §2.1 deletions and §6.1; pins stay reserved |
| Q4 | BLE disconnect | stop and vent | §5 policy, §7.1 |
| Q5 | pages | one page | §6; three page files deleted |
| Q6 | battery | 1S Li-ion, ÷2 divider, 3.2–4.2 V | §4.2; `batt` in the status payload |
| Q7 | persistence | yes | `hal/Settings` over NVS in phase 2 |
| Q8 | `log` command and tail | drop | §2.1; `core/Log` keeps the sink, loses the ring and `logTail()` |
| Q9 | buzzer | drop, keep the battery | no `hal/Buzzer`; `hal/Pins.h` changes |

Net effect: a pressure sensing pump controller with a valve, a battery gauge, a display
and a BLE interface. Everything else is gone.

## 11. Open questions (new)

* **Q10** — valve fail-safe polarity, which is a safety decision rather than a wiring
  detail: a normally closed valve (energise to open) leaves the chamber sealed if the
  board crashes or the power fails, a normally open one vents it. Recommendation:
  normally closed, because firmware that has stopped cannot be trusted to hold a valve
  open, and because venting on a brownout can ruin a process. Whichever we pick goes in
  the README, since it decides what the rig does when the lights go out.
* **Q11** — what should a low battery *do* beyond being visible? Options: warn only
  (recommended), refuse to start a new run, or stop a run in progress. With the buzzer
  gone the warning is the battery icon plus the `batt` key, so it only helps a client
  that is watching.
* **Q12** — the four button pins: keep them reserved in `hal/Pins.h` with a comment
  (recommended — it documents the planned hardware and makes re-adding trivial), or free
  them for something else?
* **Q13** — vent timing: config only (`VENT_MARGIN_KPA`, `VENT_TIMEOUT_MS`), or also a
  `set vent <ms>` command? Recommendation: config only, to keep the command surface to
  the §7.5 table.
