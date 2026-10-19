# Plan: simplify the serial wire

## Problem

The wire carries framing this device does not use. Every reply repeats the domain and verb it
answers, multi-line bodies are announced twice and closed by a line that repeats the header,
arguments are quoted and escaped even though no value needs it, the `help` domain is a
machine-readable schema with its own rows, counts and terminator, and `proto` pretends to be a
stable compatibility number. There is one controller, one reply per command, and no host in this
repository. The complexity is surface without a consumer.

## Decisions

- **No echo.** A success reply is `ok`, optionally followed by items. The domain and verb are not
  repeated. The link already sends exactly one reply per command, in arrival order, and §11
  already tells a controller to match by counting.
- **No quoting, no escapes.** A value is a token. The one verb that takes free-form text
  (`buzzer play`) takes the rest of the line. No value that a reply emits contains a space, so a
  reply is always splittable on spaces.
- **No `help`.** Delete the domain and everything that exists only to serve it: `describe()`,
  `help(topic)`, `TopicSpec`, `FieldSpec`, the page rows, `end`, and the `unknown_topic` error.
  The spec and README are the discovery mechanism.
- **One readout per domain.** `get` takes no argument and writes every field. There is no
  `get <field>`, no `all` keyword, no field lookup and no field table: a controller that wants one
  value reads the line it already polls.
- **One body rule.** A body is announced by `rows=N` or `bytes=N` on the `ok` line, then exactly
  N rows or N bytes follow. No `body=`, no `end`, no second count.
- **No `play` readback.** `buzzer get` reports `tone` and `playing`. The sequence is the
  controller's own input; echoing it back is the kind of chatter this change removes. This also
  removes the last reply value that could contain a space.
- **`proto` is `1`, and unstable.** Set `PROTOCOL_VERSION = 1` and say in the spec that the wire
  is unsettled, the number does not move per revision yet, and a controller must not treat it as a
  compatibility guarantee. Delete the proto 3 migration table.
- **`fs write <path> <bytes>[LF]` stays**, so paths are single tokens with no spaces. The fs plan
  is updated to this rule; `fs` itself is not implemented here.

## Target wire

### Commands

```
command = header [ body ]
header  = domain SP verb *( SP arg ) LF
arg     = token                         ; no quoting, no escapes
body    = %x00-FF{ N }                  ; N is declared by the verb
```

- `#` at the start of a line is a comment; a blank line gets no reply; `\r` before `\n` is ignored.
- A header is at most 128 bytes; the limit does not apply to a body.
- A verb that takes free-form text takes the rest of the line as its final argument:
  `buzzer play a4:1000 c6:500`.
- Every other argument is a single token. Paths contain no spaces.

### Replies

```
reply  = "ok"  [ SP items ] LF
       | "err" SP code [ SP detail ] LF
items  = item *( SP item )
item   = key "=" value                   ; value is a token
detail = token / items
```

- One reply per command, in arrival order. No domain/verb echo.
- An action reports the state it changed, using the same keys `get` reports:
  `ok state=PULL`, `ok target=40.0`, `ok tone=2000,500`, `ok playing=on`, `ok log=off`.
- An `err` is the code, then the detail that code defines. A code naming one thing takes it bare;
  a code with several values uses items:
  `err unknown_verb frobnicate`, `err unknown_domain frobnicate`, `err bad_argument 50`,
  `err not_ready no_target`, `err out_of_range field=target value=1 min=5.0`.
- A body is `rows=N` or `bytes=N` on the `ok` line, followed by exactly that many rows/bytes.

