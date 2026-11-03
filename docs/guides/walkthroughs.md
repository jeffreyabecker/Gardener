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

Four walks ship beside the tool, and each file's header says what it shows:

- `sample.walk` — the tour: a run on the home page, the browser over it, and a routine launched with
  settings of its own.
- `home.walk` — the target stepped, started, left running and resumed.
- `settings.walk` — the device's settings list edited and saved through every row.
- `settings-cancel.walk` — the same list with each change thrown away.

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
| `send <gesture>` | Holds the gesture's button for `--lead-beats`, then hands it to the page |
| `show <gesture>[, <gesture>…]` | Marks the named buttons and leaves the marks standing |
| `clear [button[, button]…]` | Takes the marks off the named buttons; with no button named, off every one |
| `wait <n> s\|ms` | Advances device time, rounded up to whole 250 ms beats |
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
`cancel:single`, `forward:double+ok:single`. The press is held on the picture for `--lead-beats`
beats before the page is handed it.

`send` marks nothing by itself. A walk that wants the button seen writes a `show` before it and a
`clear` after the wait:

```
show ok:single
send ok:single
wait 3 s
clear ok
```

### show and clear

The six buttons across the top of the picture are `up`, `down`, `forward`, `back`, `ok` and
`cancel`.

`show` takes gestures and marks each button the gesture names. The form after the colon decides
what the mark carries: `single` draws one outline, `double` draws two, and a count such as
`cancel:7` draws one outline and writes the seconds under the glyph. Gestures joined with `+` are
one chord, and every button it names is marked.

`clear` takes plain button names, since which form was marked does not matter when it is taken
away. `clear` with nothing else takes every mark away.

A mark is redrawn with every frame, so it lands on the next captured frame and stands until a
`clear` takes it away. Showing a button again replaces its mark, so `show ok:single` followed by
`show ok:7` drops the first and leaves the hold.

Both lines are checked against the panel before the capture starts: a button the panel does not
have is an error with its line number.

### wait

```
wait 500 ms
wait 2 s
```

Device time advances in 250 ms beats, so a wait rounds up: it advances at least as far as the line
asked and never more than one beat past it. The unit is required, and `wait 0 s` is allowed.

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
band under the glass for a note. The marks on the buttons are the walk's own, so how long a press is
shown is the script's statement rather than a count of frames the tool chose.

The picture is 130 by 100 pixels before `--scale`, and 260 by 200 at the default of 2.

## A complete walk

```
# A press that is seen, a run given time, and a note saying what is happening.
set-font ttyp0
load home
show ok:single
set-note starting the run
send ok:single
wait 5 s
clear ok
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
