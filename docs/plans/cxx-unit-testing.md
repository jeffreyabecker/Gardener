# Unit testing the core library

`libraries/Gardener` carries the machine and the control law that guards a person's body, the wire's
domains, the documents and the keys. Nothing answers whether any of it is right short of a board or
a whole firmware run. No host command compiles one of its sources and drives it, no case is named,
selected or timed, and so a change to the control loop, the wire, the documents or the keys lands
unproved. One host check does exist — `python tools/gfx/check.py` — and it answers for
`libraries/Gfx` alone: its source list is that library and its own files, so a case exists for the
drawing and for nothing the core library does. The only other host compile naming a Gardener source
is the interpreter's tool, `python tools/berry/check.py`, which builds the whole VM, needs
`firmware/data` beside it and is itself in a known broken state; it is neither a unit test suite nor
runnable, and what it does is not this plan's subject.

This plan makes the tree's host suite one command that compiles both libraries that need no
interpreter — `libraries/Gfx` and `libraries/Gardener` — drives them through fakes and cases, runs
one case by name in seconds, and exits non-zero when a case fails. The Gfx check is absorbed rather
than left beside it: one command, one test framework, one host `Arduino.h`, one stub display, one
way to run a case. A unit test is only as good as the surface it compiles against, so the plan pins
the host's Arduino surface to Arduino's published API instead of a hand-written copy of `String`; it
is only as good as the runner behind it, so a case is a `TEST_CASE` in a maintained framework rather
than a call appended to a list the tree keeps. Cases then cover the behaviour that exists today: the
link's framing, the machine's wire surface, the control law on a virtual clock, a note sequence, the
documents, the drawing and layout Gfx's cases already assert, and the frame a case drew, saved by a
writer the suite and the interpreter's tool share.

The sketches, the `.be` image, the panel's Berry-facing code and the hardware parts under
`libraries/Runtime` are left alone. No file the firmware build compiles changes, so no firmware
build is part of proving this plan.

## A case is run on the host, one at a time

There is no such command today. The Gfx tool answers for one library and does it with a runner of
the tree's own: `tools/gfx/suite.h` declares `Case`, `expect`, `expectEq` and `runCase`,
`tools/gfx/main.cpp` counts what they return, and each case file appends its cases through an
`addDrawingCases`-style function. A case there cannot be listed or run on its own, an assertion
reports a sentence rather than a file and line, and nothing stops a case at its first failed
assumption. It keeps a frame that nothing saves either. The interpreter's tool is not a unit test
suite either: its source list holds every Gardener source beside the vendored VM, the interpreter
host sources under `libraries/BerryHost/src/` and its harness, so compiling the core library and
building the interpreter are one act, and the two case files it does have run only after the image
has loaded. A failure there does not say whether the C++ or the image was wrong, and the cases
cannot run at all while the interpreter does not build.

A command in `tools/` compiles both interpreter-free libraries — `libraries/Gfx` and
`libraries/Gardener` — and their cases, and `python tools/gfx/check.py` goes with its own source
list, its host `Arduino.h`, its runner and its own idea of how a case is run. Nothing the command
builds names an interpreter, a filesystem, a radio or an RTOS: no source, header or link input under
`libraries/berry`, `libraries/BerryHost`, `tools/berry` or `firmware/data` is among its inputs, and
a source that names `<berry.h>`, `<FS.h>`, `<Wire.h>`, `<NimBLE*>`, `freertos/*` or `<SD.h>` is not
among the sources it compiles. It needs a C++ compiler, Python and a test framework and nothing
else — no ESP32 core, no `arduino-cli`, no data image — and it passes its arguments through to the
framework, so the framework's own filter, listing and reporting are what the suite has.

The fakes are the suite's: a stub display the drawing cases draw into, a pump, a pressure sensor, a
vent valve and a buzzer implementing the `libraries/Gardener/src/hal/` interfaces, a logger sink
recording the lines a domain logs, an in-memory `Library`, and a clock the case advances. No case
constructs a hardware part, and each fake's state is readable, so a case asserts what the domain did
to the part and not only what it printed.

Checked: the command passes and its counts cover both libraries' cases; `python tools/gfx/check.py`
and its runner are gone and the cases it held run under the one command; and no compiler command
the run prints carries a `-I` or a source path under `libraries/berry`, `libraries/BerryHost`,
`tools/berry` or `firmware/data`.

## The framework is doctest, not the tree's own

