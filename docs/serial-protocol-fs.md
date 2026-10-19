# Plan: proposed `fs` serial surface

Status: proposal written against the proto 3 grammar. Rebase it on the proto 4 domain framing when
the `fs` domain lands.

## Problem

The device's LittleFS contents — `/routines`, `/modules`, `/assets`, `/settings.ini` — can only be
changed by reflashing the data image. The serial protocol has no way to list, read, create, replace
or delete a file. Add a filesystem surface to the protocol and document it as a **separate proposed
spec**, because it deliberately breaks the resource/op grammar in ways the rest of the wire does not.

## Proposed surface

All commands begin with the `fs` token. Arguments are positional, not `field=value`.

| Command | Reply |
| --- | --- |
| `fs list <path>` | page: `ok result=list path=… rows=n`, one row per entry, `end` |
| `fs read <path>` | `ok path=… bytes=n` then exactly `n` raw bytes (plus one LF when `n>0`) |
| `fs write <path> <bytes> <data>` | `ok path=… bytes=n` after exactly `n` raw bytes have been written |
| `fs mkdir <path>` | `ok path=…` |
| `fs delete <path>` | `ok path=… entries=n`; directories are deleted recursively |

The write form is exactly the one requested: one command, one file, no append/aggregation stage.
`<data>` is **raw bytes**, not hex or base64, and is length-delimited by `<bytes>`.

## Decisions already confirmed

- **Raw data** on the wire, for both `fs write` and `fs read`. No encoding expansion.
- **`fs mkdir <path>`** is a separate verb; `fs write` never creates directories.
- **`fs` header lines get their own ceiling**, independent of the 128-byte command-line limit.
- **`fs delete` is recursive** for directories.

## Key design points the spec must state

### Framing

- A raw payload can contain `\n`, NUL and the current terminator, so a write cannot be
  terminator-delimited. `fs write` is **length-delimited**: the device parses the header line to the
  current terminator, then consumes exactly `<bytes>` bytes as the payload.
- `fs read` uses the same idea in reverse: a normal reply line, then exactly the announced bytes.
- One optional `\n` (or `\r\n`) is consumed after a write payload so a host may keep a line-shaped
  stream. No custom terminator is consumed or required there.
- The header line is still framed by the current terminator (`link term` works unchanged).
- `<bytes>` is decimal and authoritative: the device never searches the payload for a terminator.
- A valid header whose write cannot be honoured (missing parent, existing directory, no space)
  still consumes the declared payload before replying, so the next command starts on a boundary.
- A header refused on syntax (`<bytes>` malformed or over the maximum, malformed path) replies
  immediately; no payload is expected.
- A payload that stops arriving is abandoned after a bounded idle timeout, the partial file is
  removed, and the device replies `err bad_argument reason=short_payload`.

### Paths

- Absolute, `/`-separated, case-sensitive. `.` and `..` segments, empty segments and trailing
  slashes are refused; `/` itself is valid only for `fs list`.
- Percent-encoded on the wire (`%20` space, `%25` `%`, other `%xx` bytes), so positional arguments
  stay one token and any real filename is addressable. `fs list` reports names in the same encoding,
  so a listed name can be pasted into `fs read` or `fs delete`.
- `/settings.ini` is reserved: it belongs to `device unit=` and is refused by `fs write` and
  `fs delete` with `err unsupported path=/settings.ini reason=reserved`. This keeps one owner for
  the live settings object.

### Replies and errors

- `fs list` is a page, but its rows are positional and not `key=value` items:
  `<type> <bytes> <name>` with `type` `file` or `dir`, size `0` for a directory, and the name last so
  spaces never need escaping. `end` repeats the header.
- `fs read`'s header is items; its payload is raw bytes. For `bytes=0` the header is the whole reply.
- `fs` uses the existing error codes where they fit, plus one new code:
  `not_found` (path absent). Missing arguments, malformed paths and short payloads are
  `bad_argument`; oversized paths or byte counts are `out_of_range`; `is_dir`, `exists`,
  `no_parent`, `no_space` and `io` are `not_ready` reasons.
- A controller that does not know `not_found` must treat an unknown code as an error.

### Discovery

- `help` gains a row `resource=fs commands=5`.
- `help fs` is a hand-written page listing each verb and its positional arguments, because `fs` has
  no fields or no-argument actions to derive a page from.
- `fs get` is not defined and is `unknown_command`.

### Behaviour

- The filesystem is loaded at boot. Writes under `/routines` and `/modules` take effect at the next
  boot; `fs` does not re-run the routine loader.
- A write streams as bytes arrive, so the control loop keeps running; the reply comes after the
  payload is complete.
- Link loss during a payload abandons the write, removes the partial file and sends no reply.
- Deleting a non-empty directory is not atomic: a failure part-way leaves the entries already
  removed, and the reply says so.

## Deliverable

`docs/serial-protocol-fs.md` — this plan, written as the proposed spec. It is marked as a proposal,
as an additive extension to `proto 3` (existing command meanings do not move), and as not yet
implemented.

No other repository files change in this step. The cross-reference from `docs/serial-protocol.md`
and the firmware implementation follow once the proposal is accepted.

## Todos

1. Write `docs/serial-protocol-fs.md` from this plan: the surface, framing, paths, replies, errors,
   discovery and behaviour above.
2. Review the draft against the existing spec for consistent terms (`ok`/`err`, page shape, `-`,
   `proto` rule) and confirm every example parses as written.
