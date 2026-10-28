# Unit testing the core library

`libraries/Gardener` carries the machine and the control law that guards a person's body, the wire's
domains, the documents and the keys. Nothing answers whether any of it is right short of a board or
a whole firmware run. No host command compiles one of its sources and drives it, no case is named,
selected or timed, and so a change to the control loop, the wire, the documents or the keys lands
unproved. `python tools/gfx/check.py` is a host suite, but its source list is `libraries/Gfx` and
its own files, so it names no Gardener source; the only host compile that does is the interpreter's
tool, `python tools/berry/check.py`, which builds the whole VM, needs `firmware/data` beside it and
is itself in a known broken state.

This plan gives the core library a host unit test suite: a command that compiles the sources naming
no interpreter, no filesystem, no radio and no RTOS, drives them through fakes, runs one case by
name in seconds, and exits non-zero when a case fails. A unit test is only as good as the surface it
compiles against, so the plan pins the host's Arduino surface to Arduino's published API instead of
a hand-written copy of `String`. Cases then cover the behaviour that exists today: the link's
framing, the machine's wire surface, the control law on a virtual clock, a note sequence, the
documents, and the frame a panel case drew.

The sketches, the `.be` image, the panel's Berry-facing code, the hardware parts under
`libraries/Runtime` and the interpreter's tool are left alone. What that tool runs once the core has
a suite of its own is not this plan's subject.

## A case is run on the host, one at a time

There is no such command today. `tools/berry/check.py`'s source list holds every Gardener source
beside the vendored VM, the interpreter host sources under `libraries/BerryHost/src/` and its
harness, so compiling the core library and building the interpreter are one act, and the two case
files it does have run only after the image has loaded. A failure therefore does not say whether the
C++ or the image was wrong; the cases cannot run at all while the interpreter does not build; and
nothing outside those two files is covered. `tools/gfx/check.py` shows the shape a host suite has —
named cases, an expectation that records the first miss, a count and an exit code — but it compiles
no Gardener source, has no way to run one case, and its host `Arduino.h` is a copy.

A command in `tools/` compiles the core library's sources and runs its cases with no interpreter
behind it: no source, header or link input under `libraries/berry`, `libraries/BerryHost`,
`tools/berry` or `firmware/data` is among its inputs, and a source that names `<berry.h>`, `<FS.h>`,
`<Wire.h>`, `<NimBLE*>`, `freertos/*` or `<SD.h>` is not among the sources it compiles. It needs a
C++ compiler and Python and nothing else — no ESP32 core, no `arduino-cli`, no data image. A case is
named for the behaviour it asserts, a failing case prints its name and the expectation it missed,
and the run's last line is the counts; the exit code is 0 only when the failure count is 0.

A case is also something to run alone while a change is being made. One case is selected by name on
the same command, the run then reports that case's name and counts, and a rebuild after an edit to a
single source or case takes seconds rather than the minutes a firmware build takes.

The fakes are the suite's: a pump, a pressure sensor, a vent valve and a buzzer implementing the
`libraries/Gardener/src/hal/` interfaces, a logger sink recording the lines a domain logs, an
in-memory `Library`, and a clock the case advances. No case constructs a hardware part, and each
fake's state is readable, so a case asserts what the domain did to the part and not only what it
printed.

Checked: the command passes with `libraries/berry`, `libraries/BerryHost`, `tools/berry` and
`firmware/data` moved aside; a case whose expectation is deliberately made false fails the run and
prints its own name; the command naming one case runs that case and reports it by name; and no
compiler command the run prints carries a `-I` or a source path under the four directories.

## The host Arduino surface is Arduino's API

A case proves what the device does only if it compiles the source the device compiles, against a
surface that answers the way the device's does. There are two hand-written copies of that surface
today. `tools/gfx/host/Arduino.h` gives a `String` with `c_str`, `isEmpty`, `toFloat`, `toInt` and
`equalsIgnoreCase`; `tools/berry/host/Arduino.h` includes `WString.h`, whose `String` answers far
more. A source compiled for one tool is not a source compiled for the other, and a case can pass
against the small text type while the device's `String` answers differently. Neither defines
`Stream`, so nothing under `serial/` compiles on the host at all, and the fake clock is only the
berry tool's (`tools/berry/host/Clock.h`, advanced by `host::advance`).