A unit test suite is worth the framework under it, and the tree is currently its own. `suite.h`'s
`expect`, `expectEq` and `expectText` take a sentence as the message and record the first miss;
`runCase` prints a line per case; `main` counts. What a maintained framework gives for free is
absent: a case listed and filtered by name, a failure reported at its file and line with the
expression that failed, and assertions that either stop the case or let it go on. The six Gfx case
files also reach the runner through hand-written `addDrawingCases`, `addLayoutCases` and their
siblings, which every new case file has to extend.

The suite is built on doctest, vendored at a pinned release, 2.4.11 or newer, under `tools/` with
its MIT licence beside it, as the Arduino API and the capture library are. One source defines
`DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` and the run's `main` is the framework's; every case file
includes the header and holds `TEST_CASE` blocks named for the behaviour they assert, with
`CHECK(…)` for a failure the case can carry on past and `REQUIRE(…)` for one it cannot. Nothing in
the tree is a test framework: no `expect`, `expectEq`, `expectText`, `runCase`, case struct, counts
or `main` of its own, and no per-file `addXxxCases` list for a new file to extend.

Selecting a case is the framework's `--test-case=` with the pattern the user types, listing them is
`--list-test-cases`, and the Python script hands both through and takes the framework's exit code as
its own. A case still runs alone in the seconds a single-file rebuild takes rather than the minutes
a firmware build takes, because the command compiles only what changed.

Checked: `--list-test-cases` names every case, including the ones moved from the Gfx tool and from
`tools/berry/host/`; `--test-case=` with one name runs that case and reports only it; a case whose
`CHECK` is deliberately made false fails the run with the framework's own failure line, its file and
line included, and a `REQUIRE` in the same case stops it there; and a search of the suite's own
sources finds no `expect(`, `runCase`, case struct or `main`.

## The host Arduino surface is Arduino's API

A case proves what the device does only if it compiles the source the device compiles, against a
surface that answers the way the device's does. There are two hand-written copies of that surface
today. `tools/gfx/host/Arduino.h` gives a `String` with `c_str`, `isEmpty`, `toFloat`, `toInt` and
`equalsIgnoreCase`; `tools/berry/host/Arduino.h` includes `WString.h`, whose `String` answers far
more. A source compiled for one tool is not a source compiled for the other, and a case can pass
against the small text type while the device's `String` answers differently. Neither defines
`Stream`, so nothing under `serial/` compiles on the host at all, and the fake clock is only the
berry tool's (`tools/berry/host/Clock.h` until it moved, advanced by `host::advance`).

The host surface is Arduino's `ArduinoCore-API`, vendored at release 1.5.2 (`ARDUINO_API_VERSION`
10502) with its licence beside it, under `tools/` where no sketch build looks: `scripts/arduino.py`
offers `libraries/` as the sketch's local-library path, and a second `Arduino.h`, `WString.h`,
`Print.h` or `Stream.h` there would be resolved instead of the core's. No copy in the tree writes
those classes: the two host folders lose the ones they have. From the API come `String`, `Print`,
`Stream`, `Printable`, the declarations `Common.h` makes (`millis`, `delay` and the pin calls
nothing on the host uses), and the `using` declaration that puts `String` in the global namespace
the firmware sources write; the `itoa`, `ltoa`, `utoa`, `ultoa` and `dtostrf` implementations a
board's C library would provide are taken from the same repository's host test sources. From the
tree come only what a core supplies beside the API: the `Arduino.h` entry header a source includes,
which includes the vendored API and declares nothing of its own; a `Stream` subclass a case fills
with the text it typed and reads back what a domain printed; and the `millis()` and `delay()` a case
advances. Nothing else is needed: `device get` reads the board's chip and heap from the
`BoardFacts` function pointers it is handed, so there is no `ESP` object to supply.

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
the API's classes — the host's `Arduino.h` is the entry header, a handful of includes — and the one
suite builds both libraries against the vendored API. The cases that moved from the Gfx tool keep
their expectations and pass against the API's `String`, and a core case asserts a float formatted
with `String(1.5f, 1)` and a text grown past its first allocation, so the surface is exercised where
the firmware uses it.

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

## A drawn frame is saved by a host library the tools share

`MarkScene`, `PressureReadout` and `Band` draw through Gfx and are in the core library, but a case
over them can only assert the pixels a stub display holds. The only host code in the tree that turns
a drawn frame into a file is the interpreter's harness: `tools/berry/host/GifWriter.cpp` is a source
of the interpreter's check, its enlargement and its per-frame delay are constants in its own header,
its frames arrive as `BitmapPanel::packFrame` copies, and what it writes is decided by
`HarnessSession` — `beginCapture`, `endCapture`, `saveStill` and the stem a per-case file is named
from. `tools/gfx/check.py`'s stub display keeps a frame that no command saves. So a host check that
wants to look at a frame can only do what the interpreter's tool did — copy the writer, its LZW
stage and its constants — and the tree would carry two of them, drifting apart.