```
-> device get
<- ok proto=1 fw=0.7.0 chip=ESP32-S3 heap=298412 batt_v=3.94 batt_pct=78 log=on unit=kpa display-size=128x64
-> device set unit mmhg
<- ok unit=mmhg
-> vacuum target 40
<- ok target=40.0
-> vacuum start
<- ok state=PULL
-> vacuum get
<- ok kpa=40.07 target=40.0 floor=60.0 duty=72.0 state=HOLD sensor=1 fault=none up=45000
-> vacuum stop
<- ok state=VENT
-> buzzer tone 2000 500
<- ok tone=2000,500
-> buzzer play a4:1000 c6:500
<- ok playing=on
-> buzzer get
<- ok tone=440,610 playing=on
-> buzzer stop
<- ok playing=off
-> vacuum frobnicate
<- err unknown_verb frobnicate
-> vacuum start
<- err not_ready no_target
-> vacuum target 1
<- err out_of_range field=target value=1 min=5.0
-> frobnicate get
<- err unknown_domain frobnicate
```

## Command inventory

Every line the wire accepts and every reply it can produce. `fs` is planned rather than
implemented; it is here because this plan changes its framing too.

### Link

| Input | Reply |
| --- | --- |
| `# ...` or a blank line | none |
| a line over 128 bytes | `err bad_argument`, then bytes are discarded to the next `LF` |
| `<unknown-domain> ...` | `err unknown_domain <token>` |
| `<domain>` with no verb | `err bad_argument` |
| known domain, unknown verb | `err unknown_verb <verb>` |

`\r` before `\n` is ignored. Domain and verb are case-insensitive. `help` is not a domain any
more: it is `err unknown_domain help`.

### Error shapes

| Code | Detail |
| --- | --- |
| `unknown_domain` | bare token |
| `unknown_verb` | bare token |
| `bad_argument` | bare token, or nothing when the argument is missing |
| `not_ready` | bare reason |
| `not_found` | bare path (`fs` only) |
| `out_of_range` | `field=<name> value=<v> [min=<v>] [max=<v>]` |
| `unsupported` | `path=<path> reason=<why>` |

### `device`

`device get` reports, in this order:
`proto fw chip heap batt_v batt_pct log unit display-size`.

| Command | Success | Failure |
| --- | --- | --- |
| `device get` | `ok proto=1 fw=0.7.0 chip=ESP32-S3 heap=298412 batt_v=3.94 batt_pct=78 log=on unit=kpa display-size=128x64` | `err bad_argument <token>` for any argument |
| `device set log on` | `ok log=on` | `err bad_argument [<token>]` |
| `device set log off` | `ok log=off` | `err bad_argument [<token>]` |
| `device set unit kpa` | `ok unit=kpa` | `err bad_argument [<token>]` |
| `device set unit psi` | `ok unit=psi` | `err bad_argument [<token>]` |
| `device set unit inhg` | `ok unit=inhg` | `err bad_argument [<token>]` |
| `device set unit mmhg` | `ok unit=mmhg` | `err bad_argument [<token>]` |
| `device reboot` | `ok`, then the reset | `err bad_argument <token>` |

`batt_v` and `batt_pct` are `-` when no pack is fitted. `device set unit` writes `/settings.ini`;
a failed write still reports the unit in force.

### `vacuum`

`vacuum get` reports, in this order:
`kpa target floor duty state sensor fault up`.

| Command | Success | Failure |
| --- | --- | --- |
| `vacuum get` | `ok kpa=40.07 target=40.0 floor=60.0 duty=72.0 state=HOLD sensor=1 fault=none up=45000` | `err bad_argument <token>` for any argument |
| `vacuum target <kPa>` | `ok target=40.0` | `err out_of_range field=target value=<v> min=5.0`; `err bad_argument [<token>]` |
| `vacuum target -` | `ok target=-` | `err bad_argument <token>` |
| `vacuum start` | `ok state=PULL` | `err not_ready no_target` / `no_reading` / `latch`; `err bad_argument <token>` |
| `vacuum stop` | `ok state=<state>` (`VENT` from a run, unchanged from `IDLE` or `FAULT`) | `err bad_argument <token>` |
| `vacuum reset` | `ok state=<state>` | `err bad_argument <token>` |

