# Routine links

The routine surface the two links serve, and what both answer to: what a routine exposes, how a
link reads it, opens a routine and sends it a command. The serial side is the `routine` domain
below, the radio side the `routine_query` and `routine_reply` characteristics, and the two carry the
same surface. The line grammar and the error codes are [serial protocol](./serial-protocol.md)'s,
the radio table they join is [bluetooth.md](./bluetooth.md)'s, what a routine is — its key, its
aliases, its settings declaration and the life of its scene — is [routine.md](./ui/routine.md), and
the gesture vocabulary a command routes through is [gestures.md](./gestures.md).

A command on a link is a named action a routine declares. Sending it routes the gesture the command
names into the queue the panel feeds, so the routine's scene receives it exactly as it would a
panel press. The two links carry the same surface: the serial line as the `routine` domain's lines,
the radio as the same lines written to and read from a control point, because a list of routines has
no fixed field.

Both surfaces are the panel's. A build without a panel has no routines: the serial line answers
`err unknown_domain routine`, and the radio has neither `routine_query` nor `routine_reply`.

## Vocabulary

- A **routine key** is a routine's identity: a plain word, no `/` or `\`.
- A **root** routine is a registration; its settings live in its document's global section.
- An **alias** is a named section of a root's document: its own key and title, its own settings set,
  sharing the root's scene class, icon and settings declaration.
- A routine's **states** are the plain-word names its scene can be in, in declaration order.
- A routine's **commands** are its named actions, in declaration order; each command carries the
  whole gesture string that delivers it.
- A routine's **settings** are its settings declaration: each setting's key, kind, default and the
  kind's own bounds or choices.
- The **current routine** is the routine whose scene is on top of the stack. When the top scene is
  not a routine — the home scene, the browser, a settings list, an editor — there is no current
  routine.

## What a routine exposes

A routine's settings declaration — the instance it registers — carries three lists: `settings_cfgs`,
the settings it edits, `states`, the names its scene can be in, and `commands`, one `Command(name,
gesture)` per action its scene answers. The declarations are made once, where the file registers. A
routine with none of one declares an empty list. Its scene reports its current state through
`current_state()`, one of the declared names, while it is showing; there is no current state when it
is not. The home scene and the browser are not routines and expose nothing.

## Serial — the `routine` domain

Served by a build with a panel; a build without one answers `err unknown_domain routine`, the same
as `fs` and `buzzer`. The line grammar is [serial protocol](./serial-protocol.md)'s — one reply per
command, `key=value` items, and a body of rows ended by a blank line where the reply defines one.

| Verb | Reply | Notes |
| --- | --- | --- |
| `list` | `ok`, then rows, then a blank line | one `key=<key> kind=<kind> parent=<parent>` per routine and alias, in registration order, each alias after its routine; a root's `parent` is `-` |
| `states <key>` | `ok states=<s1>,<s2>,…` | the routine's declared states, in order; `-` when it declares none |
| `commands <key>` | `ok`, then rows, then a blank line | one `name=<name> gesture=<gesture>` per command, in order; no rows when it declares none |
| `settings <key>` | `ok`, then rows, then a blank line | one `key=<key> kind=<kind> default=<default>` per setting, in order, with the kind's bounds or choices |
| `get` | `ok key=<key> state=<state>` | the routine on top and its state; `key=- state=-` when no routine is showing |
| `launch <key> [<setting>=<value> …]` | `ok key=<key>` | open a routine from its stored set, or with the named values in their place |
| `command <name>` | `ok command=<name>` | route the named command's gesture into the queue the panel feeds |

`list` rows are `key=<key> kind=<kind> parent=<parent>`: `kind` is `root` or `alias`, and `parent`
is the root an alias resolves to, `-` for a root.

