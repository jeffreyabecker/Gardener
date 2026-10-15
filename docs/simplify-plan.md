# Plan: remove programmability, simplify the instrument

Status: **proposed** · owner: unassigned · scope: `libraries/VacuumCore`, `docs/`, `tools/`

Annotation guide: decisions taken here are marked `DECISION:`, open questions are
numbered **Q1…Q9** at the end. Edit in place and the plan will be picked up from your
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
* a **simplified display**: battery, current kPa, target kPa, countdown timer
* a **piezo buzzer** for start, target reached, complete, fault and low battery

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
| README's Scripting section, `docs/ble-protocol.md`'s transfer section | replaced by a roadmap note |

`scripts/check-layers.ps1` also loses `script` from the allowed `hosts/` order, so the
guardrail keeps describing the design rather than the plan.

### 2.2 What stays

Everything that makes it an instrument: MPRLS sensing with filtering, the PWM pump
driver with ramp/duty/run-time protection, the PID hold, the SH1106 UI, four buttons,
the BLE command/reply/telemetry transport, the layered layout with its enforced
dependency rule, and `core/ReplyQueue` (the transport still needs to hand command
lines to the main loop).

### 2.3 Where the removed code lives

It is all in git at commit `f35a3bb` (`hosts/script`, `services/ProgramStore`,
`services/ProgramUploader`, `hal/Storage`, the `prog` characteristic and the page).
The plan is that revisiting scripting is a selective revert plus a re-add of
`hosts/script/`: the layering, the command grammar (`set …` is extensible) and the
snapshot are all still there to hang it from.

## 3. Shape after removal

```
core/      Config, Snapshot, Math, Pressure, Format, Log, Pid, ReplyQueue
hal/       Pins, Device, Battery, Pump, Tachometer, PressureSensor, Display, Buttons, Buzzer, Settings
services/  VacuumControl (target + timer + safe stop), SnapshotBuilder, SnapshotText
hosts/     commands/CommandRouter, ble/BleService
ui/        Renderer, UiController, pages/StatusPage.cpp
app/       App
```

Two additions rather than removals (`hal/Battery`, `hal/Buzzer`) and one replacement:
`hal/Settings` (NVS via `Preferences`) takes over the one job the filesystem still had
— remembering a few settings across a reboot — without a filesystem. See Q7.

## 4. New hardware

### 4.1 Pin map

| Signal | GPIO | Notes |
| --- | --- | --- |
| Button UP / DOWN / OK / BACK | 0 / 1 / 3 / 10 | unchanged, active low |
| I2C SDA / SCL | 8 / 9 | unchanged, MPRLS + OLED |
| Pump PWM | 6 | unchanged, MOSFET gate |
| Pump tach | 7 | optional, still disabled by default |
| **Battery sense** | **4** | ADC1 channel 4, through a divider |
| **Buzzer** | **5** | LEDC tone output |
| spare | 2 | strapping pin, leave alone |

`DECISION:` battery on GPIO4 and buzzer on GPIO5. ESP32-C3 ADC1 is GPIO0..4 and three
of those are buttons, so GPIO4 is the only free ADC1 pin; GPIO5 sits on ADC2, which we
do not want, but it drives an LEDC channel perfectly.

### 4.2 Battery

```
BAT+ ── 100k ──┬── GPIO4
               │
              22k ── GND            (÷5.5, good for a 12 V pack)
```

* `hal/Battery.h`: `Battery(const BatteryConfig&)` with `adcPin`, `dividerRatio`,
  `emptyVolts`, `fullVolts`, `samples`, then `void update(uint32_t nowMs)`,
  `float volts()`, `uint8_t percent()`, `bool low()`, `bool present()`.
* Reads with `analogReadMilliVolts()` (the calibrated path in the ESP32 core) and
  averages, because a single ADC sample on this part is noisy.
* Percent from a straight line between the configured empty and full volts. The ESP32
  ADC is not lab grade; the plan documents ±3–5 % and a `BATTERY_CALIBRATION` factor
  for trimming. Q6 asks about chemistry.
