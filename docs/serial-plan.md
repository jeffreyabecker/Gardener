# Plan: drop Bluetooth, collapse the layers, delete the dead surface

Revision 2 of the serial plan. Revision 1 swapped the transport; this revision also
cleans out what the session accumulated around it. The motivation is a file count and a
dependency graph that no longer match what the firmware does:

* **42 files** under `src/`, of which the instrument itself is about a dozen: a pump, a
  pressure sensor, a valve, a battery gauge, a display, a control loop, a command
  grammar and a transport. Under this plan that becomes **30**.
* **six layers** for those files, including a `services/` that ends up holding two files, a
  `ui/` that holds one screen, and a `hosts/` that holds a transport and a grammar which
  are two halves of one protocol.
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
| `snapshot.device` | a `const DeviceInfo*` inside otherwise plain data, to reach 2 strings the formatter can read for itself | — |
| `Display::hline()`, `Display::line()`, `Display::height()`, `Renderer::height()` | 0 consumers; `height()` on both dies together | ~10 lines |
| `Pump::targetPercent()`, `Pump::running()`, `Pump::runTimeMs()`, `Pump::faultText()`, `Pump::resetFault()`'s unused `bool` return | 0 consumers (`faultText` only fed the snapshot; the log can use the parameter) | ~14 lines |
| `PressureSensor::healthy()`, `kpaRaw()`, `errorCount()`, `lastSampleMs()`, `clearErrors()`, and a public `readNow()` nothing outside the class calls | 0 consumers; the counter is incremented and never read | ~12 lines |
| `VacuumControl::consumeCompleted()`, `completed_` | its own comment says "nothing consumes it today" | ~8 lines |
| `pins::BUTTON_DEBOUNCE_MS`, `BUTTON_LONGPRESS_MS`, `BUTTON_REPEAT_MS` | nothing reads them since `hal/Buttons` was deleted | 3 lines |
| 4 `*Config` structs and 4 `App::*Config()` helpers | every caller passes the defaults; the only real variation is the pin number, which `hal/Pins.h` already owns | ~60 lines |
| `services/SnapshotText.{h,cpp}` | the wire payload encoder living in `services/`, so a service encodes a transport's format. The two payloads it builds belong with the protocol that defines them | 2 files |
| `buildSnapshot()` returning `SystemSnapshot` **by value** | a struct copy back into `App::snapshot_` every loop iteration, for nothing | 1 signature |
| `ui/Renderer.{h,cpp}`, `ui/pages/Pages.h`, `ui/pages/StatusPage.cpp` | three levels of indirection for one screen | 4 files |
| `String argv[8]` + `splitArgs()` with an unused `rest` out-parameter | the grammar needs verb, key, value; 8 heap-allocated `String`s per command line to produce 2 | ~25 lines |
| 6-layer rule with a sub-folder ordering map | 90 lines of PowerShell to police 42 files | — |

`Snapshot.h` and `SnapshotBuilder` are **not** on this list: they stay, and §3 argues why.

## 2. What the radical version looks like

```
core/     Config, Math, Format, Log(+cpp), Snapshot            shared foundations
hal/      Pins, Display, PressureSensor, Pump, VentValve, Battery, + .cpp each
app/      App, VacuumControl, SnapshotBuilder, SerialLink, StatusPage
```

* `core/` and `hal/` keep their meaning: **`hal/` is the base hardware abstraction, and
  that is the delineation that has to survive.** What goes is the middle: `services/`,
  `hosts/` and `ui/` are all "the instrument", and they stop being three folders with a
  graph between them.
* `app/` holds the instrument's five collaborators: the control loop, the snapshot
  builder, the serial interface (bytes, grammar and payload text — §4), the status page
  and `App` that wires them. Sixteen files spread over four layers that each may include a
  subset of the others become five units in one folder.
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

## 3. The snapshot stays; its encoder moves

