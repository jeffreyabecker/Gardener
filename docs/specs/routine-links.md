# Routine links

The routine surface the two links serve, and what both answer to: what a routine exposes, how a
link reads it, opens a routine and sends it a command. The serial side is the `routine` domain
below, the radio side a table of `routine_*` characteristics that answer the same registrations an
item at a time, and both answer for the same routines. The line grammar and the error
codes are [serial protocol](./serial-protocol.md)'s, the radio table they join is
[bluetooth.md](./bluetooth.md)'s, what a routine is — its key, its aliases, its settings declaration
and the life of its scene — is [routine.md](./ui/routine.md), and the gesture queue a command and a
press share is [gestures.md](./gestures.md).

A command on a link is a named action a routine declares. Sending it posts `cmd:<name>` into the
queue the panel feeds, so the routine's scene takes it on the same drain a press arrives on — but a
command is not a gesture. A gesture is one of the six keys with the form of its press — `ok:single`,
`cancel:2` — joined by `+`, and no key is named `cmd`, so nothing beginning `cmd:` is a gesture. The
prefix is what tells them apart: a token carrying `cmd:` is a command, and anything else on the
queue is a press. What differs between the links is the spelling: the serial line asks for a routine
by key and takes the reply as text, and the radio asks by index and takes a small fixed payload.

Both surfaces are the panel's. A build without a panel has no routines: the serial line answers
`err unknown_domain routine`, and the radio has none of the `routine_*` characteristics.

## Vocabulary

- A **routine key** is a routine's identity: a plain word, no `/` or `\`.
- A **root** routine is a registration; its settings live in its document's global section.
- An **alias** is a named section of a root's document: its own key and title, its own settings set,
  sharing the root's scene class, icon and settings declaration.
- A routine's **states** are the plain-word names its scene can be in, in declaration order; each
  carries the commands valid from it.
- A routine's **commands** are its named actions, in declaration order; a command carries a name and
  a label, is not a gesture, and its token on the queue is `cmd:<name>`.
- A routine's **settings** are its settings declaration: each setting's key, kind, default and the
  kind's own bounds or choices.
- The **current routine** is the routine whose scene is on top of the stack. When the top scene is
  not a routine — the home scene, the browser, a settings list, an editor — there is no current
  routine.

## What a routine exposes

A routine's settings declaration — the instance it registers — carries three lists: `settings_cfgs`,
the settings it edits, `states`, the names its scene can be in and the commands valid from each, and
`commands`, one `Command(name, label)` per action its scene answers. A command's name is what a link
sends and what the scene receives as `cmd:<name>`, and its label is the line the panel draws for it.
The declarations are made once, where the file registers. A routine with none of one declares an
empty list. Its scene reports its current state through `current_state()`, one of the declared
names, while it is showing; there is no current state when it is not. The home scene and the browser
are not routines and expose nothing.

## Serial — the `routine` domain

Served by a build with a panel; a build without one answers `err unknown_domain routine`, the same
as `fs` and `buzzer`. The line grammar is [serial protocol](./serial-protocol.md)'s — one reply per
command, `key=value` items, and a body of rows ended by a blank line where the reply defines one.

| Verb | Reply | Notes |
| --- | --- | --- |
| `list` | `ok`, then rows, then a blank line | one `key=<key> kind=<kind> parent=<parent>` per routine and alias, in registration order, each alias after its routine; a root's `parent` is `-` |
| `states <key>` | `ok`, then rows, then a blank line | one `state=<name> commands=<c1>,<c2>,…` per state, in order, with the commands valid from it in command declaration order; `-` when none is; no rows when it declares none |
| `commands <key>` | `ok`, then rows, then a blank line | one `name=<name> label=<label>` per command, in order; no rows when it declares none |
| `settings <key>` | `ok`, then rows, then a blank line | one `key=<key> label=<label> kind=<kind> default=<default>` per setting, in order, with the kind's bounds or choices |
| `get` | `ok key=<key> state=<state>` | the routine on top and its state; `key=- state=-` when no routine is showing |
| `launch <key> [<setting>=<value> …]` | `ok key=<key>` | open a routine from its stored set, or with the named values in their place |
| `command <name>` | `ok command=<name>` | post `cmd:<name>` into the queue the panel feeds |

`list` rows are `key=<key> kind=<kind> parent=<parent>`: `kind` is `root` or `alias`, and `parent`
is the root an alias resolves to, `-` for a root.