`states` answers a comma-separated list in one value; a routine that declares none answers `-`, not
an error. `commands` rows are `name=<name> gesture=<gesture>` — the command's name and the whole
gesture string it is routed as — and a routine that declares none answers `ok` then the blank line.
`settings` rows are `key=<key> kind=<kind> default=<default>` plus the kind's own fields: `min=` and
`max=` for a real, int or duration, `step=` for any kind that steps, `values=<k1>,<k2>,…` for a
choice, and `nullable=on` when the setting may hold nothing. `kind` is `bool`, `choice`, `real`,
`int`, `pressure` or `duration`; `default` is the setting's default in the wire form — a bool is
`on` or `off`, a pressure is kPa, a duration is whole seconds, and a nullable setting that holds
nothing is `-`. A choice's keys are plain words, the same rule as any token; a choice whose keys
are not is not servable. The kinds and their fields are [bindings.md](./ui/bindings.md).

`get` reads the routine on top. When the top scene is not a routine — the home scene included — the
key and the state are unset: the value a client holds is the empty string, printed `-` on the wire.

`launch` vents the machine before it opens the routine, and a launch from named settings is for
this visit only: values are held to their kind and the set repaired by the routine's rule exactly
as a stored set is, and nothing is written to the routine's document.

`command` takes a command name the routine declares and routes the gesture it names into the same
queue the panel feeds — reserved gestures first, and a gesture the scene showing does not answer is
dropped. `ok` is acceptance into the queue, not an act by the scene.

    -> routine list
    <- ok
    <- key=ramped_static kind=root parent=-
    <- key=ramped_interval kind=root parent=-
    <- key=interval kind=root parent=-
    <- (blank line)
    -> routine states ramped_static
    <- ok states=ready,approach,run,paused
    -> routine commands ramped_static
    <- ok
    <- name=settings gesture=up:double
    <- name=start gesture=ok:single
    <- name=stop gesture=cancel:single
    <- name=quit gesture=cancel:double
    <- (blank line)
    -> routine settings ramped_interval
    <- ok
    <- key=set_count kind=int default=4 min=1 max=20 step=1
    <- key=initial_pressure kind=pressure default=5.0 step=1
    <- key=final_pressure kind=pressure default=20.0 step=1
    <- key=step_pressure kind=pressure default=- step=1 nullable=on
    <- key=rest_pressure kind=pressure default=- step=1 nullable=on
    <- key=work_duration kind=duration default=300 min=0 max=3600 step=30
    <- key=rest_duration kind=duration default=45 min=0 max=1200 step=1
    <- key=resume_behavior kind=choice default=immediately values=immediately,after_ok
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
`err unknown_domain routine` to every verb.

## Bluetooth

The radio carries the same surface through a control point, because the surface is lists and text
and a table of fixed fields cannot hold them. Two characteristics, both the panel's — a build
without a panel has neither:

| Characteristic | Suffix | Properties | Length | Payload |
| --- | --- | --- | --- | --- |
| `routine_query` | `0018` | write | 128 | UTF-8, the `routine` command without its domain token |
| `routine_reply` | `0019` | read, notify | 512 | UTF-8, the reply, rows and blank lines included |

The client writes the command to `routine_query` and reads the reply from `routine_reply`, verbatim:
the `ok` or `err` line, then the rows and the blank line where the command defines a body. `get`,
`launch` with named settings, and every enumeration verb work the same as on the serial line.

`routine_query` is text, so a write is however much is written, up to 128 bytes; a longer write is
ignored. `routine_reply` is at most 512 bytes; the routine surface is small enough that no reply
exceeds it, and the serial line is the unbounded surface for the same commands. `routine_reply`
notifies when a reply is written, so a client that subscribes and writes `get` receives the current
routine on change.

## Rules

1. A key, a state name, a command name, a setting key and a choice's keys are plain words; a
   command's gesture is a whole gesture string. Nothing on either link takes a value with a space.
2. An alias is addressed by its own key everywhere — enumeration, launch and the current-routine
   read — and the read answers the alias key, not its root.
3. Launching vents first, whatever link asks. There is no launch that leaves a run going behind the
   routine.
4. A launch from named settings never writes the routine's document; the stored set is unchanged by
   any launch.
5. A command is sent by its declared name; the reserved gestures are the device's and are no
   routine's commands.