The host surface is Arduino's `ArduinoCore-API`, vendored at release 1.5.2 (`ARDUINO_API_VERSION`
10502) with its licence beside it, and nothing in the tree writes `String`, `Print`, `Stream` or an
`Arduino.h` of its own. From the API come `String`, `Print`, `Stream`, `Printable`, the declarations
`Common.h` makes (`millis`, `delay` and the pin calls nothing on the host uses), and the `using`
declaration that puts `String` in the global namespace the firmware sources write; the `itoa`,
`ltoa`, `utoa`, `ultoa` and `dtostrf` implementations a board's C library would provide are taken
from the same repository's host test sources. From the tree come only the three things a core would
supply: a `Stream` subclass a case fills with the text it typed and reads back what a domain
printed, the `millis()` and `delay()` a case advances, and the `ESP` object `device get` reads for
`chip` and `heap`.

The calls the firmware makes are the API's: construction from `const char*`, `String`, `char`, the
integers, and a float with a decimals argument; `c_str`, `length`, `isEmpty`, `charAt`,
`operator[]`, `indexOf` and `lastIndexOf` with an optional start, `substring` in both forms, `trim`,
`toLowerCase`, `toUpperCase`, `toInt`, `toFloat`, `toDouble`, `toCharArray`, `equalsIgnoreCase`,
`startsWith`, `endsWith`, `==`, `!=`, `<`, and `+`/`+=` against a `String`, a `const char*` and a
`char`. `Stream` is the API's, with `available`, `read`, `peek`, `readStringUntil` and every `print`
overload, so a case hands a domain the text it typed and reads back the bytes it printed.

The ESP32 core does not take its `String` from this API: `cores/esp32/WString.h` is its own, newer
fork of the same Paul Stoffregen class, with small-string storage and the long-long constructors.
The host therefore runs the API's implementation rather than the device's. The two agree on the
surface — the names, arguments and answers the firmware uses — and it is that surface the cases are
written against; swapping in the core's copy is deferred.

Checked: no `Arduino.h`, `WString.h`, `Print.h` or `Stream.h` in the tree is a hand-written copy of
the API's classes, and both host suites build against the vendored API. The Gfx suite's cases pass
unchanged against the API's `String`, and a core case asserts a float formatted with
`String(1.5f, 1)` and a text grown past its first allocation, so the surface is exercised where the
firmware uses it.

## The link's framing is a case

`SerialLink` takes a `Stream&` and its rules live in `serial/SerialLink.cpp`: a domain token ends at
a space or a tab, a `#`-leading line is ignored, a token that names no domain answers
`err unknown_domain` and the rest of the line is swallowed, a token that names a domain with no
command answers `err bad_argument`, and a token longer than `COMMAND_LINE_MAX` (128) answers
`err bad_argument` and swallows. No host check compiles that file, so those answers are only read by
a person at a terminal.

A case feeds bytes to `SerialLink::poll` through a buffer and asserts the reply: a lowercase, an
uppercase and a mixed-case domain all dispatch; a domain token with nothing after it answers
`err bad_argument`; a `#` line answers nothing and the next line still dispatches; `vacuum\tget`
dispatches as `vacuum get` does; a `\r\n` line leaves no `\r` in the command; a 129-byte token
answers `err bad_argument`; an unknown token answers `err unknown_domain <token>` lowercased; and a
domain's body — everything after its token — reaches the domain's `execute` intact.

Checked: one case per rule above, each asserting the exact bytes written; and a presence function
that turns false once makes the link report `serial: controller detached` through the recording
logger and drop a half-read token, so a reconnect never joins two commands.

## The machine's wire surface is built from fakes