`states` rows are `state=<name> commands=<c1>,<c2>,…` — the state's name and the commands valid from
it, in command declaration order, `-` when none is — and a routine that declares none answers `ok`
then the blank line. A state's row is the whole of what the routine answers from it: the scene
answers the commands its current state lists and drops the rest. `commands` rows are
`name=<name> label=<label>` — the command's name and the line the panel draws for it, each space in
the label written `_` as a setting's label is — and a routine that declares none answers `ok` then
the blank line.

`settings` rows are `key=<key> label=<label> kind=<kind> default=<default>` plus the kind's own
fields: `min=` and `max=` for a real, int or duration, `step=` for any kind that steps,
`values=<k1>,<k2>,…` for a choice, and `nullable=on` when the setting may hold nothing. `label` is
the line the panel draws beside the value, with each space written `_` so the row still splits on
spaces. `kind` is `bool`, `choice`, `real`, `int`, `pressure` or `duration`; `default` is the
setting's default in the wire form — a bool is `on` or `off`, a pressure is kPa, a duration is whole
seconds, and a nullable setting that holds nothing is `-`. A choice's keys are plain words with each
space written `_`, the same rule as any token; a choice whose keys are not is not servable. The
kinds and their fields are [bindings.md](./ui/bindings.md).

`get` reads the routine on top. When the top scene is not a routine — the home scene included — the
key and the state are unset: the value a client holds is the empty string, printed `-` on the wire.

`launch` vents the machine before it opens the routine, and a launch from named settings is for
this visit only: values are held to their kind and the set repaired by the routine's rule exactly
as a stored set is, and nothing is written to the routine's document.

`command` takes a command name the routine declares and posts `cmd:<name>` into the same queue the
panel feeds, so the scene takes it on the same drain as a press. It is not a gesture: nothing about
it is classified, no reserved gesture applies, and a command name is a plain word, so `cmd:<name>`
carries one `:` and can be neither a gesture nor a reserved one. A command the scene showing does
not answer — one its current state does not list — is dropped. `ok` is acceptance into the queue,
not an act by the scene.

    -> routine list
    <- ok
    <- key=ramped_static kind=root parent=-
    <- key=ramped_interval kind=root parent=-
    <- key=interval kind=root parent=-
    <- (blank line)
    -> routine states ramped_static
    <- ok
    <- state=ready commands=settings,start,quit
    <- state=approach commands=settings,stop,quit
    <- state=run commands=settings,stop,quit
    <- state=paused commands=settings,start,quit
    <- (blank line)
    -> routine commands ramped_static
    <- ok
    <- name=settings label=Settings
    <- name=start label=Start
    <- name=stop label=Stop
    <- name=quit label=Quit
    <- (blank line)
    -> routine settings ramped_interval
    <- ok
    <- key=set_count label=Sets kind=int default=4 min=1 max=20 step=1
    <- key=initial_pressure label=Start kind=pressure default=5.0 step=1
    <- key=final_pressure label=Final kind=pressure default=20.0 step=1
    <- key=step_pressure label=Step kind=pressure default=- step=1 nullable=on
    <- key=rest_pressure label=Rest kind=pressure default=- step=1 nullable=on
    <- key=work_duration label=Work kind=duration default=300 min=0 max=3600 step=30
    <- key=rest_duration label=Rest_time kind=duration default=45 min=0 max=1200 step=1
    <- key=resume_behavior label=Resume kind=choice default=immediately values=immediately,after_ok
    <- (blank line)
    -> routine get
    <- ok key=- state=-
    -> routine launch ramped_static target_pressure=55.0 total_duration=120
    <- ok key=ramped_static
    -> routine get
    <- ok key=ramped_static state=ready
    -> routine command start
    <- ok command=start

## Errors

The domain uses [serial protocol](./serial-protocol.md)'s error codes. A routine key or command
name nothing declares is `err not_found <key>` or `err not_found <name>`; a named setting the
routine does not declare, or a value its kind refuses, is `err bad_argument <key>`; and a launch
whose push the stack cannot take is `err not_ready full`. A build without a panel answers
`err unknown_domain routine` to every verb. The radio has no error reply: a write naming an index
nothing declares is ignored, and the characteristic keeps the answer it had.

## Bluetooth

The radio is not the serial line over a pipe: a client is a GATT host, so the surface is a table of
small characteristics and a list is browsed an item at a time rather than received as one body. Five
characteristics describe what the panel registered — the count, then one routine, state, command or
setting each time a client asks — and three more drive the panel. All eight are the panel's; a build
without a panel has none of them:

