# Plan: replace Bluetooth with a serial controller interface

The device is a BLE peripheral whose entire interface is one line grammar. That grammar
never depended on Bluetooth: `CommandRouter` turns text into action, `SnapshotText` turns
state into text, and `BleService` only moves bytes. Dropping the radio puts the same
instrument behind a cable, removes the largest dependency in the build, and deletes the
parts of the design that only existed because a radio connection is not a wire.

## 1. What actually changes

| Concern | Now | After |
| --- | --- | --- |
| Transport | NimBLE GATT server, 4 characteristics, MTU negotiation, notification chunking | one serial stream, newline terminated lines, no framing above the line |
| Reply path | router fills `ReplyQueue`, the BLE task notifies | router writes finished lines straight to the stream on the main loop |
| Inbound path | NimBLE callback parks lines in `ReplyQueue` for `poll()` | `poll()` reads the stream and dispatches inline |
| Telemetry | `state` characteristic, CCCD subscription is the enable | answers a question that a wire makes silly — see §5 |
| Link loss | disconnect or silence stops the run and vents (Q4) | USB CDC drop stops the run and vents; silence stops being detectable and stops mattering (§6) |
| Log | second stream on UART0, invisible on USB | same stream as the protocol, marked (§4.3) |
| Dependency | NimBLE-Arduino 2.5.1 | none: `Serial`/`Print` are in the core |

Nothing about the instrument changes: pressure sensing, the PID hold, the run model, the
valve, the battery gauge, the display and the safety limits are untouched.

## 2. What goes

| Removed | Because |
| --- | --- |
| `hosts/ble/BleService.{h,cpp}` | the whole radio transport: GATT service, four characteristics, CCCD subscribe callbacks, pair/bonding TODOs, `MTU` handling, 0x01/0x02 notification chunking, link-loss detection |
| `core/ReplyQueue.h` | it exists to hand bytes between the NimBLE task and the main loop. On a wire there is one thread and one loop, so both directions collapse |
| `TelemetrySettings` in `core/Snapshot.h` | there is no subscription to mirror (§5) |
| `set period` in `hosts/commands/CommandRouter` | same |
| `cfg::BLE_NAME_PREFIX`, `cfg::BLE_MTU` | no radio |
| `cfg::TELEMETRY_DEFAULT_MS`, `cfg::TELEMETRY_MIN_MS` | same |
| `NimBLE-Arduino` from `scripts/arduino.ps1 -Task libs` | nothing includes it |
| `bleak` in `tools/requirements.txt` | replaced by `pyserial` |
| `docs/ble-protocol.md` | replaced by `docs/serial-protocol.md` (§8) |

`core/` shrinks to `Config`, `Snapshot`, `Math`, `Pressure`, `Format`, `Log`, `Pid` —
seven files, all pure logic, no transport concept anywhere in it. That is the point: the
layer that was always transport-free stops having a transport-shaped hole in it.

## 3. Shape after

```
core/      Config, Snapshot, Math, Pressure, Format, Log, Pid
hal/       Pins, Device, Battery, Pump, VentValve, Tachometer, PressureSensor, Display
services/  VacuumControl, SnapshotBuilder, SnapshotText
hosts/     commands/CommandRouter, serial/SerialService
ui/        Renderer, pages/StatusPage.cpp
app/       App
```

`hosts/serial/SerialService` is the only file that knows a stream exists:

```cpp
class SerialService {
 public:
  SerialService(CommandRouter& router, Stream& stream, const SystemSnapshot& snapshot);

  void begin();
  void poll(uint32_t nowMs);
  bool controllerAttached() const;   // USB CDC presence; false for a plain UART
  bool consumeLinkLost();            // one shot, App decides what it means
  void configure(const SerialSettings& settings);   // log on/off, echo

 private:
  void drainLines();
  void handleLine(const String& line);
};
```

It takes a `Stream&` rather than reaching for `Serial`, which is what makes §4.1 a
constant instead of a rewrite. It holds no policy: it assembles lines, hands them to the
router, and reports the link. App still decides what a lost link means.

