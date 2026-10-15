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
| `core/Log.{h,cpp}` | the `LogSink` function pointer, `logBegin(sink)`, and `vsnprintf` + `snprintf` behind the `LOG*` macros: an abstraction whose only reason for existing was formatting. Removed rather than shrunk (§4) | 2 files, 60 lines |
| `core/Format.h`, and every `snprintf` in our code | `formatFloat` needs `dtostrf`; the payload renderer, the replies and the page all format with `snprintf`. Our code gets no stdio at all (§4) | ~15 call sites |
| `String` in `VacuumCore` | `WString`'s float and number paths go through `dtostrf`/`strtod`, and `splitArgs` alone allocates 8 `String`s per command line to produce 2 | ~40 uses |
| 4 `*Config` structs and 4 `App::*Config()` helpers | every caller passes the defaults; the only real variation is the pin number, which `hal/Pins.h` already owns | ~60 lines |
| `services/SnapshotText.{h,cpp}` | the wire payload encoder living in `services/`, so a service encodes a transport's format. The two payloads it builds belong with the protocol that defines them | 2 files |
| `buildSnapshot()` returning `SystemSnapshot` **by value** | a struct copy back into `App::snapshot_` every loop iteration, for nothing | 1 signature |
| `ui/Renderer.{h,cpp}`, `ui/pages/Pages.h`, `ui/pages/StatusPage.cpp` | three levels of indirection for one screen | 4 files |
| 6-layer rule with a sub-folder ordering map | 90 lines of PowerShell to police 42 files | — |

`Snapshot.h` and `SnapshotBuilder` are **not** on this list: they stay, and §3 argues why.

## 2. What the radical version looks like

```
core/     Config, Math, Text, Snapshot                          shared foundations
hal/      Pins, Display, PressureSensor, Pump, VentValve, Battery, + .cpp each
app/      App, VacuumControl, SnapshotBuilder, SerialLink, StatusPage
```

* `core/` and `hal/` keep their meaning: **`hal/` is the base hardware abstraction, and
  that is the delineation that has to survive.** What goes is the middle: `services/`,
  `hosts/` and `ui/` are all "the instrument", and they stop being three folders with a
  graph between them.
* `app/` holds the instrument's five collaborators: the control loop, the snapshot
  builder, the serial interface (bytes, grammar and payload text — §5), the status page
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
   `key=value` payload text folds into the serial interface (§5), so the format lives with
   the protocol that defines it, and adding a key becomes one edit in one file.
2. **No by-value copy.** `buildSnapshot()` gains an out-parameter and fills
   `App::snapshot_` in place, instead of returning ~100 bytes for `App` to copy.

It also gains an argument: `snapshot.device` goes with `hal/Device` (Q10), so the two
`ESP.*` facts the `info` payload needs are read where they are formatted.

`DECISION:` keep the mechanism, fix the two things that were wrong with it. The snapshot
is still rebuilt once per loop before anything reads it, so the page and a reply describe
the same instant — the consistency the *delete it* variant would have given up for no
gain.

## 4. Text and diagnostics with no stdio

The log is the smallest user of stdio, so removing only the log would leave the claim
half true. Our code stops including `<stdio.h>` entirely:

| Deleted | Replaced by |
| --- | --- |
| `core/Log.{h,cpp}`: `LogSink`, `logBegin(sink)`, `vsnprintf`, format strings, the `[I]` builder | `app/SerialLink` owning the stream and writing `[I] text\n` lines; a `log::enabled(bool)` switch |
| `core/Format.h` (`dtostrf` → `String`) | `core/Text.h` |
| `snprintf` in the payload renderer, the replies and the page | the same `core/Text.h` helpers |
| `String` in `VacuumCore` (parsing, replies, buffer building) | `char` buffers and in-place tokenizing |
| `strtof`/`strtoul` in the grammar | hand-rolled `parseFloat`/`parseU32` over the token |

`core/Text.h` is the whole replacement, and it only does what the firmware actually
prints:

```cpp
size_t unum(char* out, size_t size, uint32_t value);               // 1234
size_t unum(char* out, size_t size, uint32_t value, uint8_t width); // 0007, 09:32
size_t hex8(char* out, size_t size, uint8_t value);                // 0x3C, 0x18
size_t fixed(char* out, size_t size, float value, uint8_t decimals); // 41.3, 101.33
```

