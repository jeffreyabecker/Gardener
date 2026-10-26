# Routine links

The routine surface the two links serve, and what both answer to. The wire that carries the serial
side is the `routine` domain in [serial protocol](./serial-protocol.md); the characteristics that
carry the radio side are the `routine_*` fields in [bluetooth.md](./bluetooth.md). This file is the
contract they share: the names, what a launch and a command mean, and how the two links divide the
work. What a routine is — its key, its aliases, its settings declaration and the life of its scene
— is [routine.md](./ui/routine.md), and the gesture vocabulary a command uses is
[gestures.md](./gestures.md).

A command on a link is a gesture: the same whole gesture string a panel press produces, entering the
same queue the panel feeds. The two links differ in shape, so the serial line carries the whole
surface — enumerations included — and the radio carries the live surface and leaves the lists to
the serial line.

Both surfaces are the panel's. A build without a panel has no routines: the serial line answers
`err unknown_domain routine`, and the radio has none of the `routine_*` characteristics.

## Vocabulary

- A **routine key** is a routine's identity: a plain word, no `/` or `\`.
- A **root** routine is a registration; its settings live in its document's global section.
- An **alias** is a named section of a root's document: its own key and title, its own settings set,
  sharing the root's scene class, icon and settings declaration.
- A routine's **states** are the plain-word names its scene can be in, in declaration order.
- A routine's **commands** are the whole gesture strings its scene answers, in declaration order.
- The **current routine** is the routine whose scene is on top of the stack. When the top scene is
  not a routine — the home page, the browser, a settings list, an editor — there is no current
  routine.

## What a routine declares

A routine declares its states and its commands beside its settings declaration, made once where the
file registers, in the way the settings declaration is. A routine with no declared states or
commands declares none. The readout and the browser are not routines and declare nothing.

## The surface

**List.** A link lists every routine and alias in registration order, each alias following its
routine, each marked `root` or `alias` and, for an alias, the root it resolves to.

**States and commands.** A link reads a routine's declared states and its declared commands. A
command is the whole gesture string to send back; it is not a new name the routine has to learn.

**Settings.** A link reads a routine's settings declaration — each setting's key, kind and default —
so a controller knows what a launch may name.

**Current routine.** A link reads the key of the routine on top and its current state. An alias
answers its own key, not the root it resolves to. When there is no current routine, both read `-`.

**Launch.** A link opens a routine from its stored set, or from named settings over that set. A
launch vents the machine first — a routine may only begin from a released chamber — and a launch
from named settings is for that visit only: nothing is written to the routine's document.

**Command.** A link sends the current scene one whole gesture string. It enters the queue the panel
feeds, so the device's reserved gestures are taken first and a gesture the scene showing does not
answer is dropped; acceptance is into the queue, not an act by the scene.

## Serial and radio

The serial line carries the whole surface; its verbs, replies and errors are the `routine` domain in
[serial protocol](./serial-protocol.md). The radio is a table of fixed fields, so it carries the
live surface — the current routine and its state, a launch by key, and a command to the current
scene — and leaves enumeration, and a launch from settings a client names, to the serial line. Its
characteristics and result bytes are in [bluetooth.md](./bluetooth.md).

## Rules

1. A key, a state name and a setting key are plain words; a command is a whole gesture string.
   Nothing on either link takes a value with a space.
2. An alias is addressed by its own key everywhere — enumeration, launch and the current-routine
   read — and the read answers the alias key, not its root.
3. Launching vents first, whatever link asks. There is no launch that leaves a run going behind the
   routine.
4. A launch from named settings never writes the routine's document; the stored set is unchanged by
   any launch.
5. The device's reserved gestures are taken before any link-sent command, exactly as they are before
   a panel press, so sending one over a link has the device's reserved effect rather than the
   routine's.