## 4. The transport

### 4.1 Which port, and the one build flag that decides it

`esp32:esp32:esp32c3` ships `CDCOnBoot=Disabled`, and in that configuration
`ARDUINO_USB_CDC_ON_BOOT=0`, so the core defines `Serial` as `Serial0` — **UART0 on
GPIO20/21**, which on an ESP32-C3 Super Mini is the header pads, *not* the USB-C
connector. That is worth stating plainly because it is the current state of the firmware:
the log lines the firmware writes today do not reach a USB-C cable, they only reach a
USB-serial adapter wired to GPIO20/21.

So there are three coherent choices, and the plan recommends the third:

| Option | FQBN | Cable to the controller | Notes |
| --- | --- | --- | --- |
| A. UART0 | `esp32:esp32:esp32c3` (unchanged) | USB-serial adapter on GPIO20/21 | keeps native USB free for flashing, needs an adapter, no presence detection |
| B. USB CDC | `esp32:esp32:esp32c3:CDCOnBoot=cdc` | the USB-C cable | `Serial` becomes `HWCDCSerial`; one cable for upload, control and log; `controllerAttached()` works |
| C. both, one constant | either | either | `App` passes `Serial` or `Serial0`/`Serial1` to `SerialService`; `HWCDCSerial` is always available (`extern HWCDC HWCDCSerial;`) even with CDC off boot |

`DECISION:` default to **B, reached through C** — `Serial` is the default stream, the FQBN
in `sketch.yaml` and `scripts/arduino.ps1` gains `CDCOnBoot=cdc`, and switching to a real
UART is a one-line change in `App` plus a rebuild. Baud stops being meaningful there (USB
CDC ignores it), which is why `SERIAL_BAUD` is only used when the stream is a UART.

**Risk of getting this wrong is silent**: building without the menu option sends
everything to UART0 and the controller sees nothing. The mitigation is to pin the FQBN in
three places (`firmware/eggplan_vacuum/sketch.yaml`, the `-Fqbn` default in
`scripts/arduino.ps1`, `.vscode/arduino.json`) and to write the boot banner to the stream
the firmware is actually using, so a wrong-FQBN build is obviously quiet rather than
mysteriously quiet.

### 4.2 Line handling

```
command  = verb [ SP argument ] LF            ; \r ignored, same grammar as v2
```

* assemble bytes into a bounded line buffer (`COMMAND_LINE_MAX`), dispatch on `\n`
* one command in flight at a time is not required, but replies keep arriving in order
* a command line over the limit is dropped and answered `err bad_argument`, rather than
  silently truncated
* no framing layer: no opcodes, no lengths, no checksum. At 115200 over USB a line is
  already atomic enough, and the cost of a corrupted byte is one `err unknown_command`

### 4.3 The log shares the stream

The log sink currently writes `[I] message\n` to the same port. Two streams was a luxury
of having a radio; on a wire the options are:

1. **share the stream, and say so**: lines starting with `[` are log records, lines
   starting with `ok ` / `err ` are replies. The prefix already exists, so this costs
   nothing to implement and one line to document. A controller ignores anything that is
   not a reply; a human reads everything.
2. log off unless asked (`set log <on|off>`), so a parsing controller gets a clean channel
3. log on a second UART while control stays on USB

`DECISION:` 1 + 2 together: share the stream, document the `[` rule, and add
`set log <on|off>` so a controller that wants a strictly quiet channel can have one.
Default is on, because a bench instrument's log is the reason to plug it in.

## 5. Protocol v3

The grammar, the status payload, the info payload, the command table, the five error
codes and the state model all survive verbatim. Three things change:

1. **`proto=3`** in `info`, because the transport changed and the telemetry model changed.
2. **Telemetry is polled, not pushed.** The v2 design hung telemetry off a CCCD
   subscription, which was the radio's way of answering "does anyone want this". A wire
   has no such question: a client that wants state asks `status`. That deletes
   `TelemetrySettings`, `set period`, the `state` characteristic, and the reason a client
   had to filter anything out of a reply stream. Cost: a controller that wants a 4 Hz
   readout sends 4 commands a second, which is nothing at 115200 and keeps the device's
   behaviour a pure function of the commands it received.
3. **`set log <on|off>`** joins the `set` family, and `logs` (or the log state) appears in
   `info` as `log=on|off` so a controller can tell whether it must tolerate `[` lines.

If Q2 says keep push telemetry, the fallback is `set period <ms>` with `off` disabling it
and no default-on behaviour — but then the device talks without being asked, and a
controller has to filter, which is exactly the shape v2 was criticised for.

## 6. Link policy

| Event | v2 | v3 |
| --- | --- | --- |
| controller detaches (USB CDC drop) | disconnect → stop and vent (Q4) | same: stop and vent |
| controller present but silent | `LINK_TIMEOUT_MS`, default off (Q17) | **dropped**: a cable that is present and quiet is the normal case (an operator watching a 60 minute hold), and without a radio there is no other reason to guess |
| cable cut mid-run on battery power | n/a | the device keeps its own safety: timer expiry, `PUMP_MAX_RUN_MS`, fault latch, and a valve that fails open |

`controllerAttached()` is transport-dependent, and the two core classes differ in exactly
the way that matters:

| Stream | `operator bool()` | Presence detection |
| --- | --- | --- |
| `HWCDC` (USB CDC, option B) | `isCDC_Connected()` | yes: real host presence, so a drop is observable |
| `HardwareSerial` (UART, option A) | `uartIsDriverInstalled()` | no: true forever once started, so a drop is invisible |

Both derive from `Stream`, so the seam accepts either. When the stream cannot report
presence, `consumeLinkLost()` simply never fires — which is fine, because the plan drops
the silent-link policy anyway. The conservative behaviour (stop and vent when the
controller disappears) is preserved for the cable case, so the safety story does not
regress with the radio.

## 7. What does not change

* `hal/` entirely: pump, valve, battery, MPRLS, SH1106, `Tachometer` (still optional and
  disabled), `Pins.h` except for comments about which GPIOs carry the console
* `services/` entirely: `VacuumControl` (run model, timer, vent sequencing, fault latch),
  `SnapshotBuilder`, `SnapshotText` except `proto=3`
* `ui/` entirely unless Q9 says the display should show a link token
* `app/App` structure: composition root, same member order, one transport swapped

## 8. Phases

Each phase builds green, passes `scripts/check-layers.ps1`, and is its own commit.

| # | Phase | Content | Verify |
| --- | --- | --- | --- |
| 1 | Transport swap | `hosts/serial/SerialService`, delete `hosts/ble` and `core/ReplyQueue.h`, router writes to a `Print&`, `App` wiring, FQBN in `sketch.yaml`/`arduino.ps1`/`.vscode`, `libs` drops NimBLE, `check-layers.ps1` `ble` → `serial` | green, layer rule ok, flash and RAM measured to show the radio's real cost |
| 2 | Wire and policy | `proto=3`, delete `TelemetrySettings` and `set period`, add `set log`/`log` in `info`, link-loss through `SerialService`, drop `LINK_TIMEOUT_MS` | green |
| 3 | Docs and tools | `docs/serial-protocol.md` (grammar, line rules, key tables, command table, error codes, state model, v2→v3 migration, wiring and FQBN), delete `docs/ble-protocol.md`, README rewrite, `tools/vacuum_cli.py` on pyserial, `requirements.txt` | green, `python -m py_compile`, docs match code |

Hardware checks the user has to run (no board here): flash with `CDCOnBoot=cdc`, attach
the CLI, confirm the port appears and the boot banner arrives, confirm a run started over
the wire survives 60 minutes of silence, and confirm whether opening the port resets the
board (R5).

## 9. Risks