Q7 as amended: `core/Snapshot.h` and the builder that fills it are **kept**. The snapshot
is the seam that keeps whoever reads the machine out of the modules that produce it, and
the collapse in §2 does not remove the need for it — the page and the serial interface
both want the machine as plain data at one instant, rather than each holding four driver
references to read six numbers.

Two things about it change, and they are the real findings:

1. **The encoder moves out of `services/`.** `SnapshotText` rendered a transport's payload
   format from a service folder: a transport concern wearing a service's clothes. The
   `key=value` payload text folds into the serial interface (§4), so the format lives with
   the protocol that defines it, and adding a key becomes one edit in one file.
2. **No by-value copy.** `buildSnapshot()` gains an out-parameter and fills
   `App::snapshot_` in place, instead of returning ~100 bytes for `App` to copy.

It also gains an argument: `snapshot.device` goes with `hal/Device` (Q10), so the two
`ESP.*` facts the `info` payload needs are read where they are formatted.

`DECISION:` keep the mechanism, fix the two things that were wrong with it. The snapshot
is still rebuilt once per loop before anything reads it, so the page and a reply describe
the same instant — the consistency the *delete it* variant would have given up for no
gain.

## 4. The transport and the grammar

| Concern | Decision |
| --- | --- |
| Port | **USB CDC**, reached through a `Stream&` seam; FQBN gains `CDCOnBoot=cdc`, pinned in `sketch.yaml`, `scripts/arduino.ps1` and `.vscode/arduino.json` |
| Why not UART0 | `esp32:esp32:esp32c3` ships `CDCOnBoot=Disabled`, so `Serial` is `Serial0` = UART0 on GPIO20/21, which is **not** the USB-C socket: today's console output never reaches a USB cable. A UART also cannot detect a host (`HardwareSerial::operator bool()` is `uartIsDriverInstalled()`, true forever), while `HWCDC`'s is real presence (`isCDC_Connected()`) |
| Framing | none above the line: newline terminated, `Stream&`, bounded line buffer, `\r` or `\n` terminates, `COMMAND_LINE_MAX = 128` |
| Replies | the serial interface writes finished lines to its `Stream&`; `ReplyQueue` and the whole producer/consumer dance go |
| Grammar and text | one file pair, `app/SerialLink.{h,cpp}`, holds line assembly, the command grammar **and** the `key=value` payload text. On a wire the transport half is `available()`/`read()`/`print()` — three calls — so the "thin transport, separate grammar" split that existed for a radio with tasks, MTUs and callbacks buys nothing here, while the *protocol* half (commands, payloads, error codes) is one coherent thing. `CommandRouter.{h,cpp}` and `SnapshotText.{h,cpp}` are deleted by the merge; the two halves stay sectioned in the file |
| Log | shares the stream under its existing `[I]` prefix: lines starting with `[` are log records, `ok `/`err ` are replies. `set log <on\|off>` and `log=on\|off` in `info` let a controller insist on a clean channel |
| Link loss | a dropped USB CDC link stops the run and vents (mirrors Q4/Q10); the silent-link watchdog is dropped, because a cable that is present and quiet is the normal case |
| Parsing | in-place three-token parse (`verb [key] [value]`) instead of `String argv[8]` and an unused `rest` |

## 5. Protocol v3

Grammar, status keys, info keys, the five error codes and the state model survive
verbatim. Four changes:

1. `proto=3`, and `docs/serial-protocol.md` documents **only** the serial protocol. No
   Bluetooth vocabulary survives anywhere in the deliverables: not in the protocol doc,
   not in the README, not as a "replaced the radio" note. The removed transport is a git
   history question, not a documentation question (Q16).
2. **Telemetry is polled, not pushed** — the subscription model was the radio's way of
   asking "does anyone want this", so `TelemetrySettings`, `set period` and the push path
   go. A controller that wants 4 Hz sends 4 `status` a second, which is nothing at
   115200, and the device's behaviour stays a pure function of the commands it received.
3. `set log <on|off>` and `log=on|off` in `info` (Q3).
4. No `proto=2` migration table. There is no released client to migrate, and writing one
   would mean describing the transport being removed, which Q16 rules out.