The machine's two domains are shaped around `Machine&` (`serial/Domains.h`) and read the board's
chip and heap from the `BoardFacts` they are handed, so nothing in them needs a board — and still
`serial/domains/Vacuum.cpp` and `serial/domains/Device.cpp` are compiled by no host check. What the
wire promises — the `get` field list, the bounds and the refusals — is asserted nowhere.

A case builds the machine's wire surface from a fake pump, sensor and valve — the same parts the
control law's cases use — and the concrete parts stay in the composition the sketch calls. Then
`vacuum get` answers `pressure_kpa` to two decimals, `target` and `floor` and `duty` to one,
`state`, `sensor`, `fault`, `hold`, `max_run` and `up`; `vacuum target 5` queues it, `vacuum target`
with a second token, `-1`, `nan` and `60.1` answer
`err out_of_range field=target value=<token> min=0.0` or `max=60.0`, and `0`, `-` and `60` are
accepted by queueing; `vacuum start` refuses with `err not_ready latch` while a fault is latched and
`err not_ready no_reading` with no reading, refuses `0`, `1.5` and `901` as
`err out_of_range field=hold` with `min=1` or `max=900`, and answers `ok state=PULL` when it queues;
`vacuum stop` and `vacuum reset` answer `ok state=VENT` and `ok state=IDLE` and queue their op;
`device get` answers `proto`, `fw`, `chip`, `heap`, `log` and `caps`; `device set log=we` answers
`ok log=WE` and reaches the sink, `device set log=x` answers `err bad_argument x` and leaves the
levels alone, and a second token refuses before anything applies.

Checked: the cases above, each reply asserted byte for byte, a refused `start` leaving the fake sink
empty while a queued one fills it, and the capability byte — a fake valve that vents when
de-energised puts `FailSafeValve` (0x04) in `caps`, one that holds without power does not.

## The control law runs on a virtual clock

`VacuumSystem` is the class the device's whole safety story rests on and it is shaped for a host: it
takes `Pump*`, `PressureSensor*` and `VentValve*`, all pure virtual, and `service(nowMs)` carries
its own time. Nothing constructs it on the host, and it reads `millis()` for every timestamp it
keeps — `begin`, `release`, `start`, `beginTargetVent` and `closeValve` — so a case has to own the
clock before any of those is called.

A case drives `begin()` and 50 ms `service()` beats with a fake pump, sensor and valve on a clock it
advances, and asserts the state, the valve, the duty and `faultCode()` against the numbers the loop
already carries: `CONTROL_PERIOD_MS` 50, `CONTROL_MIN_TARGET_KPA` 5.0, `CONTROL_FLOOR_KPA` 60.0,
`CONTROL_DEADBAND_KPA` 0.2, `PUMP_DUTY_MAX_PERCENT` 90, `VENT_FLOOR_KPA` 1.0 with `VENT_MARGIN_KPA`
2.0, `VENT_TIMEOUT_MS` 30000, `CONTROL_MAX_RUN_MS` 900000, `CONTROL_PROGRESS_KPA` 1.0 within
`CONTROL_PROGRESS_TIMEOUT_MS` 10000, and `CONTROL_OVERSHOOT_KPA` 5.0 over
`CONTROL_OVERSHOOT_READINGS` 5.

A target of 0 releases — VENT, valve open, pump stopped; `stop` from Idle changes nothing and starts
no vent; `stop` while a release is venting does not restart its 30 s; a start while running is a
no-op that keeps the first deadline; a hold of 0 or 901 asks 900 s and `holdSeconds()` reports 900;
the deadline ends the run as a release, not a fault, and `takeDeadlineExpired()` answers true once;
a target set shallower than the reading while holding opens the valve, closes it at target + 1.0 kPa
and returns to PULL; a sensor that stops answering faults `sensor` on the next beat, opens the valve
and refuses a start with `latch`; a release still above 3.0 kPa at 30 000 ms faults `vent_timeout`;
a pull at or above half the duty cap with less than 1 kPa of headway in 10 000 ms faults
`no_progress`; five readings 5 kPa past the target fault `overshoot`; the duty never exceeds 90; and
every release stops the pump before it opens the valve.

