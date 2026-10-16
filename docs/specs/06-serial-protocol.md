# Spec 06 — Serial protocol: program commands

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §11 and the wire
half of constraint 2. Extends [the current protocol](../serial-protocol.md) additively.

## 1. Principles

- Every command produces exactly one reply line, as today. The exceptions are the
  paged/multi-line payloads (`list`, `cat`), which end with a single `eof` line and still
  begin with one `ok`/`err` line.
- `draw`/`flush` stay fire-and-forget (validated locally, replied immediately).
- Commands call the plant directly; the reply reflects the result of that call, so `err`
  codes remain authoritative.

## 2. New commands

| Command | Reply | Notes |
| --- | --- | --- |
| `list` | `ok programs=N` + one row per program + `eof` | rows: `name size=<n> params=<n> default=<0|1> running=<0|1>` |
| `cat <name> [offset] [len]` | `ok file <name> <offset> <len> <total>` + source lines + `eof` | paged, as the old `cat` |
| `rm <name>` | `ok removed <name>` | `rm default` re-seeds the passive source |
| `set mode <standalone\|peripheral>` | `ok mode=standalone` / `ok mode=peripheral` | switch the control mode |
| `run <name> [k=v …]` | `ok running <name>` / `err …` | stop current → vent → start; switches to stand-alone if needed |
| `stop` | `ok stopped` | peripheral mode: stop → vent → idle |
| `params <name>` | `ok params=<name> k=v …` | effective values (persisted over defaults) |
| `set params <name> k=v …` | `ok params=<name> k=v …` | persist without running |
| `prog <name>` | `ok stored <name> size=<n>` / `err …` | one shot: `set term` a marker first, then the source follows the first newline and runs to the marker (§6). Not chunked and not hex encoded |

Control and drawing commands (`start`, `stop`, `set target`, `ambient`, `reset`, `draw`,
`flush`) are **peripheral-mode only**; in stand-alone mode they are refused with `err`
(use `set mode peripheral`). Program commands (`list`, `cat`, `rm`, `run`, `params`,
`set params`, `prog`) work in both modes, and so does `set term` — framing is not control.

## 3. `run` parameter validation

- Split `k=v` tokens on whitespace.
- Unknown key → `err bad_argument` (with the key named in a `[W]` log).
- Value failing type/range validation (spec 02 §6) → `err out_of_range`.
- Missing required params are filled from persisted values then manifest defaults; there
  are no required-without-default params.
- A bare `run <name>` uses persisted-then-default values.

Example:

```
-> run pulse cycles=5
<- ok running pulse
-> params pulse
<- ok params=pulse cycles=5 mode=pulse beep=false
-> set params pulse cycles=10
<- ok params=pulse cycles=10 mode=pulse beep=false
```

## 4. `status` and `info` extensions

`status` gains:

| Key | Meaning |
| --- | --- |
| `program` | `default`, `pulse`, … or `-` when idle |
| `running` | `0`/`1` |
| `host` | `0`/`1` |
| `s0` / `s0v` … `s7` / `s7v` | program state table (spec 07 §4), key then value |

`info` is unchanged (the runtime is always Berry).

`help` becomes:

```
ok help=info|status|help|start|stop|reset|reboot|ambient|set|draw|flush|list|cat|rm|run|params|prog
```

## 5. Error codes

The existing five stay. Two are added:

| Code | Meaning |
| --- | --- |
| `not_found` | `cat`/`rm`/`run`/`params`/`set params` named a program that does not exist |
| `full` | a `prog` payload over `ProgramStore::kMaxBytes`, or a store with no room left |

The full set is `unknown_command`, `bad_argument`, `out_of_range`, `not_ready`,
`unsupported`, `not_found`, `full`.

## 6. Upload framing

- The source is **not** chunked and **not** hex encoded: the client sets a terminator that cannot
  appear in the source, then sends the whole program as one command.

```
-> set term <<<END>>>
<- ok term=<<<END>>>
-> prog pulse
   ... program source, free form, any number of lines ...
<<<END>>>
<- ok stored pulse size=412
```

- The first line of the payload is `prog <name>`; everything after the first newline, up to the
  terminator, is the source. Because the terminator delimits it, the device knows the size and
  never needs a length or CRC argument from the client.
- The device validates the name, stages the payload to `/programs/.upload.tmp`, and renames it
  into place (spec 02 §3). It computes the size and CRC-32 from the payload it received, so
  neither is passed on the wire.
- A bad name, a payload over `ProgramStore::kMaxBytes`, or a full store replies `err` and
  deletes the staging file; an interrupted upload never leaves a half-written program.
- `prog` carries a multiline payload, so like `draw text` it is recognised before the line is
  split.
- `set term off` puts newline framing back. The custom terminator is a receive-side setting:
  replies stay `LF` terminated (protocol §3).

## 7. Backward compatibility

- All existing commands keep their replies and semantics; `stop` changes meaning only in
  the program era (it already vented; it now also stops a running program).
- There is no chunked upload: `prog begin` / `prog append` / `prog commit` / `prog abort` are
  not commands. A program arrives whole, delimited by `set term` (§6).
- A controller written before programs existed sees the extra `status`/`info` keys and
  must ignore unknown keys, which the protocol already requires.
- `run`, `list`, `cat`, `rm`, `params`, `prog` were absent before and are new surface.

## 8. Acceptance criteria

- `list` returns `default` first with `default=1`.
- `run` with a bad key replies `err bad_argument` and starts nothing.
- `run` with an out-of-range value replies `err out_of_range` and starts nothing.
- An upload that overruns `ProgramStore::kMaxBytes` replies `err full` and leaves the previous
  program intact.
- `status` shows `program`/`running`/`host` and the state table while a program runs.