`reset` also changes meaning slightly, as a consequence of the dead-surface sweep: with
the sensor error counter gone (nothing read it) it is the pump latch and the controller
fault, nothing else.

The two payload renderers (`status`, `info`) are functions in `SerialLink` (§3, §4), so a
new key is one edit in the one file that defines the format. Both render from the
snapshot, which is why the payload cannot disagree with what the display is showing.

The display is **unchanged**: Q15 says no link token, so the state row stays as it is and
the layout in the README does not move.

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
| 1 | Serial transport | `SerialLink` over `Stream&`, delete `hosts/ble` and `core/ReplyQueue.h`, `CommandRouter` keeps the grammar but writes to the stream, `SnapshotText` untouched for now, `proto=3`, delete `TelemetrySettings`/`set period`/`LINK_TIMEOUT_MS`/`BLE_*`/`TELEMETRY_*`, add `set log` and `log=` in `info`, FQBN in three files, `libs` drops NimBLE, `check-layers.ps1` `ble` → `serial` | green, layer rule ok, **flash and RAM measured** to price the radio |
| 2 | Dead surface sweep | the inventory in §1, minus the structural items: `Pressure.h`, `Tachometer` (totally, Q9), `Device` (the `info` payload reads `ESP.*` where it formats), the unconsumed accessors, the four `*Config` structs and `App::*Config()`, button timings, `Pid` folded into `VacuumControl`, three-token parsing | green |
| 3 | Collapse, and fold the encoder into the serial interface | move to `core/`+`hal/`+`app/`, merge the grammar and the payload text into `app/SerialLink.{h,cpp}`, delete `CommandRouter`/`SnapshotText`, keep `Snapshot.h` + `app/SnapshotBuilder`, fill the snapshot in place, `Renderer`+`Pages.h`+`StatusPage` → one `StatusPage`, `App` slims, `check-layers.ps1` rewritten small | green, layer rule ok, **no value changed**: byte-identical payloads and an unchanged display, checked by reading the new `SerialLink` against the old `SnapshotText` |
| 4 | Docs and tools | `docs/ble-protocol.md` deleted, `docs/serial-protocol.md` written standalone (Q16: no transport history in it), README rewritten for three layers with no snapshot and no transport mentions, `tools/vacuum_cli.py` on pyserial, `requirements.txt`, `FIRMWARE_VERSION` → `0.2.0`, this plan deleted | green, `python -m py_compile` |

## 8. Risks

| id | Risk | Mitigation |
| --- | --- | --- |
| R1 | the tethered instrument; nothing wireless can drive it | accepted. The line grammar is unchanged, so a bridge to any other link later is a transport, not a redesign |
| R2 | a controller that dies mid-run leaves a sealed chamber | unchanged device protections: timer expiry, `PUMP_MAX_RUN_MS`, fault latch, and a valve that fails open |
| R3 | building without `CDCOnBoot=cdc` sends the console to GPIO20/21 and looks dead | FQBN pinned in three files, README states the failure mode, boot banner goes to the stream in use |
| R4 | opening the port from the host may reset the ESP32-C3 (USB-Serial-JTAG reset sequences) | verify on hardware; boot-safe means an aborted run and a vented chamber, so document it either way |
| R5 | the log corrupts a parsing controller's replies | documented `[` rule, `set log off`, and no unmarked line is ever emitted |
| R6 | folding the grammar and the payload text into one file makes "transport" and "protocol" one unit | accepted: the byte half is three calls, the two halves are sectioned, and keeping the snapshot (Q7) means a second transport still has a seam for the machine view |
| R7 | losing the `*Config` structs removes the DI seam for a board variant | `hal/Pins.h` becomes that seam explicitly, which is where a board variant edits today anyway |
| R8 | deleting a driver for hardware that is not fitted looks like losing capability | the tach is a revert of one commit when a wire is fitted, exactly like the Berry removal; say so in the commit |
| R9 | phase 3 is large and could hide a behaviour change in a reshuffle | keep it behaviour-free by contract, and keep the wire-visible changes in phases 1 and 2 |