`target` and `fault` are `-` when unset. `state` is `IDLE`, `PULL`, `HOLD`, `VENT` or `FAULT`.
A target deeper than `floor` is accepted; the run simply never arrives.

### `buzzer`

`buzzer get` reports, in this order: `tone playing`.

| Command | Success | Failure |
| --- | --- | --- |
| `buzzer get` | `ok tone=440,610 playing=on` or `ok tone=- playing=off` | `err bad_argument <token>` for any argument |
| `buzzer tone <hz> <ms>` | `ok tone=2000,500` | `err out_of_range field=tone value=<hz> min=100.0 max=10000.0`; `err bad_argument [<token>]` |
| `buzzer play <sequence>` | `ok playing=on` | `err bad_argument <token>` naming the first pair that does not parse |
| `buzzer stop` | `ok playing=off` | `err bad_argument <token>` |

`tone=-` when silent. `ms=0` runs until `buzzer stop`. The sequence is the rest of the line:
note/duration pairs separated by spaces, `r` for a rest, no commas. The sequence is not read back.

### `fs` (planned)

Paths are absolute, `/`-separated, case-sensitive, and a single token with no spaces. `.`, `..`,
empty segments and trailing slashes are refused; `/` is valid only for `fs list`.
`/settings.ini` is reserved for writes and deletes.

| Command | Success | Failure |
| --- | --- | --- |
| `fs list <path>` | `ok rows=N`, then N rows of `file <name>` or `dir <name>` | `err bad_argument [<token>]`; `err not_found <path>`; `err not_ready not_dir` |
| `fs read <path>` | `ok bytes=N`, then exactly N raw bytes | `err bad_argument [<token>]`; `err not_found <path>`; `err not_ready is_dir` |
| `fs write <path> <bytes>[LF]` + payload | `ok bytes=N` | `err bad_argument [<token>]`; `err out_of_range field=bytes value=<n> max=1048576`; `err unsupported path=/settings.ini reason=reserved`; `err not_ready no_parent` / `no_space` / `io` |
| `fs mkdir <path>` | `ok` | `err bad_argument [<token>]`; `err not_ready exists` / `no_parent` / `no_space` / `io` |
| `fs delete <path>` | `ok removed=N` | `err bad_argument [<token>]`; `err not_found <path>`; `err unsupported path=/settings.ini reason=reserved`; `err not_ready io` |

`fs write` is length-delimited: the header's count is authoritative, the payload may contain any
byte, and a valid header whose payload stops arriving for 2 s is abandoned with
`err bad_argument` and the partial file removed. A recursive delete that fails part-way is
`err not_ready io`; the count is not on the wire, and `fs list` shows what remains.

## Changes

### 1. `libraries/VacuumCore/src/app/serial/Domain.h`

- `Domain` shrinks to `name()` and `execute(Stream&)`.
- Delete `TopicSpec`.
- `DomainList`: keep `add`/`find`; delete `count`/`at` (only help used them).
- `addItem`: drop the quote/escape scan; emit the value or `-` for null. Keep `addKey`,
  `addItemU32`, `addItemFloat`, `addItemFlag`.
- `replyOk(Stream&, const String& items)` and `replyErr(Stream&, code, items)`.
- Delete `replyRow`, `replyEnd`, `replyFieldHelp`, `replyUnknownTopic`.
- `replyBadArgument(out, token)` -> `err bad_argument <token>`, or `err bad_argument` alone when
  there is nothing to name.
- `replyUnknownVerb(out, verb)` -> `err unknown_verb <verb>`.
- `replyNotReady(out, reason)` -> `err not_ready <reason>`.
- `replyOutOfRange` keeps its keyed items; it drops the domain/verb parameters.
- Delete `FieldSpec`, `findField`, `readField` and `readFields`. Nothing looks a field up by name
  any more.

### 2. Delete `Domains/Help.cpp`