Rounding has to match what `%.1f`/`%.2f` produced, because the payload is a contract:
half away from zero, and `-0.0` renders as `0.0`.

**The drivers stop narrating.** With no log abstraction there is nothing for `hal/` to
log *to* — and that is the right outcome rather than a sacrifice: `hal/` may not include
`app/`, so a driver writing diagnostics was reaching past its own layer anyway. The
information that matters already comes back as values (`Present()`, `tripped()`,
`faultCode()`, the `begin()` return), and the composition root reports it:

* `App` logs the boot banner, what failed to come up, and every state transition
  (`IDLE → PULL → HOLD → VENT`), the fault code, a vent timeout and a lost link
* one place decides what is worth saying, instead of ~15 scattered one-liners, and it is
  the only place that can see the whole machine

Consequence worth naming: the bench output text changes. That is not a protocol surface
(`ok `/`err ` lines and the `[` rule are untouched), so nothing on the wire moves — but
the messages a person sees on a terminal are new, and only the useful ones survive.

## 5. The transport and the grammar

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

## 6. Protocol v3

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

The two payload renderers (`status`, `info`) are functions in `SerialLink` (§3, §5), so a
new key is one edit in the one file that defines the format. Both render from the
snapshot, which is why the payload cannot disagree with what the display is showing.

The display is **unchanged**: Q15 says no link token, so the state row stays as it is and
the layout in the README does not move.

## 7. What must not change

The instrument. `hal/`'s behaviour is identical after the sweep — the pump still ramps,
caps duty and trips on `PUMP_MAX_RUN_MS`; the MPRLS still does median + EMA with an
ambient reference; the valve still energises to close and fails open; the battery still
averages trimmed millivolts; `VacuumControl` still owns the run model, the countdown
start, the vent margin and the fault latch. This is a deletion and a reshuffle, not a
redesign of the machine.

## 8. Phases

Each phase builds green, passes `scripts/check-layers.ps1`, and is its own commit.
Phases 1–3 change behaviour; phase 4 must not, so a regression has somewhere to hide.

| # | Phase | Content | Verify |
| --- | --- | --- | --- |
| 1 | Serial transport | `SerialService` over `Stream&`, delete `hosts/ble` and `core/ReplyQueue.h`, replies written to the stream, `proto=3`, delete `TelemetrySettings`/`set period`/`LINK_TIMEOUT_MS`/`BLE_*`/`TELEMETRY_*`, add `set log` and `log=` in `info`, FQBN in three files, `libs` drops NimBLE, `check-layers.ps1` `ble` → `serial` | green, layer rule ok |
| 2 | No stdio | delete `core/Log.{h,cpp}` and `core/Format.h`, add `core/Text.h`, remove `String` from `VacuumCore` (in-place tokenizing, `char` buffers, hand-rolled `parseFloat`/`parseU32`), payload text and the page format with `text::`, drivers stop narrating, `App` owns diagnostics and the serial interface writes them | green, and `grep -rn "stdio\|snprintf\|vsnprintf\|strtof\|strtoul\|String" src` finds nothing in `VacuumCore` |
| 3 | Dead surface sweep | the inventory in §1 minus the structural items: `Pressure.h`, `Tachometer` totally (Q9), `Device`, the unconsumed accessors, the four `*Config` structs and `App::*Config()`, button timings, `Pid` folded into `VacuumControl` | green |
| 4 | Collapse, and fold the encoder into the serial interface | move to `core/`+`hal/`+`app/`, merge the grammar and the payload text into `app/SerialLink.{h,cpp}`, delete `SnapshotText`, keep `Snapshot.h` + `app/SnapshotBuilder`, fill the snapshot in place, `Renderer`+`Pages.h`+`StatusPage` → one `StatusPage`, `App` slims, `check-layers.ps1` rewritten small | green, layer rule ok, **no value changed**: byte-identical payloads and an unchanged display |
| 5 | Docs and tools | `docs/ble-protocol.md` deleted, `docs/serial-protocol.md` written standalone (Q16: no transport history in it), README rewritten for three layers with no snapshot and no transport mentions, `tools/vacuum_cli.py` on pyserial, `requirements.txt`, `FIRMWARE_VERSION` → `0.2.0`, this plan deleted | green, `python -m py_compile` |

