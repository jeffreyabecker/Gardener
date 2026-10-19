# Plan: implement the `fs` serial domain

## Problem

The device's LittleFS contents (`/routines`, `/modules`, `/assets`, `/settings.ini`) can only be
changed by reflashing the data image. Implement the proposal in
`docs/plans/serial-protocol-fs.md` as a proto 4 domain: `fs list`, `fs read`, `fs write`,
`fs mkdir` and `fs delete`.

The proposal was written against proto 3. Rebase it onto the domain-first framing: `fs` is a
domain, owns its own grammar, formats its own replies and appears in `help`. `fs write` is one
length-delimited command, not an aggregation of chunks. Adding a domain is additive, so
`PROTOCOL_VERSION` stays `4`.

## Wire surface

All `fs` arguments are positional. Paths are a single argument and use the proto 4 quoted-argument
rules: `fs write "/a file" 12`. There is **no percent-encoding layer**; quoting and the existing
`\"` / `\\` escapes already carry spaces and quotes.

```
fs list <path>
  ok fs list path=<path> entries=N
  <file|dir> <name>
  ...

fs read <path>
  ok fs read bytes=N
  <exactly N raw bytes>

fs write <path> <bytes>[LF]
  <exactly `bytes` raw bytes>
  [optional CR LF]
  ok fs write path=<path> bytes=N

fs mkdir <path>
  ok fs mkdir path=<path>

fs delete <path>
  ok fs delete path=<path> entries=N
```

- `fs write` is length-delimited. The header's `<bytes>` is authoritative; the device never
  searches the payload for a terminator. The payload may contain `LF`, NUL and any other byte.
- `fs read` replies with a header carrying `bytes=N`; the host reads exactly N bytes. A zero-byte
  file sends only the header.
- `fs list` replies with a header carrying `entries=N`, then N entry lines. Each entry line is
  `type name`: `type` is `file` or `dir`, and `name` is a bare or quoted argument using the same
  rules as a command argument, so a listed name can be pasted into `fs read` or `fs delete`.
  `entries=N` is a count item, not a new body type, and it tells the host when the response is
  complete.

  ```
  -> fs list /tmp
  <- ok fs list path=/tmp entries=2
  <- file a.txt
  <- dir sub
  ```
- `fs` replies have no `end` line. The count in the header (`entries=N` or `bytes=N`) is
  authoritative, so the host knows when the reply is complete.
- `fs delete` counts every entry removed, including the target; a file is `entries=1`.
- A write header that is syntactically valid but cannot be honoured still consumes the declared
  payload before replying, so the next command starts on a boundary. A header refused on syntax
  (`<bytes>` malformed or over the cap, malformed path) replies immediately and expects no payload.
- `FS_WRITE_MAX_BYTES = 1 MiB`. A larger count is `out_of_range`.
- `FS_WRITE_IDLE_MS = 2000`. A payload that stops arriving is abandoned, the partial file is
  removed and the reply is `err fs write bad_argument reason=short_payload`.
- One optional `LF` or `CRLF` after a completed write payload is consumed.

## Paths

- Absolute, `/`-separated, case-sensitive. `.` and `..`, empty segments and trailing slashes are
  refused. `/` itself is valid only for `fs list`.
- `/settings.ini` is reserved: `fs write` and `fs delete` refuse it with
  `err ... unsupported path=/settings.ini reason=reserved`; `fs read` and `fs list` may see it.
- A path that is not absolute, has a bad segment, or is missing is `bad_argument` unless the path
  parses and the filesystem says it is absent (`not_found`).

## Errors

- New code `not_found` for an absent path.
- `bad_argument` for missing/malformed arguments and `reason=short_payload`.
- `out_of_range field=bytes value=<n> max=1048576` for an oversized write.
- `unsupported path=<path> reason=reserved` for `/settings.ini` writes/deletes.
- `not_ready` reasons: `is_dir`, `not_dir`, `exists`, `no_parent`, `no_space`, `io`.
- A recursive delete that fails part-way replies
  `err fs delete not_ready reason=io entries=<removed>`.

## Implementation

### 1. Streaming dispatch

Current `SerialLink::poll()` reads every available byte into `buffer_`, so a `fs write` payload
would be buffered in RAM before the domain sees it. Change the link to stop at the command header:

- Read command bytes one at a time until `LF`; do not read past it.
- Dispatch the header. If the domain reports that it is streaming, record it as the active domain.
- On later polls, call the active domain's `service()` instead of reading another header.
- On link loss, call `cancel()` on the active domain so a partial file is closed and removed.
- `Domain` gains default no-op `streaming()`, `service(Stream&, uint32_t)` and `cancel()` hooks.
  `fs` is the only domain that overrides them; the hooks exist because a body cannot be handled
  by a synchronous `execute()` without blocking the control loop.

