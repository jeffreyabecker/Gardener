# Spec 06 — Serial protocol: program commands

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §11 and the wire
half of constraint 2. Written in the proto 2 spelling — resource-first commands and one reply
shape, [the protocol](../serial-protocol.md) §3 — and the `set mode` / `list` / `cat` / `rm` /
`run` / `params` / `prog` spellings this spec first carried are superseded; §7 maps them.

## 1. Principles

- Every command produces exactly one reply line. The exceptions are pages (`help`, `store list`,
  `store cat`), which are one `ok` line, then rows, then a single `end` line.
- `panel draw` / `panel flush` stay fire-and-forget (validated locally, replied immediately).
- Commands call the plant directly; the reply reflects the result of that call, so `err`
  codes remain authoritative. A command that would change what is running is applied at the
  program's yield point rather than inside its stack (spec 01 §5); reads are answered where
  they arrive.
- A refusal is never only a log line. `device log=off` is a legitimate thing for a controller
  to do, so whatever a program command refuses, it says on the reply line (§5).

## 2. Commands

`store` is the program store; `vm` is the runtime that runs one. Neither is a `set`: the
resource tells you what is being touched, and the op tells you what is being done to it.

| Op | Reply | Notes |
| --- | --- | --- |
| `store list` | `ok result=list programs=<n>` + one row per program + `end result=list rows=<n>` | rows: `program=<name> size=<n> params=<n> default=<0\|1> running=<0\|1>`, read from the store index (spec 02 §7). `default` is first |
| `store cat <name> [offset] [len]` | `ok result=cat program=<name> offset=<n> len=<n> total=<n>` + source lines + `end result=cat rows=<n>` | a page, as the old `cat` was paged |
| `store rm <name>` | `ok result=rm program=<name>` | `store rm default` re-seeds the passive source |
| `store run <name> [k=v …]` | `ok result=run program=<name> state=<state>` / `err …` | stop current → vent → start; switches to stand-alone if it has to |
| `store params <name>` | `ok program=<name> k=v …` | effective values (persisted over defaults) |
| `store params <name> k=v …` | `ok program=<name> k=v …` | persist without running |
| `store put <name>` | `ok result=put program=<name> size=<n> crc=<n>` / `err …` | one shot: `link term` a marker first, then the source follows the first newline and runs to the marker (§6). Not chunked and not hex encoded |

`store params` is the same shape whether it is read or written, which is the point of the
no-value-is-a-read rule: one parser covers both, and the reply always reports the effective set
rather than the request.

Reads (`store list`, `store cat`, `store params <name>`) are answered where they arrive; writes
and lifecycle ops (`store run`, `store rm`, `store params <name> k=v …`, `store put`) are applied
at the program's yield point.

| Mode | Availability |
| --- | --- |
| both | every `store` op, every `vm` field, and `link term` — framing is not control, and neither is the store |
| `peripheral` | `plant` writes and actions, every `panel` op |
| `standalone` | `vm mode=peripheral` is the way out; everything else under `vm` is a read |

`vm mode` replaces the old `set mode`; `vm mode=peripheral` stops the program, vents, halts the
VM and hands the plant and panel to the host, and `vm mode=standalone` starts `default` if
nothing is running (spec 05 §2). A control command that arrives in stand-alone mode is
`err not_ready mode=standalone`, not a silent no-op: the mode is visible in the reply, so a
controller that asked for control it did not have knows why it was refused.

## 3. `store run` parameter validation

- Split `k=v` tokens on whitespace, after the name.
- Unknown key → `err bad_argument token=<key>`, and nothing starts.
- Value failing type/range validation (spec 02 §6) → `err out_of_range field=<key> value=<v>`
  plus the bound it failed: `min=`, `max=` or `choices=`.
- Validation is all-or-nothing and happens **before** the current program is disturbed: a run
  that is going to fail does not stop what is already running. The old protocol could not promise
  that, because its reply carried only a code.
- Missing required params are filled from persisted values then manifest defaults; there
  are no required-without-default params.
- A bare `store run <name>` uses persisted-then-default values.
- `store params <name> k=v …` validates the same way and persists only if every value passes.

Example:

```
-> store run pulse cycles=5
<- ok result=run program=pulse state=PULL
-> store params pulse
<- ok program=pulse cycles=5 mode=pulse beep=false
-> store params pulse cycles=10
<- ok program=pulse cycles=10 mode=pulse beep=false
-> store run pulse cycles=999
<- err out_of_range field=cycles value=999 min=1 max=100
-> store run pulse fake=1
<- err bad_argument token=fake
```

## 4. `vm get` extensions

The runtime's state is reported by `vm get`, not by the plant: it is the VM that owns the plant,
so `program`, `running` and `host` are fields of `vm`.

| Key | Meaning |
| --- | --- |
| `mode` | `standalone` / `peripheral` |
| `program` | `default`, `pulse`, … or `-` when idle |
| `running` | `0` / `1` |
| `host` | `0` / `1` |
| `s0` / `s0v` … `s7` / `s7v` | program state table (spec 07 §2), key then value. Up to 8 pairs, each side ≤ 11 characters, `-` when unused |

`help` becomes a page in the proto 2 shape, with one row per resource and then one row per field
or op. The examples are abridged:

```
-> help
<- ok result=help rows=6
<- resource=plant fields=12 actions=4
<- resource=panel fields=4 actions=2
   ... one row per resource ...
<- resource=device fields=7 actions=1
<- end result=help rows=6
-> help vm
<- ok result=help resource=vm rows=20
<- field=mode type=choice access=rw choices=standalone,peripheral
<- field=program type=text access=r
   ... one row per field, then one per action ...
<- end result=help resource=vm rows=20
```

Two consequences worth stating, because they are the reason the shape changed:

- `help` names things that can be asked for. Every resource, field, action and op a page names is
  known to the device, so a controller that enumerates `help` and replays what it finds is never
  told `err unknown_command` — which the old `set`/`draw` list could not promise.
- `help store` and `help store run` publish the same bounds the validator enforces, because both
  read the manifest (spec 02 §6). A controller does not have to hardcode `min=1 max=100`.

## 5. Error codes

The seven codes in [the protocol](../serial-protocol.md) §14 are the whole set, and each carries
items naming what was involved. The two that exist because of programs:

| Code | Meaning |
| --- | --- |
| `not_found` | `store cat` / `store rm` / `store run` / `store params` named a program that does not exist: `err not_found program=pulse` |
| `full` | a `store put` payload over `ProgramStore::kMaxBytes`, or a store with no room left: `err full program=pulse max=8192` |

The rest, as they land on these commands: `unknown_command` names a resource, op or removed
spelling; `bad_argument` names a key, a stray argument or an over-long line; `out_of_range` names
a field, its value and the bound; `not_ready` names a reason (`no_target`, `no_reading`, `latch`)
or a mode; `unsupported` names a read-only field or a feature this build does not have.

## 6. Upload framing

- The source is **not** chunked and **not** hex encoded: the client sets a terminator that cannot
  appear in the source, then sends the whole program as one command.

```
-> link term=<<<END>>>
<- ok term=<<<END>>>
-> store put pulse
   ... program source, free form, any number of lines ...
<<<END>>>
<- ok result=put program=pulse size=412 crc=3298
-> link term=off
<- ok term=off
```

- The first line of the payload is `store put <name>`; everything after the first newline, up to the
  terminator, is the source. Because the terminator delimits it, the device knows the size and
  never needs a length or CRC argument from the client.
- The device validates the name, stages the payload to `/programs/.upload.tmp`, and renames it
  into place (spec 02 §3). It computes the size and CRC-32 from the payload it received, so
  neither is passed on the wire, and the reply reports what it computed.
- A bad name is `err bad_argument field=name value=<name>`, a payload over
  `ProgramStore::kMaxBytes` is `err full program=<name> max=<n>`, and a full store is `err full`
  with no room to stage. In every case the staging file is deleted and the previous program is
  left intact: an interrupted upload never leaves a half-written program.
- `store put` carries a multiline payload, so like `panel draw text` it is recognised before the
  line is tokenised.
- `link term=off` puts newline framing back. The custom terminator is a receive-side setting:
  replies stay `LF` terminated (protocol §12).

## 7. Superseded spellings

| This spec first said | Proto 2 |
| --- | --- |
| `set mode <standalone\|peripheral>` | `vm mode=<standalone\|peripheral>` |
| `list` | `store list` |
| `cat <name> [offset] [len]` | `store cat <name> [offset] [len]` |
| `rm <name>` | `store rm <name>` |
| `run <name> [k=v …]` | `store run <name> [k=v …]` |
| `params <name>` | `store params <name>` |
| `set params <name> k=v …` | `store params <name> k=v …` |
| `prog <name>` | `store put <name>` |
| `status` carrying `program` / `running` / `host` / the state table | `vm get` carries them; `plant get` stays about the plant |
| `help` as a `\|`-packed one-liner | `help` as a page (§4) |

The command surface did not grow a `set` and did not grow a bare verb while these changes
happened: `store` and `vm` were added as resources, which is the whole argument for the rename
landing before the program commands ship rather than after.

## 8. Backward compatibility

- This spec is part of proto 2 and takes the 0.5.0 break with it. There is no additive path:
  [the protocol](../serial-protocol.md) §18 is the migration table, and a 0.5.0 controller gets
  `err unknown_command token=…` on its first command.
- There is no chunked upload: `store put begin` / `append` / `commit` / `abort` are not commands.
  A program arrives whole, delimited by `link term` (§6).
- A controller written before programs existed sees the extra keys in `vm get` and the extra rows
  in `help`, and must ignore unknown keys, which the protocol already requires.
- A controller written before this spec sees `store` and `vm` as resources it does not know:
  `err unknown_command token=store`, which is a truthful answer — the store is not in that build.

## 9. Acceptance criteria

- `store list` returns `default` first with `default=1`, and ends with `end result=list rows=<n>`.
- `store run` with a bad key replies `err bad_argument token=<key>` and starts nothing.
- `store run` with an out-of-range value replies `err out_of_range field=<key> value=<v> min=…`
  and starts nothing, **and leaves a currently running program running**.
- `store params <name>` read and `store params <name> k=v …` write answer with the same payload,
  and the write leaves the store unchanged if any value fails validation.
- `store cat missing` is a single `err not_found program=missing`, with no rows and no `end`.
- An upload that overruns `ProgramStore::kMaxBytes` replies `err full program=<name> max=<n>` and
  leaves the previous program intact.
- `vm get` shows `program`, `running`, `host` and the state table while a program runs.
- `plant start` in stand-alone mode replies `err not_ready mode=standalone`, and `vm mode=peripheral`
  is accepted in stand-alone mode.
- No resource, field, action or op named in a `help` page answers `err unknown_command`.