| id | Risk | Mitigation |
| --- | --- | --- |
| R1 | losing wireless control: the instrument is now tethered, and a phone cannot drive it | accepted. The wire format is unchanged, so a wireless bridge later is a transport, not a redesign: the same line grammar over BLE-UART, ESP-NOW, WiFi telnet or a second MCU |
| R2 | the log corrupts a parsing controller's reply stream | the `[` prefix rule is documented and `set log off` exists; the device never emits an unmarked line |
| R3 | building without `CDCOnBoot=cdc` silently sends the console to GPIO20/21 | FQBN pinned in three files; the boot banner goes to the stream in use; README states the failure mode |
| R4 | a 60 minute hold with nobody watching the cable | unchanged and already handled by the device: timer expiry, `PUMP_MAX_RUN_MS`, fault latch, valve fails open |
| R5 | opening the port from the host resets the ESP32-C3 (USB-Serial-JTAG reset sequences are a real behaviour on this part) | verify on hardware first; if it resets, boot-safe means the run aborts and the chamber vents, which is safe but surprising — document it, or keep `controllerAttached()` transitions out of the safety path |
| R6 | a controller that dies mid-run leaves a sealed chamber | recovered by the timer or by `PUMP_MAX_RUN_MS`; the vent valve is only closed while a run is active |
| R7 | deleting `ReplyQueue` looks like losing a safety property (the queue bounded memory) | it existed for cross-task hand-off. `SerialService` keeps a bounded line buffer and replies are written inline, so there is still no unbounded growth; say so in the commit |

## 10. Questions

Recommendations are marked, so "all as recommended" is a valid answer.

| # | Question | Recommendation |
| --- | --- | --- |
| Q1 | Which port carries the control interface? | **USB CDC via `CDCOnBoot=cdc`, reached through a `Stream&` seam** so UART0 stays one line away |
| Q2 | Telemetry: keep pushing it, or make the controller poll `status`? | **poll only**; delete `TelemetrySettings`, `set period` and the push path |
| Q3 | The log shares the protocol stream? | **yes**, with the documented `[` rule, **plus `set log <on\|off>`**, default on |
| Q4 | Does a dropped USB CDC link stop the run and vent? | **yes**, mirroring Q4/Q10: the controller leaving has to release the chamber |
| Q5 | Keep a silent-link watchdog? | **drop it**; the radio made it cheap, a wire makes it nonsense |
| Q6 | Add `set log <on\|off>`, and `log=on\|off` to `info`? | **yes to both** — a controller needs to know what to expect on the wire |
| Q7 | `SERIAL_BAUD` and the maximum command line? | **115200** (only used for a real UART; USB CDC ignores it) and **128 bytes** (the 512 existed for program transfer, which is gone) |
| Q8 | Bump to `proto=3` and replace `docs/ble-protocol.md` with `docs/serial-protocol.md`? | **yes**; keep the v2 migration table and add a v2→v3 one |
| Q9 | Should the display show that a controller is attached (a small `USB` token on the state row)? | **yes, small**: the display is a readout, and "is anyone listening" is the one thing a readout cannot otherwise tell you |
| Q10 | Keep `tools/vacuum_cli.py`, rebuilt on pyserial? | **yes**, with `--port`, `list` for COM ports, and the same command surface; `scripts/arduino.ps1 -Task monitor` stays for raw eyes-on-monitor use |
| Q11 | Do the four reserved button pins still have a purpose, or is this the moment to drop them from `Pins.h`? | **keep the pins, drop nothing**: unmapped hardware is cheaper than a board revision (Q16 stands) |
| Q12 | Should a line end on `\n` only, or should a bare `\r` also terminate it (some terminals send CR only, and a serial console is exactly where people find that out)? | **accept both** as terminators: `\r\n` then yields one empty line, which the router already skips. Two lines of code, and a CR-only terminal would otherwise look like a dead device |

## 11. Status

| Revision | Change | Sections |
| --- | --- | --- |
| rev 1 | initial plan: serial transport, polling telemetry, log sharing, link policy | all |