* `present()` is false when the reading is below a plausible minimum, so a
  mains-powered build does not show a permanent 0 % warning.

### 4.3 Buzzer

* `hal/Buzzer.h`: passive piezo on LEDC, so it can play tones rather than click:
  `Buzzer(const BuzzerConfig&)`, `update(nowMs)`, `play(Pattern)`, `beep(hz, ms)`,
  `setEnabled(bool)`, `bool enabled()`.
* **Non-blocking.** Patterns are a short list of tones with durations that `update()`
  walks; a blocking `delay()` in the buzzer would stall the control loop.
* Patterns: `Start` (short, 2 kHz), `Reached` (two short), `Complete` (three short),
  `Fault` (one long, 400 Hz), `LowBattery` (two low). All configurable in `core/Config.h`.
* Muting is a setting (BACK long-press and a BLE command), persisted through
  `hal/Settings`.

## 5. Run model

Today the controller only knows a target. The countdown timer needs it to know about a
*run*:

```
IDLE ──start(target, seconds)──> PULLING ──pressure <= target──> HOLDING ──timer 0──> COMPLETE
  ^                                  │                              │
  └────────── stop() / abort ────────┴──────────────────────────────┘
                                   FAULT (sensor lost, pump latch)
```

* `VacuumControl` gains: `startRun()`, `stopRun()`, `setTarget(kpa)`,
  `setDurationSeconds(uint32_t)`, `remainingSeconds()`, `RunState state()`,
  `bool consumeCompleted()`.
* `DECISION:` the countdown starts when the pressure first reaches the target, so the
  number on the screen means "hold time remaining". Q3 asks to confirm.
* On expiry: pump off, state `COMPLETE`, `consumeCompleted()` returns true once, which
  is what `App` uses to play the buzzer pattern. No new event machinery: `App` already
  owns the wiring.
* `target off` (or `set target off`) means pull down and stay there with no timer.
* Existing protections remain: duty cap, slew limit, max run time, sensor-loss latch.

## 6. Simplified display

One page. No page navigation, so no cursor, no graphs, no info page — diagnostics are
available over BLE.

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

### 6.1 Buttons

| Button | Action |
| --- | --- |
| UP / DOWN | target ∓1 kPa (auto-repeat comes free from `hal/Buttons`) |
| OK click | start the run / stop the run |
| OK long | cycle the timer preset: off, 1, 5, 10, 30 min |
| BACK click | abort: pump off, timer cleared, state `IDLE` |
| BACK long | mute or unmute the buzzer |

Q5 asks to confirm this mapping.

### 6.2 Buzzer events

Fired by `App` from state transitions, never from inside a driver:
`start` → `Start`, reached target → `Reached`, timer expiry → `Complete`, `FAULT`
entry → `Fault`, battery crossing the low threshold (with hysteresis) → `LowBattery`.

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
   `state`; what happens on disconnect (Q4).
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
   | `state` | — | `IDLE`, `PULL`, `HOLD`, `DONE`, `FAULT` |
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
   | `stop` | `ok stopped` |
   | `set target <kPa\|off>` | `ok target=40.0` |
   | `set timer <seconds\|off>` | `ok timer=600` |
   | `set ambient <kPa>` | `ok ambient=101.33` |
   | `ambient` | `ok ambient=101.33` (capture now) |
   | `set buzzer <on\|off>` | `ok buzzer=off` |
   | `telemetry <on\|off\|ms>` | `ok telemetry=on period=250` |
   | `reset` | `ok reset` (clear the pump latch and sensor errors) |
   | `reboot` | `ok rebooting` |
   | `log [n]` | `ok` then the tail, then `eof` |
6. **Error codes**: `unknown_command`, `bad_argument`, `out_of_range`, `not_ready`,
   `unsupported`. Every `err` is one of these five, so clients can switch on it.
7. **Framing**: none needed when the payload fits the negotiated MTU. The 0x01/0x02
   chunking stays as the fallback for a client that never raises MTU above 23 (a
   status line is ~120 bytes), documented as such.
8. **Worked examples**: a complete session per command group, plus a telemetry burst,
   plus what a client sees on connect and on disconnect.