Checked: one case per sentence above, on an advanced clock, so the whole set runs in under a second.

## A note sequence is a case

`BuzzerPlayer` is a parser and a schedule over `Buzzer&`, which is pure virtual, and nothing checks
it: `panel/BuzzerPlayer.cpp` is in the interpreter's compile list and has no case there, and its
callers are the panel's `buzzer` domain and the `device` binding, neither of which has a case. The
grammar and the bounds are real behaviour: `note:duration` pairs separated by spaces or tabs, note
names `a` to `g` with an optional `#` or `b` and one octave digit, `r` for a rest, equal temperament
with A4 at 440 Hz, a frequency outside `BuzzerMinHz` 100 to `BuzzerMaxHz` 10000 refused rather than
clamped, a duration of zero refused, and a sequence that is empty or has a non-space after a
duration refused, with `badToken` left at the first token that did not parse.

A fake `Buzzer` records the level it is driven at, so the schedule is assertable too: `validate`
answers without starting anything, `play` starts the first note at the current millisecond, `update`
moves on at a note's end and silences the part after the last one, `remainingMs` counts down and
answers 0 once stopped, and a continuous `tone(hz, 0)` has no remaining time.

Checked: `validate("a4:1000 c6:500")` is true; `validate("h4:100")` is false with `badToken`
pointing at `h4:100`; `a0`, `c#0` and `b9` are each refused as out of the part's range or of MIDI,
while `b8` at 7902 Hz is accepted; `a4:0`, `a4`, `a4:100x` and the empty sequence are false with the
token named; a refused `play` leaves the sound that was playing; and a played sequence drives the
fake at 440 Hz, then at 1047 Hz, then silent.

## The documents are read from a store the suite owns

`Config` and `RoutineSettingStore` take their files through `hal/Library.h`, which is the seam a
host case needs, but the only in-memory `Library` is `tools/berry/host/MemoryFiles.h` — it copies
`firmware/data` and exists for the harness — and the two case files that use it,
`RoutineRegistryCheck.cpp` and `RoutineSettingStoreCheck.cpp`, are compiled into the interpreter's
check and called once the image loads. The document's shape, the typed reads and the
one-write-per-save rule are therefore checked only in a run that has loaded the interpreter.

The suite owns an in-memory `Library` written per case and loaded from no directory, and the
registry's and store's cases move onto it. `Config`'s shape is asserted as it parses: `;` lines,
blank lines and lines with no `=` are ignored; a header is trimmed, so `[ cold ]` is `cold`; keys
before the first header are the empty section; the first `=` splits, so a value may hold one; a key
that appears twice keeps its first place and the last value; and `text()` gives back the same text,
with the empty section unheaded and the sections in the order they first appeared. It reads its
types from that text: `on`, `true`, `1` and `yes` in any case are true, `off`, `false`, `0` and `no`
false, and anything else leaves the fallback; a missing document leaves every default; and five
`set`s and one `save` write once, while a second `save` writes nothing.

Checked: one case per rule above; the fixture document's alternate section and its defaults still
read back through the registry; the store still writes a settled set back; and the fake store's
write count is asserted at 1 after a multi-key save and at 0 after a repeat.

## A drawn frame is a case's to save

`MarkScene`, `PressureReadout` and `Band` draw through Gfx and are in the core library, but a case
over them can only assert the pixels a stub display holds. The only host code in the tree that turns
a drawn frame into a file is the interpreter's harness: `tools/berry/host/GifWriter.cpp` is a source
of the interpreter's check, its enlargement and its per-frame delay are constants in its own header,
its frames arrive as `BitmapPanel::packFrame` copies, and what it writes is decided by
`HarnessSession` — `beginCapture`, `endCapture`, `saveStill` and the stem a per-case file is named
from. `tools/gfx/check.py`'s stub display keeps a frame that no command saves.