| Characteristic | Suffix | Properties | Length | Payload |
| --- | --- | --- | --- | --- |
| `routine_count` | `0018` | read | 2 | uint16 LE, how many routines and aliases are registered |
| `routine_spec` | `0019` | write, read, notify | 78 | in `routine_index`; out `key`, `parent`, `title`, `state_count`, `command_count`, `settings_count` |
| `routine_state` | `001A` | write, read, notify | 50 | in `routine_index` and `state_index`; out `state_name`, `command_count`, then the indexes of the commands that state accepts |
| `routine_command` | `001B` | write, read, notify | 48 | in `routine_index` and `command_index`; out `command_name`, `command_label` |
| `routine_setting` | `001C` | write, read, notify | 131 | in `routine_index` and `setting_index`; out the setting's key, label, kind, flags and bounds |
| `routine_current` | `001D` | read, notify | 4 | uint16 LE `routine_index` and `state_index`, `0xFFFF` each when no routine is showing |
| `routine_launch` | `001E` | write | 2 | uint16 LE `routine_index`, opened from its stored set |
| `routine_send` | `001F` | write | 2 | uint16 LE `command_index`, sent to the routine on top |

An index is `uint16 LE` and counts from zero in the order the serial enumeration lists the same
items: routines in registration order with each alias after its routine, and states, commands and
settings in declaration order. A write is exactly the two or four bytes its row names — one index,
or two — and a short or long one is ignored rather than padded. A client writes the indexes and then
reads — or takes the notification of — the answer that write selects, which stays until another
write selects another item.

The reply is a fixed head and, where a row defines one, a list that runs to its end:

| Field | Width | Meaning |
| --- | --- | --- |
| `key` | 24 | a routine's key, UTF-8 and NUL-padded |
| `parent` | 24 | the root an alias resolves to, empty for a root |
| `title` | 24 | the title the panel draws, empty when it has none |
| `state_name` | 16 | a state's name |
| `command_name` | 24 | a command's `name=` |
| `command_label` | 24 | a command's `label=` |
| setting `key` | 24 | a setting's `key=` |
| setting `label` | 24 | the line the panel draws beside the value |
| choice key | 16 | one of a choice's keys as the declaration spells it, a space included |

`routine_spec` answers `key`, `parent` and `title`, then three `uint16 LE` counts — the states,
commands and settings the routine declares — so the payload is always 78 bytes. `routine_state`
answers `state_name`, then `command_count` and that many `command_index`, the commands the state
accepts in declaration order, so the payload is `18 + 2n` bytes and a state lists at most 16.
`routine_command` answers `command_name` and `command_label`.

`routine_setting` answers the key and the label, one byte of `kind` — `0` bool, `1` choice, `2`
real, `3` int, `4` pressure, `5` duration, the serial kinds in that order — one byte of `flags`,
then `default`, `min`, `max` and `step` as four bytes each, one byte of `choice_count`, and that
many 16-byte choice keys, so a choice declares at most four. A four-byte value is read by its kind:
`int32` for an int, `uint32` whole seconds for a duration, `float32` for a real or a pressure, `0`
or `1` for a bool, and an index into the choice keys for a choice. `flags` bit 0 is nullable, and
bits 1 to 5 say `default`, `min`, `max`, `step` and the choice keys are present; an absent value is
four zero bytes and is not to be read.

`routine_current` is the radio's `get`: the routine on top and the state it is in, as two indexes,
`0xFFFF` in both when the top scene is not a routine. It notifies, so a client that subscribes
learns of a launch, a quit or a state change without polling.

`routine_launch` opens the routine the index names from its stored set; a visit's own values are the
serial line's, because two bytes have nowhere to put them. `routine_send` is the serial `command` by
index: it posts the command's `cmd:<name>` into the queue the panel feeds, for the routine on top,
and a command the current state does not list is dropped. Neither write notifies: what a client
watches for the outcome is `routine_current`.

## Rules

1. A key, a state name, a command name, a setting key and a choice's keys are plain words. Nothing
   on either link takes a value with a space.
2. An alias stands for itself everywhere — enumeration, launch and the current-routine read — so a
   client that names it gets its own key and its own counts, never its root's.
3. Launching vents first, whatever link asks. There is no launch that leaves a run going behind the
   routine.
4. A launch from named settings never writes the routine's document; the stored set is unchanged by
   any launch.
5. A command is sent by its declared name and reaches the scene as `cmd:<name>`. It is not a
   gesture: the reserved gestures are the device's and are no routine's commands, and a plain-word
   name behind `cmd:` spells nothing the classifier can produce. The `commands` reply lists a name
   and a label — a command has no gesture to report.
6. A state's row is the whole of what the routine answers from it: a command its current state does
   not list is dropped.