## 9. Decisions

Answered on 2026-10-15: **Q1–Q14 yes, Q15 no, Q16 with the "drop the mentions" amendment,
Q17 confirmed.** Every answer is folded into the sections above; this table is the record.

| # | Question | Answer |
| --- | --- | --- |
| Q1 | Port and seam: USB CDC via `CDCOnBoot=cdc`, reached through a `Stream&`? | **yes** |
| Q2 | Telemetry: poll `status` only, deleting `TelemetrySettings` and `set period`? | **yes** |
| Q3 | Log: shares the stream under the `[` rule, plus `set log <on\|off>` and `log=` in `info`? | **yes** |
| Q4 | Link: dropped USB CDC link stops and vents; silent-link watchdog dropped? | **yes** |
| Q5 | Parsing: `\r` or `\n` terminates; 128-byte line cap? | **yes** |
| Q6 | Layers: collapse `services/`, `hosts/`, `ui/` into `app/`, leaving `core/`, `hal/`, `app/`? | **yes** |
| Q7 | The snapshot: delete `Snapshot.h`, `SnapshotBuilder`, `SnapshotText` and let the page and the router read modules? | **amended**: keep `Snapshot.h` and `SnapshotBuilder`, **fold the payload text into the serial interface**, and drop the by-value return. `SnapshotText` is deleted as a file, its format lives with the protocol (§3, §4) |
| Q8 | `Pid`: fold into `VacuumControl`? | **yes** |
| Q9 | Tachometer | **remove it totally**: driver, ISR, pin, three constants, config fields, the accessors on `Pump`, the `tach=` key and `rpm` |
| Q10 | `hal/Device`: delete it and let the `info` payload read `ESP.getChipModel()`/`getFreeHeap()` where it formats? | **yes** |
| Q11 | The four `*Config` structs and `App::*Config()` helpers: delete, with `hal/` reading `pins::`/`cfg::` directly? | **yes** (R7) |
| Q12 | The dead accessors in §1, plus `Pump::resetFault()` becoming `void`, `PressureSensor::readNow()` becoming private, and `reset` losing its sensor-counter line? | **yes** |
| Q13 | UI: `Renderer` + `pages/Pages.h` + `StatusPage` → one `app/StatusPage.{h,cpp}`? | **yes** |
| Q14 | `scripts/arduino.ps1`: drop `-Task link`, keep build/upload/monitor/libs/clean? | **yes** |
| Q15 | Display: add a `USB` token on the state row? | **no** — the layout does not move |
| Q16 | Versions and tools | **yes**, plus: **drop the mentions entirely** — `docs/ble-protocol.md` is deleted rather than superseded, `docs/serial-protocol.md` describes only the serial protocol, and no deliverable (README, doc, script, source comment) refers to the removed transport |
| Q17 | Button pins: keep the four pin numbers with a "nothing reads them" comment? | **yes** (Q16 of the earlier plan stands) |

## 10. Status

| Revision | Change | Sections |
| --- | --- | --- |
| rev 1 | transport swap to serial, polling telemetry, log sharing, link policy, five phases | all |
| rev 2 | adds the cruft inventory, the three-layer collapse, the snapshot deletion, the dead-surface sweep, `0.2.0`/`proto=3`, renumbers the questions | §1–§3, §7–§9 |
| rev 3 | answers folded in: Q1–Q14 yes, Q15 no (display unchanged), Q16 with "drop the mentions entirely" (no transport vocabulary in any deliverable, no migration table), Q17 confirmed; Q9 strengthened to "remove it totally" | §5, §7, §9 |
| rev 4 | Q7 amended: the snapshot and its builder are **kept**, and only the payload encoder folds into the serial interface. The grammar and the payload text merge into one `app/SerialLink.{h,cpp}`; the by-value snapshot copy is fixed instead of deleted; file count 42 → 30 rather than 29 | §1–§4, §7–§9 |