Phase 2's ordering is deliberate: it runs after the transport swap, so by then `ReplyQueue`
and the NimBLE task are gone and "no `String`, no stdio" is a clean property of the whole
library rather than a claim with exceptions.

## 9. Risks

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
| R9 | phase 4 is large and could hide a behaviour change in a reshuffle | keep it behaviour-free by contract, and keep the wire-visible changes in phases 1–3 |
| R10 | hand-rolled number text must render exactly what `%.1f`/`%.2f` rendered, or the payload contract changes silently | `core/Text.h` rounds half away from zero and prints `-0.0` as `0.0`; phase 2 checks a value table (0, 0.04, 0.05, 9.95, 41.25, 101.325, 172.369, negative, large) against the old output before deleting `Format.h` |
| R11 | the drivers stop narrating, so bench output loses messages | intentional: `hal/` was reaching past its layer to log. `App` logs what a person needs — boot, what failed to come up, state transitions, fault code, vent timeout, link loss — and the messages change, which is not a protocol surface |

## 10. Decisions

Answered on 2026-10-15: **Q1–Q14 yes, Q15 no, Q16 with the "drop the mentions" amendment,
Q17 confirmed.** Every answer is folded into the sections above; this table is the record.

| # | Question | Answer |
| --- | --- | --- |
| Q1 | Port and seam: USB CDC via `CDCOnBoot=cdc`, reached through a `Stream&`? | **yes** |
| Q2 | Telemetry: poll `status` only, deleting `TelemetrySettings` and `set period`? | **yes** |
| Q3 | Log: shares the stream under the `[` rule, plus `set log <on\|off>` and `log=` in `info`? | **yes** — and with §4 the flag lives in the serial interface, which is the only thing it can do itself: it silences its own diagnostics, everything else goes to the grammar |
| Q4 | Link: dropped USB CDC link stops and vents; silent-link watchdog dropped? | **yes** |
| Q5 | Parsing: `\r` or `\n` terminates; 128-byte line cap? | **yes** |
| Q6 | Layers: collapse `services/`, `hosts/`, `ui/` into `app/`, leaving `core/`, `hal/`, `app/`? | **yes** |
| Q7 | The snapshot: delete `Snapshot.h`, `SnapshotBuilder`, `SnapshotText` and let the page and the router read modules? | **amended**: keep `Snapshot.h` and `SnapshotBuilder`, **fold the payload text into the serial interface**, and drop the by-value return. `SnapshotText` is deleted as a file, its format lives with the protocol (§3, §5) |
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
| Q18 | `core/Log` and the stdio it needs | **remove both**: `core/Log.{h,cpp}` and `core/Format.h` are deleted, `core/Text.h` hand-rolls every number the firmware prints, `String` leaves `VacuumCore`, the payloads, replies, page and parser all use `char` buffers, and `hal/` stops narrating so `App` owns diagnostics (§4) |

## 11. Status

| Revision | Change | Sections |
| --- | --- | --- |
| rev 1 | transport swap to serial, polling telemetry, log sharing, link policy, five phases | all |
| rev 2 | adds the cruft inventory, the three-layer collapse, the snapshot deletion, the dead-surface sweep, `0.2.0`/`proto=3`, renumbers the questions | §1–§3, §8–§10 (as numbered then) |
| rev 3 | answers folded in: Q1–Q14 yes, Q15 no (display unchanged), Q16 with "drop the mentions entirely" (no transport vocabulary in any deliverable, no migration table), Q17 confirmed; Q9 strengthened to "remove it totally" | §6, §8, §10 |
| rev 4 | Q7 amended: the snapshot and its builder are **kept**, and only the payload encoder folds into the serial interface. The grammar and the payload text merge into one `app/SerialLink.{h,cpp}`; the by-value snapshot copy is fixed instead of deleted; file count 42 → 30 rather than 29 | §1–§5, §8–§10 |
| rev 5 | Q18: no stdio at all — `core/Log` and `core/Format` deleted, `core/Text.h` hand-rolls the number text, `String` leaves `VacuumCore`, `hal/` stops narrating and `App` owns diagnostics. Five phases, with the no-stdio sweep after the transport swap so it is a whole-library property | new §4; §1, §3, §6, §8, §9, §10 |