The writer is a host library of its own under `tools/`, beside the suite rather than inside it, and
the suite and the interpreter's tool both link it: one copy of the LZW stage and one pair of
constants for every host tool that saves a frame. It sits outside `libraries/`, so no sketch build
sees it and nothing about the two builds' image changes. It draws nothing and knows no panel, page,
gesture, overlay band, routine, harness or interpreter: its only inputs are a width, a height,
frames packed one bit per pixel, row-major and top-down — which is how a display buffer already
holds them — and a path. It names no Arduino header either, so any host compile links it with no
board behind it. It writes a GIF89a with a two-entry palette of white and black, a NETSCAPE loop,
and one frame per entry held `GifDelayCs` 100 centiseconds, each enlarged by nearest neighbour at
`RenderScale` 2, so a 128 x 64 frame is a 256 x 128 file. Those two values move with the writer and
keep their numbers, so the files the interpreter's tool writes do not change. A still is one frame;
an armed capture takes frames as they arrive and writes them when asked, so a case that advances a
fake clock can save the sequence it drove. A path that cannot be opened answers false without ending
the run, as `writeGif` does now, and the naming policy — where a case's file goes and what its stem
is — stays with the tool that names it.

Checked: two cases in the suite — one over a core panel scene, one over a Gfx component — each save
a still they drew, and each file begins `GIF89a`, measures 256 x 128, carries the two-entry palette
and the 100 cs delay, and holds ink at the column and row the case drew, at twice the coordinate; an
armed capture of three frames writes three image blocks; the writer's name appears under its one
home in `tools/` and nowhere else in the tree; and no source or header under `tools/berry/` is among
the suite's inputs, so it needs no harness to save a frame.

## Consequences

- The Build section of `.github/copilot-instructions.md` and `README.md` name the suite's command as
  the check for a C++ change, and say what it covers — both `libraries/Gfx` and
  `libraries/Gardener` — because `python tools/gfx/check.py` no longer exists.
- The tree carries `ArduinoCore-API` 1.5.2 under `tools/` with its LGPL-2.1-or-later notice beside
  it; no `String`, `Print` or `Stream` is the tree's own, and moving to a newer release is a
  deliberate change that the checks hold.
- `tools/gfx/check.py` and its host folder go: its per-area case files become `TEST_CASE`s under the
  new suite, its stub display moves with them, and its `suite.h` assertions and `main` do not
  survive. Gfx's behaviour does not change, and the cases it had are what holds that.
- The tree carries doctest at a pinned release, 2.4.11 or newer, under `tools/` with its MIT licence
  beside it; the suite's `main`, its filter, its listing and its failure report are the framework's,
  and replacing it is a deliberate change that the cases hold.
- The berry skill says the interpreter's check is for the image, the pages and the bindings, keeps
  its statement that the check is broken and not to be run, and sends anything under
  `libraries/Gardener` or `libraries/Gfx` to the suite instead.
- The registry's and the store's case files leave `tools/berry/host/` for the suite, so one copy of
  each exists; the interpreter's tool keeps its harness, its image work and its `.be` suites, and
  nothing in this plan requires it to be run.
- `tools/berry/host/GifWriter.h` and its `.cpp` leave for the host library under `tools/`, and the
  harness includes its header instead. The captures that tool writes keep their frames, palette,
  delay, scale and paths.
- `README.md` and the berry skill name that host library as what saves a frame, so a host tool that
  draws links it rather than the harness.
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
- `PanelTasks.cpp` and the bindings, which name the interpreter.
- Reviving `tools/berry/check.py`, and the spec for what it loads and writes, which the parking lot
  holds until the tool runs again.

## How it is known to hold

1. The suite's command exits 0 and prints the framework's summary, covering both `libraries/Gfx`'s
   cases and the core library's.
2. `--list-test-cases` names every case, and `--test-case=` with one name runs that case alone.
3. A case whose `CHECK` is deliberately made false fails that run, reporting the framework's own
   failure line with the file and line, and a `REQUIRE` stops its case there.
4. Two stills the suite saves begin `GIF89a`, measure 256 x 128 and hold ink where their cases drew
   it, one over a core panel scene and one over a Gfx component.
5. `git diff --stat` over `libraries/Gardener/src` and `libraries/Gfx/src` is empty. Nothing the
   firmware build compiles changes, so no firmware build is part of proving this, and the suite is
   the only host check over the tree's C++ that runs at all.