A capture facility in the host tree belongs to no check and links none of the interpreter, the data
image or the harness; the core suite and the Gfx suite both link it, and the interpreter's tool may
adopt it when it runs again. It draws nothing and knows no page, gesture, overlay band or routine:
it takes a width, a height and frames packed one bit per pixel, row-major and top-down, which is how
a display buffer already holds them, and writes a GIF89a with a two-entry palette of white and
black, a NETSCAPE loop, and one frame per entry held `GifDelayCs` 100 centiseconds, each enlarged by
nearest neighbour at `RenderScale` 2 — a 128 x 64 frame is a 256 x 128 file. A still is one frame;
an armed capture takes a frame per beat and writes it when asked, so a case that advances a fake
clock can save the sequence it drove. The path is the caller's, and one that cannot be opened
answers false without ending the run, as `writeGif` does now.

Checked: a case in the core suite saves a still it drew, and the file begins `GIF89a`, measures
256 x 128, carries the two-entry palette and the 100 cs delay, and holds ink at the column and row
the case drew, at twice the coordinate; a case in `tools/gfx/check.py` saves a still the same way;
and no source or header under `tools/berry/` is among either suite's inputs, so neither needs the
harness to save a frame.

## Consequences

- The Build section of `.github/copilot-instructions.md` and `README.md` name the core suite's
  command beside `tools/gfx/check.py`, and send a change to the machine, the wire, the documents or
  the keys to it.
- The tree carries `ArduinoCore-API` 1.5.2 with its LGPL-2.1-or-later notice beside it; no `String`,
  `Print` or `Stream` is the tree's own, and moving to a newer release is a deliberate change that
  the checks hold.
- `tools/gfx/check.py`'s host folder changes with the shared surface; Gfx's own behaviour does not,
  and its cases are what holds that.
- The berry skill says the interpreter's check is for the image, the pages and the bindings, names
  the core suite for anything under `libraries/Gardener`, and keeps its statement that the
  interpreter's check is broken and not to be run. The core suite becomes the only host check that
  names the firmware's own C++.
- The registry's and the store's case files leave `tools/berry/host/` for the core suite, so one
  copy of each exists; the interpreter's tool keeps its harness, its image work and its `.be`
  suites.
- The wire's field list and refusals become cases rather than a description: the archived serial
  spec is out of date and read by nothing, and it stays that way.
- `MemoryFiles` stays with the harness, which loads the image with it; the suite's store is its own.

## Deferred

- The concrete parts — `PwmPump`, `XgzpSensor`, `SolenoidValve`, `PwmBuzzer`, `AdcBattery`,
  `Sh1106Device` and `Buttons` under `libraries/Runtime` — mean nothing off a board; the parking-lot
  entry on verifying the two builds on hardware is where that lives.
- The ESP32 core's own `WString`, the class the device actually runs, in place of the API's
  `String`, if the two are ever seen to disagree: it is a newer fork with small-string storage and
  a log call on the allocation-failure path, so it is more than a header swap.
- `Machine::boot`'s device probes beyond the capability byte, `SafeReboot` and the two sketches.
- `BleLink`, the two BLE domains and NimBLE.
- `Panel`, `PanelTasks.cpp`, `Routines` and the bindings, which name the interpreter.
- Reviving `tools/berry/check.py`, and the spec for what it loads and writes, which the parking lot
  holds until the tool runs again.

## How it is known to hold

1. The core suite's command exits 0 and prints its counts with `libraries/berry`,
   `libraries/BerryHost`, `tools/berry` and `firmware/data` moved aside, and one case selected by
   name runs alone and reports its own name.
2. A case whose expectation is deliberately made false fails that run.
3. `python tools/gfx/check.py` exits 0 against the API's `String` and the tree's one `Stream`.
4. A still saved by the core suite and one saved by `tools/gfx/check.py` are GIF89a and 256 x 128,
   with ink where the case drew it.
5. `python scripts/arduino.py --task build` compiles both sketches: the suite adds no build symbol,
   and the seams it needs — the `hal/` interfaces and `hal/Library.h` — are the composition the
   sketch already calls.
