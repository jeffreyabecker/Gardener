# How should a walkthrough of the app be scripted and turned into a GIF?

The host harness already draws the app: `--art <routine>` and `--scene <name>` in
[tools/berry/check.py](../../tools/berry/check.py) push a page, run a canned few beats of it and
write the frames as an animated GIF. What each of those walks does is written by hand in
`tools/berry/driver.be`, one Berry function per picture, so a new picture — the browser, a settings
row being edited, a cycle caught mid-phase — is a function rather than an argument. The question is
what it takes for the walk itself to be data: a command list naming the scene, the gestures, the
waits and how fast the rig pumps and vents, and a tool that runs the list and leaves the GIF.

## The command list

One command per line, run top to bottom. A line beginning with `#` is a comment, and a blank line is
discarded. A `load`'s settings are `key=value` pairs separated by `;`, and no value can hold a `;`,
so the pairing is never ambiguous. A nullable setting with nothing in it is written `key=`, with
nothing after the `=`. A string or a choice value may hold spaces and is taken as written; a number,
a pressure or a duration is parsed.

| Command | What it does | What it is today |
| --- | --- | --- |
| `load <scene> (settings)` | pushes `home`, `browser`, or a routine by key; settings are `key=value;key=value` | `routines.activate(key, map)`; `home` and `browser` are globals |
| `send <gesture>` | one gesture, spelled as the classifier spells it | `harness.press`, into the queue the page drains next beat |
| `wait <n> s\|ms` | advances the device's virtual clock by that much | `HarnessSession::beat`, one 250 ms beat: four control ticks and a page poll |
| `set-pump <kpa_per_s>` | the chamber's pull at full duty | `PullKpaPerSecond`, 3, a constant in the host |
| `set-vent <kpa_per_s>` | the chamber's venting rate | `VentKpaPerSecond`, 40, a constant in the host |
| `emit` | writes what was drawn and quits | `harness.capture_finish()`, after which a run returns |

The two rate commands are what make a walk possible at all inside a GIF: at 3 kPa/s a forty-kPa
target is thirteen seconds of device time, which is fifty-two frames of a picture whose point is
the last one.

## The tool

A program of its own, outside the check: `tools/walkthrough/`, built against one small library that
holds what the host already has — the session and the chamber from `tools/berry/host/`, the capture
and the writer from `tools/host/capture/`, beside the vendored VM and Gfx — and with nothing in its
arguments, its output or its exit code about the image's files or the routines a check drives.

It reads the whole command file into a list before anything is drawn, skipping the blank lines and
the `#` comments, refuses the file when a line is not a command and names the line, and then runs
the list. The four verbs that are not the page's are the host's already: `wait` converts its units
into beats and hands the count to `HarnessSession::beat`, `set-pump` and `set-vent` are setters on
the chamber, and `emit` writes the captured frames and returns. The other two need no Berry of the
tool's either: `send` is `Routines::pushPress`, and `load` is a name looked up three ways — `home`
gives the panel back to its root, `browser` is the image's global wrapped in a `BerryScene` the way
the interpreter wraps `__rootScene`, and any other name is a routine key handed to
`RoutineRegistry::activate` and pushed over the stack after the `Ux::popToRoot` and the stop a
launch does. The tool therefore writes no Berry at all: the vocabulary stays the image's and the
tool only calls it.

What it costs: a third build and a third `main` beside the check's and the harness's, and a small
library carved out of the sources the check compiles today — the session, the harness module and the
capture — so the two build against the same ones rather than the tool copying them. A `load` that
carries settings costs a conversion: the pairs are split on `;`, which a value can never hold, and
each text is turned into the kind its key was declared with — a string or a choice taken as written,
spaces and all, and a number, a pressure or a duration parsed — into the `std::any` a settings map
holds.

## Where the command list and the tree disagree

- **Two clocks, not one.** A `wait` is device time: the units convert into the session's beats and
  the beat is what advances the machine, the chamber and the page. Playback is the tool's own — a
  frame delay given on the command line, with the check's `GifDelayCs` of a hundred centiseconds
  left to the check — so a `wait 20 s` is eighty beats of device time and still a picture whose
  frames each play for as long as the tool was told. Device time and playback are two numbers, and
  the file only states one of them. Nothing waits on the wall clock either: the beats are run back
  to back, so a long walk costs frames in the GIF rather than seconds of the tool's time.
- **Beats, not milliseconds.** The conversion is into 250 ms beats and the remainder rounds up, so a
  `wait 300 ms` is two beats and a `wait 1 s` is four: the clock always advances at least as far as
  the line asks, and never more than one beat past it.
- **`set-pump` configures a model, not a rig.** What moves is the chamber's depth per duty second
  in the host; the leak stays at its own constant and the ceiling stays `--ceiling-kpa`. A file
  that reads like a bench setting is a file someone will trust as one.
- **A gesture is a label as well as a press.** The strip under the picture shows the gesture for
  `GestureStrip::Frames` frames after it is sent, so a `send` and a one-beat `wait` put the press in
  the queue and a single frame of the label in the picture.

## What would have to be true

- The tool builds and runs on its own, and the check's runs are unchanged by it.
- `load` and `send` are the only verbs that reach the image, and they reach it through the calls a
  launch and a press already take — the registry and the stack for a page, the press queue for a
  gesture — so no Berry is written for the tool.
- The whole file is read and checked before the first frame is drawn, so a misspelled scene fails
  before half a GIF is on disk.
- A wait converts to whole beats by rounding up, so a walk's device time is never less than the
  file says.
- A settings key the routine does not declare is refused before the picture rather than dropped on
  the way in, and each value is converted to the kind its key was declared with.
- The GIF's timing, frame size and enlargement, and the output path are the tool's options, held
  apart from the device time the waits advance; the check's own constants stay the check's.
- The same file run twice writes the same GIF, which holds because the clock and the chamber are
  the host's.

## Recommendation

The tool as described, built against the small library the check also builds against, with `wait`
converting its units into beats for the session, the GIF's timing one of the tool's options rather
than a line of the file, and a setter on the chamber for each of its rates.
