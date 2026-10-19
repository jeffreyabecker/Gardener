# Plan: implement the `fs` serial domain

## Problem

The device's LittleFS contents (`/routines`, `/modules`, `/assets`, `/settings.ini`) can only be
changed by reflashing the data image. Add an `fs` domain with `fs list`, `fs read`, `fs write`,
`fs mkdir` and `fs delete`.

`fs` is a domain like the others: it owns its grammar and formats its own replies. `fs write` is
one length-delimited command, not an aggregation of chunks. The wire is unsettled and
`PROTOCOL_VERSION` stays `1`.

## Wire surface

All `fs` arguments are positional and unquoted. A path is one token and contains no spaces. There
is no percent-encoding layer and no quoting.

```
fs list <path>
  ok rows=N
  <file|dir> <name>
  ...

fs read <path>
  ok bytes=N
  <exactly N raw bytes>

fs write <path> <bytes>[LF]
  <exactly `bytes` raw bytes>
  [optional CR LF]
  ok bytes=N

fs mkdir <path>
  ok

fs delete <path>
  ok removed=N
```

- `fs write` is length-delimited. The header's `<bytes>` is authoritative; the device never
  searches the payload for a terminator. The payload may contain `LF`, NUL and any other byte.
- `fs read` replies with a header carrying `bytes=N`; the host reads exactly N bytes. A zero-byte
  file sends only the header.
- `fs list` replies with a header carrying `rows=N`, then N entry lines. Each entry line is
  `type name`: `type` is `file` or `dir`, and `name` is one token, so a listed name can be pasted
  into `fs read` or `fs delete`. `rows=N` is the count and tells the host when the response is
  complete.

  ```
  -> fs list /tmp
  <- ok rows=2
  <- file a.txt
  <- dir sub
  ```
- `fs` replies have no `end` line. The count in the header (`rows=N` or `bytes=N`) is
  authoritative, so the host knows when the reply is complete.
- `fs delete` counts every entry removed, including the target; a file is `removed=1`.
- A write header that is syntactically valid but cannot be honoured still consumes the declared
  payload before replying, so the next command starts on a boundary. A header refused on syntax
  (`<bytes>` malformed or over the cap, malformed path) replies immediately and expects no payload.
- `FS_WRITE_MAX_BYTES = 1 MiB`. A larger count is `out_of_range`.
- `FS_WRITE_IDLE_MS = 2000`. A payload that stops arriving is abandoned, the partial file is
  removed and the reply is `err bad_argument`.
- One optional `LF` or `CRLF` after a completed write payload is consumed.

## Paths

- Absolute, `/`-separated, case-sensitive, and one token: no spaces. `.` and `..`, empty segments
  and trailing slashes are refused. `/` itself is valid only for `fs list`.
- `/settings.ini` is reserved: `fs write` and `fs delete` refuse it with
  `err unsupported path=/settings.ini reason=reserved`; `fs read` and `fs list` may see it.
- A path that is not absolute, has a bad segment, contains a space, or is missing is
  `err bad_argument <token>` unless the path parses and the filesystem says it is absent
  (`err not_found <path>`).

## Errors

- New code `not_found` for an absent path: `err not_found <path>`.
- `bad_argument` for missing or malformed arguments and a short payload, naming the first bad
  token when there is one.
- `out_of_range field=bytes value=<n> max=1048576` for an oversized write.
- `unsupported path=<path> reason=reserved` for `/settings.ini` writes/deletes.
- `not_ready <reason>` with `is_dir`, `not_dir`, `exists`, `no_parent`, `no_space` or `io`.
- A recursive delete that fails part-way replies `err not_ready io`; `fs list` shows what remains.

## Implementation

### 1. Streaming dispatch

`SerialLink::poll()` already reads one byte at a time and stops at the `LF`, so a `fs write`
payload is still on the port when the domain is called. What remains is to keep the link from
reading a new header while a body is in flight:

- `execute(Stream& io)` reads the rest of its line from `io` and parses the header. If the domain
  reports that it is streaming, record it as the active domain.
- On later polls, call the active domain's `service(Stream&, uint32_t)` instead of reading another
  header.
- On link loss, call `cancel()` on the active domain so a partial file is closed and removed.
- `Domain` gains default no-op `streaming()`, `service(Stream&, uint32_t)` and `cancel()` hooks.
  `fs` is the only domain that overrides them; the hooks exist because a body cannot be handled
  by a synchronous `execute()` without blocking the control loop.

`fs write` reads and writes in small chunks (128 bytes) as bytes arrive from `io`. `fs read`
writes one small chunk per poll. Neither loops over the whole file in one pass.

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

Add `app/serial/Domains/Fs.cpp` and `createFsDomain()` in `Domains.h`.

- Local argument parser: one `nextArg()` that takes the next bare token, and a `rest()` for the
  final path where a verb needs it. Do not add a global tokenizer.
- Local path validation: absolute, no empty/`.`/`..` segments, no trailing slash, no spaces,
  root only for list, exact `/settings.ini` reservation.
- `fs list` writes each row straight to `ctx.out`: `file <name>` or `dir <name>`, one `\n`.
- State machine: `Idle`, `Reading`, `Writing`, `Discarding`. `streaming()` is true for the last
  three. `service()` advances one chunk, enforces the 2 s idle timeout, and sends the final reply.
- `execute()` parses the verb and arguments and either completes a synchronous command or starts a
  state.
- `fs get` is not defined and is `unknown_verb`.

### 4. Constants and wiring

- `core/Config.h`: `FS_WRITE_MAX_BYTES = 1024 * 1024`, `FS_WRITE_IDLE_MS = 2000`.
- `SerialLink.cpp`: add the `fs` domain after `buzzer`.
- Keep `COMMAND_LINE_MAX = 128` for the header only.

### 5. Protocol documentation

Update `docs/specs/serial-protocol.md`:

- Intro: add `fs` to the domain list.
- Replies: document the `rows=N` and `bytes=N` body counts and the `fs` entry line.
- Add the `fs` section with the five verbs, path rules, framing, errors and behaviour.
- Errors: add `not_found` and the `fs` reasons.
- Renumber the sections after `fs` and update cross-references.
- Delete `docs/plans/serial-protocol-fs.md` and remove its entry from `docs/README.md`.

### 6. Validation

- Build: `powershell -ExecutionPolicy Bypass -File scripts/arduino.ps1 -Task build`.
- Wire checks with a serial terminal or a small host script:
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
- A failed recursive delete is not atomic. The reply is `err not_ready io`, and `fs list` shows
  what remains.
