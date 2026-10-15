# Plan: drop Bluetooth, collapse the layers, delete the dead surface

Revision 2 of the serial plan. Revision 1 swapped the transport; this revision also
cleans out what the session accumulated around it. The motivation is a file count and a
dependency graph that no longer match what the firmware does:

* **42 files** under `src/`, of which the instrument itself is about a dozen: a pump, a
  pressure sensor, a valve, a battery gauge, a display, a control loop, a command
  grammar and a transport.
* **six layers** for those files, one of which (`services/`) will hold a single file once
  the snapshot mechanism goes, and another (`ui/`) holds one screen.
* **an audit found more than a dozen dead surfaces**, including a whole driver (the
  tachometer), a whole file (five unit conversions with zero consumers), four unused
  display primitives, and accessors whose only reader is the snapshot mechanism itself.

Two of those dead surfaces are the ones that were flagged during the work:
`core/ReplyQueue.h` (a hand-off queue whose only reason was the NimBLE task) and
`hal/Tachometer.h` (a driver for hardware that is not fitted and is disabled by default).

## 1. The cruft inventory

Everything below was verified by grep, not by memory. "0 consumers" means the only
occurrence of the symbol is its own declaration and definition.

| Item | Evidence | Size |
| --- | --- | --- |
| `core/ReplyQueue.h` | a queue between the NimBLE task and the main loop; a wire has one loop | 44 lines |
| `core/Pressure.h` | `kpaToHpa`, `kpaToBar`, `kpaToPsi`, `kpaToInHg`, `kpaToMmHg`: 0 consumers. kPa is the only unit in the firmware | 15 lines |
| `hal/Tachometer.{h,cpp}` | `TACH_ENABLED = false`, no wire fitted, ISR + RPM window + smoothing for a signal nobody reads | 50 lines, 2 files |
| `pins::PUMP_TACH`, `TACH_ENABLED`, `TACH_PULSES_PER_REV`, `PumpConfig::tach*`, `Pump::rpm()`, `Pump::tachEnabled()`, `tach=` in `info`, `rpm` in the snapshot | all of it exists to serve the above | ~20 more lines |
| `hal/Device.{h,cpp}` | `readDeviceInfo()` returns 3 fields; `sdk` is **never emitted**, so 2 live fields | 2 files, 40 lines |
| `snapshot.device` | a `const DeviceInfo*` in a struct that is copied every loop, to reach 2 strings | — |
| `Display::hline()`, `Display::line()`, `Display::height()`, `Renderer::height()` | 0 consumers; `height()` on both dies together | ~10 lines |
| `Pump::targetPercent()`, `Pump::running()`, `Pump::runTimeMs()`, `Pump::faultText()`, `Pump::resetFault()`'s unused `bool` return | 0 consumers (`faultText` only fed the snapshot; the log can use the parameter) | ~14 lines |
| `PressureSensor::healthy()`, `kpaRaw()`, `errorCount()`, `lastSampleMs()`, `clearErrors()`, and a public `readNow()` nothing outside the class calls | 0 consumers; the counter is incremented and never read | ~12 lines |
| `VacuumControl::consumeCompleted()`, `completed_` | its own comment says "nothing consumes it today" | ~8 lines |
| `pins::BUTTON_DEBOUNCE_MS`, `BUTTON_LONGPRESS_MS`, `BUTTON_REPEAT_MS` | nothing reads them since `hal/Buttons` was deleted | 3 lines |
| 4 `*Config` structs and 4 `App::*Config()` helpers | every caller passes the defaults; the only real variation is the pin number, which `hal/Pins.h` already owns | ~60 lines |
| `core/Snapshot.h`, `services/SnapshotBuilder.{h,cpp}`, `services/SnapshotText.{h,cpp}` | a 40-field struct re-traversed and copied **every loop iteration**, for two consumers | 6 files |
| `ui/Renderer.{h,cpp}`, `ui/pages/Pages.h`, `ui/pages/StatusPage.cpp` | three levels of indirection for one screen | 4 files |
| `String argv[8]` + `splitArgs()` with an unused `rest` out-parameter | the grammar needs verb, key, value; 8 heap-allocated `String`s per command line to produce 2 | ~25 lines |
| 6-layer rule with a sub-folder ordering map | 90 lines of PowerShell to police 42 files | — |