`fs write` reads and writes in small chunks (128 bytes) as bytes arrive. `fs read` writes one
small chunk per poll. Neither loops over the whole file in one pass.

### 2. Storage

Extend `app/Storage.h/.cpp` without leaking `LittleFS` into the header:

- `Entry { String name; bool directory; }`.
- `exists(path)`, `isDirectory(path)`, `freeBytes()`.
- `listEntries(path, std::vector<Entry>&)` returning false when the path is not a directory.
- `makeDirectory(path)`.
- `remove(path, size_t& removed)` recursive; collect child paths before deleting so iteration is
  not invalidated.
- Streaming handles: `openRead`, `readChunk`, `closeRead`, `openWrite`, `writeChunk`,
  `closeWrite`, `abortWrite`. Use an opaque `WriteHandle`/`File*` in the `.cpp`; `Storage.h` only
  has a forward-declared handle pointer. `abortWrite` closes and removes the partial file.
- Keep the existing text `read`/`write` and suffix `list` API; implement `list` on top of
  `listEntries` so `Routines` keeps its behaviour.

### 3. `FsDomain`

Add `app/serial/Domains/Fs.cpp` and `createFsDomain()` in `Domains.h`; register it before `help`.

- Local argument parser: one `nextArg()` that handles a bare token or a quoted token with `\"`
  and `\\` escapes, and rejects leftovers. Do not add a global tokenizer.
- Local path validation: absolute, no empty/`.`/`..` segments, no trailing slash, root only for
  list, exact `/settings.ini` reservation.
- Local `appendArg()` for `fs list` row names; reuse the same quoting rules as `addItem`, ideally
  by splitting the value-quoting part of `addItem` into `addArg()` in `Domain.h`.
- State machine: `Idle`, `Reading`, `Writing`, `Discarding`. `streaming()` is true for the last
  three. `service()` advances one chunk, enforces the 2 s idle timeout, and sends the final reply.
- `execute()` parses the verb and arguments and either completes a synchronous command or starts a
  state.
- `describe()` reports `domain=fs topics=5`.
- `help(topic)` is hand-written: the top page lists `list`, `read`, `write`, `mkdir`, `delete`,
  and each topic page names its arguments, body shape and errors.
- `fs get` is not defined and is `unknown_verb`.

### 4. Constants and wiring

- `core/Config.h`: `FS_WRITE_MAX_BYTES = 1024 * 1024`, `FS_WRITE_IDLE_MS = 2000`.
- `SerialLink.cpp`: add the `fs` domain; keep `help` last.
- Keep `COMMAND_LINE_MAX = 128` for the header only.

### 5. Protocol documentation

Update `docs/specs/serial-protocol.md`:

- Intro: add `fs` to the domain list.
- Grammar: add `fs`'s `entry = type SP name LF` reply shape; `fs write` uses the existing header +
  body rule.
- Replies: document `fs list`'s `entries=N` count and entry lines, plus `fs read`'s `bytes=N`
  count. State that `fs` replies have no `end` line: the count is what tells the host the response
  is complete.
- Add the `fs` section with the five verbs, path rules, framing, errors and behaviour.
- Help: domain count becomes 5; add `fs` rows and a `help fs` example.
- Errors: `not_found` is no longer reserved; add the fs reasons and `short_payload`.
- Renumber the sections after `fs` and update cross-references (`§8`, `§10`, `§11`).
- Migration: note that `fs` is additive and proto stays 4.
- Delete `docs/plans/serial-protocol-fs.md` and remove its entry from `docs/README.md`.

### 6. Validation

- Build: `powershell -ExecutionPolicy Bypass -File scripts/arduino.ps1 -Task build`.
- Wire checks with a serial terminal or a small host script:
  - `help`, `help fs`, `help fs write`.
  - `fs mkdir /tmp`, `fs list /`, `fs write /tmp/a 5` with `a\nb\0c`, `fs read /tmp/a`.
  - `fs write` with a declared count larger than the bytes sent, then wait 2 s.
  - `fs write /settings.ini 0`, `fs mkdir /tmp`, `fs delete /tmp`, `fs delete /missing`.
  - A large write (near the cap) to confirm the control loop keeps running and RAM stays flat.
- No host test suite exists; the build is the automated gate. Link checking stays clean.

## Notes

- The `fs` domain does not re-run the routine loader after writing under `/routines` or
  `/modules`; changes take effect at the next boot.
- `fs list` collects the directory into a `std::vector<Entry>` before writing rows. A directory
  with an enormous number of entries uses memory proportional to the list; the existing
  `Routines` loader already uses the same shape.
- `fs write` never creates parent directories; `fs mkdir` is the only way to add one.
- A failed recursive delete is not atomic. The reply reports how many entries were removed.