9. **Migration from v1**: the table of what changed (characteristics, telemetry
   prefix, command names, the removed transfer section).

`tools/vacuum_cli.py` follows: `status`, `start`, `stop`, `set …`, `telemetry`, `watch`,
`info`, `log`, `reset`, `reboot`. Its `--help` text mirrors §7.5.

## 8. Phases

Each phase builds green, passes `check-layers.ps1`, and is its own commit.

| # | Phase | Content | Verify |
| --- | --- | --- | --- |
| 1 | Rip out programmability | §2.1 deletions, `check-layers.ps1` hosts order, command router, snapshot, CLI, docs stubs | green, layer rule ok, `status` unchanged in shape |
| 2 | Run model | `VacuumControl` target + timer + `RunState`; snapshot fields; `start`/`stop`/`set timer` | green |
| 3 | Battery | `hal/Battery`, `hal/Pins` pins, snapshot fields, `batt` keys | green |
| 4 | Buzzer | `hal/Buzzer`, patterns, `App` event wiring, mute setting + `set buzzer` | green |
| 5 | Simplified UI | single page, `Renderer` helpers, delete `Graph`/`Info`/`Programs` pages, button mapping, `hal/Settings` | green, layout matches §6 |
| 6 | Protocol & docs | rewrite `docs/ble-protocol.md`, README, CLI, `proto=2` | green, docs match code |

Once phase 1 lands, phases 2–6 are independent of each other and can be reordered.

## 9. Risks

| id | Risk | Mitigation |
| --- | --- | --- |
| R1 | Deleting the store loses the only persistence, so the target resets on every reboot | `hal/Settings` (NVS) in phase 5, or earlier if it annoys us; Q7 |
| R2 | ADC noise or a wrong divider makes the battery read nonsense | `analogReadMilliVolts` + averaging, `present()` gating, documented calibration, Q6 |
| R3 | A blocking buzzer stalls the control loop | pattern player is pumped from `update()`, never `delay()` |
| R4 | Dropping the `prog` characteristic breaks any existing client | `proto=2` in `info`, and §7.9 migration table |
| R5 | Revisiting scripting later re-adds complexity | §2.3: the code is a revert of one commit, and the `set` grammar is additive |

## 10. Open questions

* **Q1** — confirm the run model in §5: does the countdown start when the target is
  first reached (recommended, "hold time remaining"), or the moment the run starts
  ("total run time")? Recommendation: on reaching target, with `target off` meaning
  "no timer".
* **Q2** — should reaching the target and completing the timer also *vent*? There is no
  vent valve in the BOM, so the answer is "coast and bleed up" today. If a vent valve is
  planned, it changes the state machine now rather than later.
* **Q3** — button mapping in §6.1: is "OK long = cycle timer preset" the right place for
  the timer, or would you rather set the duration only over BLE and keep OK long free?
* **Q4** — on BLE disconnect, should a running pump keep running (recommended, the timer
  and max run time protect it) or stop (safer for an unattended rig)?
* **Q5** — one page only, or keep a second screen for faults and firmware/heap now that
  there is no page cursor? Recommendation: one page, faults as an inverted banner.
* **Q6** — battery chemistry and pack voltage: Li-ion (3.0–4.2 V, ÷2 divider), 12 V
  lead/AGM (10.5–12.8 V, ÷5.5) or something else? This sets the divider and the empty/full
  volts, and whether a low-battery warning should stop a run.
* **Q7** — persist settings (target, timer, buzzer mute, ambient) in NVS with
  `hal/Settings`? Recommendation: yes, it is ~30 lines and it is the one thing the
  filesystem was still doing for us.
* **Q8** — keep the `log` command and the in-RAM log tail, or drop it now that there is
  no `eval`/script debugging? Recommendation: keep, it is the only way to pull a trace
  from a sealed unit.
* **Q9** — the buzzer and battery are new hardware: is this a new board revision (then
  `hal/Pins.h` changes and the old build stops matching), or a bodge on the existing
  one? It only matters for how loudly the docs have to say "your board may not have
  these".
