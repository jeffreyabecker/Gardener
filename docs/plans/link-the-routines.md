# Link the routines

The routine surface both links serve — the serial `routine` domain and the eight `routine_*`
characteristics — is specified in [routine-links.md](../specs/routine-links.md) and implemented
nowhere. Behind it, what it advertises does not exist either: a routine registers a key, a title, an
icon, a scene class and a settings declaration, and nothing that names a state, a command or a
setting's kind. This plan makes that data exist and makes it readable from the links without a link
ever reading the interpreter. It does not build the two surfaces: the serial domain and the
characteristic table are the work that consumes this, and until they land nothing new is advertised.

The constraint the plan is built around is the interpreter's. One VM is driven from the panel task,
once a beat, with no lock and no second core, so a link task that asked the VM a question would be a
second writer of an interpreter that has no synchronisation. The data therefore leaves the VM once,
at boot, before the link tasks are spawned, and is held in C++ in the wire's own shape: a fixed
table of routines, states, commands and settings that is read by both surfaces and never written
again. Because one table feeds the serial rows and the radio payloads, the two cannot disagree about
an order or a value.

## A routine declares its states and its commands

Today `Registration` in `/system/registration.be` holds a key, a title, an icon, a scene class, the
settings it edits and the alternates its document named. Nothing names a state or a command, no
scene can say where it is, and the registry answers `keys`, `title`, `settings_cfgs` and `icon` and
nothing about either.

The record carries two more lists. A command is a plain-word name and the line the panel draws; a
state is a plain-word name and the commands valid from it, named by their command names. Both are
optional and empty when a routine declares neither, so every routine file in the tree keeps
registering and running unchanged. The check that a state names only commands its routine declares
happens where the record is built: a name nothing declares is dropped with one line on the
diagnostics stream, the way a setting kind that cannot work is left out today. The registry answers
`states(key)` and `commands(key)` in declaration order, an alias answering its root's lists.

A state carries at most 16 commands, because the radio's state payload is 50 bytes and carries
their indexes two at a time; the seventeenth is cut, with one line at boot, and the serial row shows
what the radio holds.

It is checked by registering the three shipped routines with their declarations and confirming the
browser still lists and launches them, and by a harness case that reads each routine's states and
commands and finds the declared names in declaration order, with an alias answering its root's
lists.

## A setting says which kind it is

A setting's kind is its class today — `NumericEditCfg`, `ChoiceEditCfg`, `PressureEditCfg` and the
rest —
so no link can name it as the wire does.

Every setting answers one of six names: `bool`, `choice`, `real`, `int`, `pressure`, `duration`. The
name belongs to the kind rather than the class, so a class extending a kind answers the kind. A
setting whose class answers no name is left out of the declaration with one line, because no link
could describe it.

It is checked by each of the six kinds' configs reporting its own name, and the device's jingle row
reporting `choice`.

## A scene reports the state it is in

No scene answers `current_state()`, so a `get` or a `routine_current` read has nothing behind it.

While a routine's scene is showing, it answers `current_state()` with one of the state names its
registration declares, and a routine that declares no states answers nothing. The device's own
scenes — home, the browser, a settings list, an editor — are not routines and are never asked. The
three shipped routines answer from the phase their machine is already in, so the state a link reads
is the state the page draws.

It is checked by a harness case that pushes each shipped routine, steps its machine through every
phase and reads the name the scene answers, and by the browser and the home scene answering none.

## No link reads the interpreter

There is nothing to read today, and the shortest implementation — a link asking the registry — is
unsafe, because the interpreter is entered from the panel task only.

The routine data is copied out of the interpreter once, at boot, before the serial and radio tasks
exist, into a table no code writes afterwards. A link reads that table from its own task with no
lock, no queue and no second copy to keep in step. The table's order is the specification's:
routines in registration order with each alias directly after its routine, and states, commands and
settings in declaration order; an alias carries its own key and title and its root's counts.