- Remove `createHelpDomain` from `Domains.h`.
- Remove the help registration in `SerialLink.cpp`.
- `SerialLink::linkError` writes `err <code> [detail]` without the `- -` placeholders;
  `unknown_domain` passes the bare token.

### 3. `Domains/Device.cpp`, `Domains/Vacuum.cpp`, `Domains/Buzzer.cpp`

- Delete `describe()`, `help(topic)`, `kTopics`, and the page helpers (`pageItems`,
  `beginPage`/`endPage`, `writeReplyRow`/`writeErrorRow`, `pageHeader`).
- Delete `kFields`, the per-field readers and `kFieldCount`. `get` becomes one function per domain
  that appends every item in order; `tone` and `play` build their own item instead of looking one
  up by name.
- Update every `replyOk`/`replyErr`/`replyBadArgument` call to the new signatures.
- `BuzzerDomain::play`: replace the quoted/escaped argument parser with "the rest of the line is
  the sequence", trimmed. Reply `ok playing=on`.
- `BuzzerDomain::get` emits `tone` then `playing`.
- `DeviceDomain::executeSet` keeps reporting the field it set: `ok log=off`, `ok unit=mmhg`.
- `VacuumDomain::handleTarget` keeps `target=`, now without the echo; `start`/`stop`/`reset` keep
  `state=`.
- `bad_argument` names the offending token, not the whole argument tail.

### 4. `core/Config.h`

- `PROTOCOL_VERSION = 1`; replace the proto-3 comment with the unstable-wire note.

### 5. Spec `docs/specs/serial-protocol.md`

- Intro: domains are `device`, `vacuum`, `buzzer`; `proto` is `1` and the wire is unstable.
- Connecting: drop step 4 (help); keep `device get` as the version check.
- §3 Grammar: drop `quoted`/escapes; document bare tokens and the free-form final argument.
- §5-§7 domain tables: `get` has no arguments. Delete the `get <field>` rows, the `all` keyword
  and the `all|<field>` syntax; a `get` with any argument is `bad_argument`. The command inventory
  above is the source for those tables, including every error shape.
- §4 Replies: the new two-line grammar, no echo, no `body=`/`end`, one count rule; update every
  example. Delete the "match without counting" claim.
- Delete §8 `help`; renumber §9-§12 to §8-§11 and fix cross-references.
- §9 Errors: delete `unknown_topic`; update `unknown_verb` and `unknown_domain` examples;
  `bad_argument` in the new form.
- §10/§11 state model and rates: update example replies; the counting paragraph now stands alone.
- Replace §12 migration with a short "Stability" note.

### 6. Other docs

- `README.md`: the buzzer section says the wire uses `buzzer tone=2000,500` and
  `buzzer play=a4:1000_c6:500`; change to `buzzer tone 2000 500` and
  `buzzer play a4:1000 c6:500`. The "Over the serial line" section loses the help/`end` sentence
  and gets the new example replies, `device get` instead of `device get all`, and `proto=1`.
- `docs/plans/serial-protocol-fs.md`: rewrite the wire surface for this framing:
  `ok rows=N`, `ok bytes=N`, `ok bytes=12`, `ok removed=N`, no `end`, no quoted arguments, paths
  are single tokens with no spaces, no `describe()`/`help()`, and `PROTOCOL_VERSION` is `1`.
  Error details follow the same rule: `err not_found /missing` is bare; `err out_of_range
  field=bytes value=… max=…` and `err unsupported path=… reason=reserved` stay keyed.
- `docs/README.md`: drop the "proto 4" label from the fs plan entry.

## Validation

- `powershell -File scripts/arduino.ps1 -Task build` is the gate. There is no host test suite.
- Link checking stays clean: no `hal/` -> `app/` includes are introduced.

## Out of scope

- Implementing the `fs` domain; only its plan document is updated.
- The vacuum control model, state machine, safety behaviour and poll rates.
- The `[I]`/`[W]`/`[E]` diagnostics format.
