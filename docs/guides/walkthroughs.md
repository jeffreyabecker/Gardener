# Panel walkthroughs

A walkthrough is a text file of commands that drives the Gardener panel on a host and writes the
frames it drew as an animated GIF. The tool runs the firmware rather than a model of it: the
interpreter's host side and the panel's registry, overlay, settings and control law are compiled
against the vendored VM, with a bitmap for the glass and a model chamber for the rig, over the same
session the Berry check drives. No board, no device interpreter and no flashing are involved.

## Running one

```powershell
python tools/walkthrough/walk.py tools/walkthrough/sample.walk
python tools/walkthrough/walk.py my.walk --out build/my.gif --delay-cs 20 --scale 3
```

The tool needs Python and a C++ compiler — `g++` or `clang++`, with `CC` or `CXX` choosing another.
The first run compiles into `tools/walkthrough/build/`, which is ignored; a later run compiles only
what changed. The GIF goes to `tools/walkthrough/build/<script>.gif` unless `--out` names a path.

Five walks ship beside the tool, and each file's header says what it shows:

- `sample.walk` — the tour: a run on the home page, the browser over it, and a routine launched with
  settings of its own.
- `home.walk` — the target stepped, started, left running and resumed.
- `settings.walk` — the device's settings list edited and saved through every row.
- `settings-cancel.walk` — the same list with each change thrown away.
- `units.walk` — a target set in kPa, the display unit changed to inHg from that list, and the home
  page read back in the new unit.

## The command file

A command file is one command per line, run top to bottom. A line that begins with `#` is a
comment, and a blank line is not a command.

The whole file is read and every scene, font, mark and setting resolved before the first frame is
captured, so a mistake is a line number rather than a half-written picture:

```
[E] line 12: no button 'okay'
```

A command-file or pre-flight error exits 2 and writes nothing. A failure while the walk is running
or the GIF is written exits 1. A successful run prints the frame count, the device time and the
output path.

## Commands

| Command | What it does |
| --- | --- |
| `load <scene> [key=value;…]` | Puts a page on top: `home`, `browser`, or a routine by its key |
| `send <gesture> [<n> s\|ms]` | Presses the button, marked on the picture for the time given — a second by default |
| `wait <n> s\|ms` | Advances device time, rounded up to whole 250 ms beats |
| `advance <n> s\|ms` | The same device time with no frames kept for it |
| `set-rate <pull> <vent>` | Sets the model chamber's pull at full duty and its venting rate, in kPa/s |
| `set-font <design>` | Sets the design a page that names no family draws in |
| `set-note <text>` | Writes a line in the band under the glass |
| `clear-note` | Blanks the band |
| `emit` | Writes the frames as the GIF and ends the walk |

### load

`load home` returns to the panel's root. `load browser` opens the scene the image sets as its
browser. Any other name is a routine's key, as the browser lists it.

A routine can be launched with settings of its own, each pair separated by a semicolon:

```
load interval set_count=3;target_pressure=30;work_duration=60
```

The set starts from the routine's own document, and each pair overrides one key through the row's
own parse and accept, so a walk and a document cannot disagree about what a value means. Every key
has to be one the routine declares and every value one its row accepts. `home` and `browser` take
no settings.

### send

One gesture, spelled as the panel's classifier spells it: `ok:single`, `up:double`,
`cancel:single`, `forward:double+ok:single`. The six buttons are `up`, `down`, `forward`, `back`,
`ok` and `cancel`, and the form after the colon decides what the mark carries: `single` draws one
outline, `double` draws two, and a count such as `cancel:7` draws one outline and writes the seconds
under the glyph. Gestures joined with `+` are one chord, and every button it names is marked.

```
send ok:single            # marked for a second
send up:double 2 s        # marked for two
send cancel:single 750 ms # and for three quarters
```

The press is held on the picture for `--lead-beats` beats before the page is handed it, and the mark
stays for the time the line gives it, which starts at the line — so the frames show the button down,
then the page's answer, then the buttons bare again. A send with no time marks for a second. Two
presses closer together than the mark still read as two: the second takes the picture over and its
own clock starts there.

The gesture is checked against the panel before the capture starts: a button the panel does not have
is an error with its line number.

### wait

```
wait 500 ms
wait 2 s
```

Device time advances in 250 ms beats, so a wait rounds up: it advances at least as far as the line
asked and never more than one beat past it. The unit is required, and `wait 0 s` is allowed.

### advance

```
advance 10 s
advance 750 ms
```

The same device time a wait would move, with nothing kept for it: the machine, the timers and the
page run exactly as they would under a wait, and the flushes in that period are drawn to the glass
but not recorded. The next frame the walk draws shows the state the advance arrived at, so a run
left to climb for ten seconds costs one frame rather than forty.

```
send ok:single
wait 1 s
advance 10 s
set-note the reading has arrived
wait 1 s
```

Nothing about the device is skipped — only the picture. Use `wait` where the change is what the walk
is showing and `advance` where the walk wants the GIF to move on.

### set-rate

The model chamber's pull at full duty and its venting rate, in kPa per second. Both have to be above
zero.

```
set-rate 20 45
```

### set-font

The design every leaf that names no family draws in. The image ships `ttyp0`, `profont`, `x11` and
`adafruit`; a name the image does not answer for is an error with its line number.

```
set-font profont
```

A page that names its own family keeps it. The firmware composition sets the panel's design once on
a device, so a walk that wants the device's look sets it rather than relying on a default.

### set-note and clear-note

`set-note` writes the rest of the line in the band under the glass, drawn in the built-in face and
centred while it fits. A note wider than the picture is clipped at the far edge.

```
set-note waiting for the vacuum to settle
clear-note
```

The band is part of every frame, so clearing a note leaves it empty rather than making the picture
shorter. Like a mark, a note lands on the next captured frame and stands until it is cleared.

### emit

`emit` writes the frames as the GIF and ends the walk. The file has to name one, and nothing may
follow it: a line after `emit` is an error, as is a file that never reaches one. A walk that drew no
frames writes no picture and exits 1.

## The picture

Every frame is the panel's glass inside a dotted border, with the six buttons across the top and a
band under the glass for a note. A press marks its button for the time its send line gives it, so how
long the picture shows a press is the script's statement rather than a count of frames the tool
chose.

The picture is 130 by 100 pixels before `--scale`, and 260 by 200 at the default of 2.

## A complete walk

```
# A press that is seen, a run given time, and a note saying what is happening.
set-font ttyp0
load home
set-note starting the run
send ok:single 2 s
wait 5 s
set-note the run is holding
wait 3 s
clear-note
send cancel:single
wait 1 s
emit
```

## Options

| Option | Default | What it does |
| --- | --- | --- |
| `--data` | `firmware/data` | the data image the walk runs against |
| `--out` | `tools/walkthrough/build/<script>.gif` | where the GIF goes |
| `--delay-cs` | `25`, one beat | how long each frame plays, from 1 to 65535 centiseconds |
| `--lead-beats` | `2`, half a second | beats a press is held before the page takes it, 0 to 1000 |
| `--scale` | `2` | how much the picture is enlarged, 1 to 16 |
| `--ceiling-kpa` | `70` | the deepest depth the model chamber reaches, above zero |
| `--no-cache` | off | rebuild every object rather than the ones that changed |
| `--verbose` | off | show the compiler commands and the device log |

`--delay-cs 25` makes a walk play at the speed it was walked. `--lead-beats 0` lands a press with no
held frames, so a reader sees the panel answer without the button going down first. A `.gif` under
`tools/walkthrough/build/` is there for a human to look at, like the check's.