The table carries what the radio can hold — 24 bytes for a key, a parent, a title, a command name, a
command label, a setting key or a setting label; 16 for a state name or a choice key; 16 commands in
one state's list; 4 choice keys in one setting. Anything wider is cut to its field with one line on
the diagnostics stream at boot, so no item is silently missing from one link and present on the
other.

It is checked by a link domain that answers from the table compiling and the panel task continuing to
beat with it registered, by a boot log with nothing cut for declarations inside the fields, and by
one cut line for a declaration wider than its field.

## The current routine and its state are readable without asking the interpreter

Nothing publishes which routine is on top or which state it is in.

The panel task publishes the routine's index in the table and its state's index once a beat, as one
value, so a reader takes both or neither. When the top scene is not a routine, both are unset. When
a routine declares no states, or its scene answers a name it does not declare, the routine index is
real and the state is unset — with one line, on the change, for the undeclared name. A routine that
is showing but was never registered is unset too, with one line.

It is checked by a harness case that launches a routine and, after a beat, reads the pair it names;
by the browser being pushed and the pair reading unset; and by a page driven to each of its states
and the state index moving with it.

## The table renders as each surface needs it

Nothing renders the table. Both surfaces will need it, and building either one against an unshaped
table is how the two drift apart.

Two renderers over the table, each in its own wire's form: the text the serial rows are printed from,
and the fixed payloads the characteristics answer with. Both are pure functions of the table, so both
compile and are asserted on the host and neither reaches a VM, a stream or a radio.

The text is the rows [routine-links.md](../specs/routine-links.md) defines, with three rules the
grammar leaves implicit: a space in a label or a choice key is written `_`, a nullable setting
holding nothing prints `-`, and a number prints by its kind — a pressure with one decimal, an int, a
duration and a bool as text, a step as a whole number. The bytes are the payloads the radio's rows
define, with strings cut and NUL-padded, a number written by its kind, and the flags naming which of
the default, bounds and choices are present.

It is checked by a harness case that reads the rows of the routines in the image and compares them
line for line, and by a payload of each of the four read as bytes with its length, its kind byte and
its flags byte asserted.

## Consequences

[Serial and radio](../specs/routine-links.md) — Rule 1 gains the field widths and the cut to them,
and the rows for `get` and `routine_current` gain the routine that declares no states: a real routine
index and no state.

[Bindings](../specs/ui/bindings.md) — `Command` and `RoutineState` as records a routine file builds,
the two new `Registration` arguments and fields, the kind every setting config carries, and the
registry's `states()` and `commands()`, with the rule that a declaration naming what it does not
declare loses that name at registration.

[Routines](../specs/ui/routine.md) — the declaration's three lists, and `current_state()` among what
a scene answers.

The surfaces themselves are unchanged by this plan. Until their domains exist, a client on either
link sees exactly what it sees today.

## Deferred

The serial `routine` domain — its verbs, its rows, `err not_ready full` and the named settings a
launch accepts — and the eight `routine_*` characteristics with their write-select slots and
`routine_current`'s notify. Two limits belong to that work and are recorded here so they are not met
late: the link's field table is full at 16 characteristics with the machine's and the panel's
already registered, and its subscription mask is a 16-bit word.

Everything inbound: launching vent-first, a launch's named settings held to their kind and repaired
by the routine's rule, and a command posted as `cmd:<name>` into the gesture queue. It all runs on
the panel task and needs a request path from a link into it, and none of it is a read of the
interpreter.

The two surfaces on a device, which is the parking lot's "Verify the two builds on hardware".

## How it is known to hold

`python tools/berry/check.py --tests` — the image loads with the declarations, the three routines
still drive, and the harness cases above assert the table, the rows, the payloads and the pair.

`python scripts/arduino.py --task build` — once, for the C++ and its guards.

On a device, later: the exchanges the examples in [routine-links.md](../specs/routine-links.md) show.