## 2. What the radical version looks like

```
core/     Config, Math, Format, Log(+cpp), Pid(+cpp)          shared foundations
hal/      Pins, Display, PressureSensor, Pump, VentValve, Battery, + .cpp each
app/      App, VacuumControl, CommandRouter, SerialService, StatusPage
```

* `core/` and `hal/` keep their meaning: **`hal/` is the base hardware abstraction, and
  that is the delineation that has to survive.** What goes is the middle: `services/`,
  `hosts/` and `ui/` are all "the instrument", and they stop being three folders with a
  graph between them.
* The page draws, the controller controls, the router parses, the transport moves bytes,
  and `App` wires them: five collaborators in one folder beats sixteen files spread over
  four layers that each may include a subset of the others.
* The layer rule becomes three lines of intent, checked by a script a third of the size:

```
core/   may not include hal/ or app/
hal/    may not include app/
app/    may include anything
```

`DECISION:` `core/` is renamed in intent only, not in path: it is "shared foundations",
not "pure logic", because `Log` and `Format` already use `Arduino.h` and `String`. The
README claims purity today and it was already only half true — the honest version is
"`hal/` never sees `app/`", which is the rule that actually protects the design.

## 3. What replaces the snapshot

`SystemSnapshot` exists so `ui/` and `hosts/` can read the machine without depending on
the modules that produce it. It is a real seam, and it costs:

* a struct with ~40 fields, kept in sync by hand in three places (producer, wire encoder,
  page)
* `buildSnapshot()` returning ~100 bytes **by value**, copied into `App::snapshot_` on
  every trip round the main loop
* a full traversal of pump, sensor, control, battery and device per iteration to rebuild
  what the readers could have read
* four fields nobody reads (`rpm`, `tachEnabled`, `faultText`, `bleConnected` before it
  went) because keeping the struct complete was easier than auditing it

In the collapsed tree both consumers live in `app/`, beside the modules, so the seam
protects nothing:

* **`CommandRouter`** already holds `VacuumControl&`, `Pump&`, `PressureSensor&` and
  `TelemetrySettings&`; `status`/`info` become `snprintf` over those references plus
  `Battery&` and two `ESP.*` calls. This also fixes a layering smell: the **wire payload
  encoder** stops living in `services/` (where it encoded a transport's format) and lives
  with the grammar that produces it.
* **`StatusPage`** takes `const` references to the pump, sensor, control and battery and
  draws. It gained nothing from a copied snapshot it had to be told how to fill.

The consistency the snapshot bought — every reader seeing one instant — is worth naming
and dismissing: the sensor itself re-samples every 20 ms and is median+EMA filtered, and
the display and a reply are microseconds apart. Values that move slowly do not need a
frozen sample.

`DECISION:` delete the mechanism. Adding one value to the status payload becomes one edit
in one function, instead of a field, a producer line and two readers.

## 4. The transport (from revision 1, unchanged)

| Concern | Decision |
| --- | --- |
| Port | **USB CDC**, reached through a `Stream&` seam; FQBN gains `CDCOnBoot=cdc`, pinned in `sketch.yaml`, `scripts/arduino.ps1` and `.vscode/arduino.json` |
| Why not UART0 | `esp32:esp32:esp32c3` ships `CDCOnBoot=Disabled`, so `Serial` is `Serial0` = UART0 on GPIO20/21, which is **not** the USB-C socket: today's console output never reaches a USB cable. A UART also cannot detect a host (`HardwareSerial::operator bool()` is `uartIsDriverInstalled()`, true forever), while `HWCDC`'s is real presence (`isCDC_Connected()`) |
| Framing | none above the line: newline terminated, `Stream&`, bounded line buffer, `\r` or `\n` terminates, `COMMAND_LINE_MAX = 128` |
| Replies | the router writes finished lines to a `Print&`; `ReplyQueue` and the whole producer/consumer dance go |
| Log | shares the stream under its existing `[I]` prefix: lines starting with `[` are log records, `ok `/`err ` are replies. `set log <on\|off>` and `log=on\|off` in `info` let a controller insist on a clean channel |
| Link loss | a dropped USB CDC link stops the run and vents (mirrors Q4/Q10); the silent-link watchdog is dropped, because a cable that is present and quiet is the normal case |
| Parsing | in-place three-token parse (`verb [key] [value]`) instead of `String argv[8]` and an unused `rest` |

## 5. Protocol v3

Grammar, status keys, info keys, the five error codes and the state model survive
verbatim. Three changes:

1. `proto=3`.
2. **Telemetry is polled, not pushed** — the CCCD subscription was the radio's way of
   asking "does anyone want this", so `TelemetrySettings`, `set period` and the push path
   go. A controller that wants 4 Hz sends 4 `status` a second, which is nothing at
   115200, and the device's behaviour stays a pure function of the commands it received.
3. `set log <on|off>` and `log=on|off` in `info`.

`reset` also changes meaning slightly, as a consequence of the dead-surface sweep: with
the sensor error counter gone (nothing read it) it is the pump latch and the controller
fault, nothing else.

## 6. What must not change

The instrument. `hal/`'s behaviour is identical after the sweep — the pump still ramps,
caps duty and trips on `PUMP_MAX_RUN_MS`; the MPRLS still does median + EMA with an
ambient reference; the valve still energises to close and fails open; the battery still
averages trimmed millivolts; `VacuumControl` still owns the run model, the countdown
start, the vent margin and the fault latch. This is a deletion and a reshuffle, not a
redesign of the machine.

## 7. Phases

Each phase builds green, passes `scripts/check-layers.ps1`, and is its own commit.
Phases 1 and 2 change behaviour; phase 3 must not, so a regression has somewhere to hide.

| # | Phase | Content | Verify |
| --- | --- | --- | --- |
| 1 | Serial transport | `SerialService` over `Stream&`, delete `hosts/ble` and `core/ReplyQueue.h`, router writes to `Print&`, `proto=3`, delete `TelemetrySettings`/`set period`/`LINK_TIMEOUT_MS`/`BLE_*`/`TELEMETRY_*`, add `set log` and `log=` in `info`, FQBN in three files, `libs` drops NimBLE, `check-layers.ps1` `ble` → `serial` | green, layer rule ok, **flash and RAM measured** to price the radio |
| 2 | Dead surface sweep | the inventory in §1, minus the structural items: `Pressure.h`, `Tachometer`, `Device` (router calls `ESP.*`), the unconsumed accessors, the four `*Config` structs and `App::*Config()`, button timings, `Pid` folded into `VacuumControl`, three-token parsing | green |
| 3 | Collapse and delete the snapshot | move to `core/`+`hal/`+`app/`, delete `Snapshot.h`/`SnapshotBuilder`/`SnapshotText`, page and router read modules, `Renderer`+`Pages.h`+`StatusPage` → one `StatusPage`, `RunState` moves to `VacuumControl.h`, `App` slims, `check-layers.ps1` rewritten small | green, layer rule ok, **no value changed**: same payloads, same display, verified by diff-reading the router's output |
| 4 | Docs and tools | `docs/serial-protocol.md` replaces `docs/ble-protocol.md`, README rewritten for three layers and no snapshot, `tools/vacuum_cli.py` on pyserial, `requirements.txt`, `FIRMWARE_VERSION` → `0.2.0`, this plan deleted | green, `python -m py_compile` |

## 8. Risks

| id | Risk | Mitigation |
| --- | --- | --- |
| R1 | the tethered instrument; no phone can drive it | accepted. The line grammar is unchanged, so a wireless bridge later is a transport, not a redesign |
| R2 | a controller that dies mid-run leaves a sealed chamber | unchanged device protections: timer expiry, `PUMP_MAX_RUN_MS`, fault latch, and a valve that fails open |
| R3 | building without `CDCOnBoot=cdc` sends the console to GPIO20/21 and looks dead | FQBN pinned in three files, README states the failure mode, boot banner goes to the stream in use |
| R4 | opening the port from the host may reset the ESP32-C3 (USB-Serial-JTAG reset sequences) | verify on hardware; boot-safe means an aborted run and a vented chamber, so document it either way |
| R5 | the log corrupts a parsing controller's replies | documented `[` rule, `set log off`, and no unmarked line is ever emitted |
| R6 | losing the snapshot removes a seam a future second transport would want | accepted and named: the collapse is the point. If a second transport arrives (a radio bridge, a second MCU), the seam to reintroduce is a *view for the transport*, not a view for the UI and the transport and everything else |
| R7 | losing the `*Config` structs removes the DI seam for a board variant | `hal/Pins.h` becomes that seam explicitly, which is where a board variant edits today anyway |
| R8 | deleting a driver for hardware that is not fitted looks like losing capability | the tach is a revert of one commit when a wire is fitted, exactly like the Berry removal; say so in the commit |
| R9 | phase 3 is large and could hide a behaviour change in a reshuffle | keep it behaviour-free by contract, and keep the wire-visible changes in phases 1 and 2 |

## 9. Questions

Recommendations are marked, so "all as recommended" is a valid answer. Q1–Q5 restate
revision 1's questions, unchanged by this revision.

| # | Question | Recommendation |
| --- | --- | --- |
| Q1 | Port and seam: USB CDC via `CDCOnBoot=cdc`, reached through a `Stream&`? | **yes** |
| Q2 | Telemetry: poll `status` only, deleting `TelemetrySettings` and `set period`? | **yes** |
| Q3 | Log: shares the stream under the `[` rule, plus `set log <on\|off>` and `log=` in `info`? | **yes** |
| Q4 | Link: dropped USB CDC link stops and vents; silent-link watchdog dropped? | **yes** |
| Q5 | Parsing: `\r` or `\n` terminates; 128-byte line cap? | **yes** |
| Q6 | Layers: collapse `services/`, `hosts/`, `ui/` into `app/`, leaving `core/`, `hal/`, `app/`? | **yes** |
| Q7 | The snapshot: delete `Snapshot.h`, `SnapshotBuilder`, `SnapshotText` and let the page and the router read modules? | **yes** — this is the big one, and the trade-off is R6 |
| Q8 | `Pid`: fold into `VacuumControl` (one consumer, no tests, gains already in `core/Config.h`)? | **yes** |
| Q9 | Tachometer: delete the driver, its pin, its config, the `tach=` key and `rpm` until a tach is fitted? | **yes** |
| Q10 | `hal/Device`: delete it and let the router call `ESP.getChipModel()`/`getFreeHeap()` for `info`? | **yes** |
| Q11 | The four `*Config` structs and `App::*Config()` helpers: delete, with `hal/` reading `pins::`/`cfg::` directly? | **yes** (R7) |
| Q12 | The dead accessors in §1, plus `Pump::resetFault()` becoming `void` and `PressureSensor::readNow()` becoming private, and `reset` losing its sensor-counter line? | **yes** |
| Q13 | UI: `Renderer` + `pages/Pages.h` + `StatusPage` → one `app/StatusPage.{h,cpp}`? | **yes** |
| Q14 | `scripts/arduino.ps1`: drop `-Task link` (the IDE 2 junction, never exercised here) and keep build/upload/monitor/libs/clean? | **yes** |
| Q15 | Display: add the small `USB` token on the state row when a controller is attached? | **yes** — the display is a readout, and "is anyone listening" is the one thing it cannot otherwise tell you |
| Q16 | Versions and tools: firmware `0.2.0`, `proto=3`, `docs/serial-protocol.md` replaces `docs/ble-protocol.md`, `tools/vacuum_cli.py` rewritten on pyserial, this plan deleted at the end? | **yes** |
| Q17 | Button pins: keep the four pin numbers with a "nothing reads them" comment? | **yes** (Q16 from the earlier plan stands) |

## 10. Status

| Revision | Change | Sections |
| --- | --- | --- |
| rev 1 | transport swap to serial, polling telemetry, log sharing, link policy, five phases | all |
| rev 2 | adds the cruft inventory, the three-layer collapse, the snapshot deletion, the dead-surface sweep, `0.2.0`/`proto=3`, renumbers the questions | §1–§3, §7–§9 |
