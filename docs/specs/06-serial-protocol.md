# Spec 06 — Serial protocol: program commands

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §11 and the wire
half of constraint 2. Extends [the current protocol](../serial-protocol.md) additively.

## 1. Principles

- Every command produces exactly one reply line, as today. The exceptions are the
  paged/multi-line payloads (`list`, `cat`), which end with a single `eof` line and still
  begin with one `ok`/`err` line.
- `draw`/`flush` stay fire-and-forget (validated locally, replied immediately).
- Commands that can fail go through the plant request/response path (spec 01 §3.6), so
  `err` codes remain authoritative.

## 2. New commands

| Command | Reply | Notes |
| --- | --- | --- |
| `list` | `ok programs=N` + one row per program + `eof` | rows: `name size=<n> params=<n> default=<0|1> running=<0|1>` |
| `cat <name> [offset] [len]` | `ok file <name> <offset> <len> <total>` + source lines + `eof` | paged, as the old `cat` |
| `rm <name>` | `ok removed <name>` | `rm default` re-seeds the passive source |
| `run <name> [k=v …]` | `ok running <name>` / `err …` | stop current → vent → start; params validated |
| `stop` | `ok stopped` | stop current → vent → **idle** (host present, so no `default`) |
| `params <name>` | `ok params=<name> k=v …` | effective values (persisted over defaults) |
| `set params <name> k=v …` | `ok params=<name> k=v …` | persist without running |
| `prog begin <name>` | `ok uploading <name>` / `err …` | start a chunked upload |
| `prog append <hex>` | `ok n=<bytes>` | hex-encoded chunk (case-insensitive), max 512 source bytes per line |
| `prog commit <size> <crc32>` | `ok stored <name>` / `err crc` / `err size` | stage + rename; atomic |
| `prog abort` | `ok aborted` | delete staging |

`eval <expr>` stays an optional REPL behind the VM (spec 03 §1) and replies with the
printed value.

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

`info` gains `runtime=berry|none`.

`help` becomes:

```
ok help=info|status|help|start|stop|reset|reboot|ambient|set|draw|flush|list|cat|rm|run|params|prog
```

## 5. Error codes

The existing five stay. One is added:

| Code | Meaning |
| --- | --- |
| `not_found` | `cat`/`rm`/`run`/`params`/`set params` named a program that does not exist |

The full set is `unknown_command`, `bad_argument`, `out_of_range`, `not_ready`,
`unsupported`, `not_found`. `err timeout` may appear when the plant does not answer a
request/response command within the serial task's timeout (spec 01 §3.6).

## 6. Upload framing

- `prog begin <name>` validates the name and opens the staging file.
- `prog append <hex>` appends decoded bytes; cumulative size is capped at
  `ProgramStore::kMaxBytes` (64 KiB).
- `prog commit <size> <crc32>` checks size and CRC-32 (standard polynomial, as in spec 02)
  against the staged bytes, then atomically renames the staging file into place and
  refreshes the store.
- Any failure (bad name, full store, bad hex, oversize, CRC/size mismatch) deletes the
  staging file; an interrupted upload never leaves a half-written program.
- The hex encoding keeps the line grammar's "no spaces in a value" rule intact.

## 7. Backward compatibility

- All existing commands keep their replies and semantics; `stop` changes meaning only in
  the program era (it already vented; it now also stops a running program).
- A controller written before programs existed sees the extra `status`/`info` keys and
  must ignore unknown keys, which the protocol already requires.
- `run`, `list`, `cat`, `rm`, `params`, `prog` were absent before and are new surface.

## 8. Acceptance criteria

- `list` returns `default` first with `default=1`.
- `run` with a bad key replies `err bad_argument` and starts nothing.
- `run` with an out-of-range value replies `err out_of_range` and starts nothing.
- An upload with a wrong CRC replies `err crc` and leaves the previous program intact.
- `status` shows `program`/`running`/`host` and the state table while a program runs.
