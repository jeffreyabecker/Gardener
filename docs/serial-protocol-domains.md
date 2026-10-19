# Domain-first serial protocol

Status: proposed revision `proto=4`. This is the working design; `docs/serial-protocol.md` remains
the current proto 3 surface until this is implemented.

## Problem

The wire is one universal grammar (`resource op field=value`) implemented by a single table in
`SerialLink`/`SerialPayload`/`SerialHelp`. A domain cannot own its own syntax: `fs` already needs
positional arguments and a raw payload, and the current design has no place to put either. Rebuild
the protocol from scratch around domains.

**New wire revision: `proto=4`.** This is a deliberate break from proto 3; existing controllers must
stop on the revision rather than guess.

## Protocol shape

### Commands

```
command = header [ body ]
header  = domain SP verb *( SP arg ) LF
arg     = bare / quoted
bare    = printable except space, DQUOTE, backslash
quoted  = DQUOTE *( printable / "\" ( DQUOTE / "\" ) ) DQUOTE
body    = %x00-FF{ N }        ; N is declared by the domain's grammar
```

- Every command starts with a domain, then a verb owned by that domain, then positional arguments.
  There is no universal `field=value` grammar and no `get`/`set` imposed by the dispatcher.
- Domain and verb are case-insensitive; arguments are case-sensitive.
- Quoted arguments carry spaces and quotes: `buzzer play "a4:1000 c6:500"`.
- `-` means unset, `off` means explicitly disabled. `""` is an empty string.
- The header ends at `LF`, always. A `\r` immediately before the `LF` is ignored, so `\r\n` works.
  There is no command that changes the terminator, and no `link` domain.
- A command may have a body after its header. Most domains do not; `fs write` will, with the header
  `fs write <path> <bytes>` followed by exactly `<bytes>` raw bytes. The body may contain any byte,
  including `LF`; the header's count is authoritative. A domain that defines a body says so on its
  help page.
- A command header is at most 128 bytes; the limit does not apply to a body. A longer header is
  answered `bad_argument` and discarded, never truncated into a different command.
- `#` at the start of a line is a comment. A blank line gets no reply.
- `help` is the one meta-command: `help`, `help <domain>`, `help <domain> <verb>`.

### Replies

Every reply begins with a header line. The header is the same for every domain; what follows it
depends on the body the header announces.

```
header = "ok"  SP domain SP verb *( SP item ) LF
       | "err" SP domain SP verb SP code *( SP item ) LF
item   = key "=" value
key    = ALPHA *( ALPHA / DIGIT / "_" / "-" )

reply  = header [ body ]
body   = rows | bytes
rows   = row{ N } end
bytes  = %x00-FF{ N } end
row    = item *( SP item ) LF
end    = "end" SP domain SP verb *( SP item ) LF
```

- Every header echoes the domain and verb it answers, so a controller can match a reply without
  counting. `ok` means accepted; `err` carries a code and never has a body.
- An `ok` header announces its body with `body=rows rows=N` or `body=bytes bytes=N`. No `body=`
  item means the header is the whole reply, which is what most commands use.
- `body` is a reserved key; a domain may not use it for data. `rows` and `bytes` are the body's
  count, not ordinary fields, when `body=` is present.
- The `end` line repeats the domain, verb and `body=`; its count is what was actually sent, so a
  body that stops early is detectable. A short body also carries `error=<code>`.
- An `err` is a complete reply. Once an `ok` header has announced a body, the body and its `end`
  line follow; a failure goes in the `end` line, not in a second header.
- The body shapes above are the ones the protocol defines so far; a domain's help page names the
  shape it uses.
- A row is items. `help` uses a rows body; `fs list` will. `fs read` will use a bytes body.
- A bytes body may contain any byte, including `LF`; a controller reads exactly `N` bytes, then the
  `end` line.
- Values in headers and rows are bare unless they contain a space, `"` or `\`, in which case they
  are quoted and escaped. The old whitespace-to-`_` substitution is removed.
- An action in the current domains replies with the fields it changed:
  `ok vacuum start state=PULL`. That is a convention of those domains, not a rule of the framing; a
  verb answers with whatever its own help page says.
- Unknown domain: `err - - unknown_domain token=<token>`. Missing verb:
  `err <domain> - bad_argument`. Unknown verb: `err <domain> <verb> unknown_verb`.
- Diagnostics stay `[I]`/`[W]`/`[E]` records, distinct from replies; `device set log off` still
  silences them.

### Domains and verbs

| Domain | Verbs | Fields read with `get` |
| --- | --- | --- |
| `device` | `get all\|<field>`, `set log <on\|off>`, `set unit <kpa\|psi\|inhg\|mmhg>`, `reboot` | `proto fw chip heap batt_v batt_pct log unit display-size` |
| `vacuum` | `get all\|<field>`, `target <kPa\|->`, `start`, `stop`, `reset` | `kpa target floor duty state sensor fault up` |
| `buzzer` | `get all\|<field>`, `tone <hz> <ms>`, `play "<note>:<ms> ..."`, `stop` | `tone play playing` |

Battery state is a `device` fact, not a vacuum one. `device get all` reports `batt_v` and
`batt_pct`; the vacuum domain has no `batt` field. A pack that is not fitted reads `-` for both.

```
-> device get all
<- ok device get proto=4 fw=0.7.0 chip=ESP32-S3 heap=279384 batt_v=3.94 batt_pct=78 log=on unit=kpa display-size=128x64
-> vacuum get all
<- ok vacuum get kpa=40.07 target=40.0 floor=60.0 duty=72.0 state=HOLD sensor=1 fault=none up=45000
```

`panel` is gone. Its one fact is now a read-only `device` field:

```
-> device get display-size
<- ok device get display-size=128x64
```

The value is `widthxheight` in pixels. Because it is a field, `device get all` reports it too. The
wire still does not draw on the panel.

`link` is gone entirely. There is no command for the terminator, host attachment, or any other
setting of the receive path; host presence remains internal to `SerialLink` because link loss still
stops the run.

- `get all` reads every field in one line; `get <field>` reads one. A bare `get` is `bad_argument`;
  `all` is a keyword, not a field. A verb the domain does not accept is `unknown_verb`; a `get`
  argument that is not a field is `bad_argument token=<field>`.
- Write verbs return the field they changed: `ok vacuum target target=40.0`,
  `ok device set log=off`.
- `device reboot` replies `ok device reboot` before restarting.
- `buzzer stop` replaces `buzzer tone=-`; `buzzer tone <hz> 0` still means until stopped.

### Help

Each domain exposes `describe()` and `help(verb)`:

- `help` -> `ok help domains body=rows rows=N`, rows `domain=<name> verbs=<n> about="<text>"`,
  `end help domains body=rows rows=N`.
- `help <domain>` -> `ok help <domain> body=rows rows=N`, rows
  `verb=<name> args="<syntax>" about="<text>"`, `end help <domain> body=rows rows=N`. For example,
  the `device` rows include `verb=get args="all|<field>"` and
  `verb=set args="log <on|off>|unit <unit>"`.
- `help <domain> <verb>` -> `ok help <domain> verb=<verb> body=rows rows=N`, then `arg=`, `reply=`
  and `error=` rows, then `end help <domain> verb=<verb> body=rows rows=N`. `help device set`
  describes both forms, `log` and `unit`.
- `help <domain>` for a domain that does not exist is `err help <domain> unknown_domain`; an unknown
  verb under a real domain is `err help <domain> unknown_verb token=<verb>`.
- Every verb a page names is a verb the domain accepts, so replaying `help` can never produce
  `unknown_verb`.

### Errors

| Code | Meaning |
| --- | --- |
| `unknown_domain` | the first token is not a domain (dispatcher-level) |
| `unknown_verb` | the domain does not accept that verb |
| `bad_argument` | missing, malformed, extra or unterminated argument |
| `out_of_range` | parses but is outside the field's bounds |
| `not_ready` | valid but cannot be honoured now; carries `reason=` |
| `unsupported` | defined by the protocol, not available in this build |

`not_found` is reserved for the future `fs` domain.

## Implementation

### New pieces

- `app/serial/Domain.h` — the interface and the context every domain receives:

  ```cpp
  class Domain {
   public:
    virtual ~Domain() {}
    virtual const char* name() const = 0;
    virtual void execute(Tokens& tokens) = 0;  // everything after the domain token
    virtual void describe() = 0;               // one row for `help`
    virtual void help(const char* verb) = 0;   // null = the verb list
  };
  ```

  `DomainContext { AppContext& app; Wire& wire; bool& logEnabled; }` is built by `SerialLink` and
  passed to every domain at construction.
- `app/serial/Wire.h/.cpp` — headers, the rows body, the error helpers, and value quoting. It owns
  the reply side of the stream; `SerialLink` keeps the byte/framing side. The bytes body is part of
  the contract but is only implemented when `fs read` arrives.
- `app/serial/FieldTable.h/.cpp` — the shared `FieldSpec` table, `get all`/`get <field>` handling,
  and field rows for help. Shared mechanics only; each domain still owns its verb dispatch.
- `app/serial/Domains.h` plus `app/serial/Domains/Device.cpp`, `Vacuum.cpp`, `Buzzer.cpp` — one
  private class per domain with a factory. Each class parses its own verbs, owns its help text, and
  lives in its own file.
- `SerialLink` is reduced to: stream, buffer, line splitting, domain lookup, the `help` meta-command,
  host presence, and diagnostics. It constructs the domains in help order and holds them for the
  device's lifetime. Host presence stays here because no domain exposes it on the wire.

### Changed pieces

- `LineGrammar` gains quoted-token handling (`Tokens` decodes `"..."` with `\"`/`\\` in place and
  reports an unterminated quote).
- `Items` quotes values that need it and stops replacing whitespace with `_`.
- The battery readers move from the vacuum table to the device table: `device get all` owns
  `batt_v` and `batt_pct`, and `vacuum get all` no longer has `batt`.
- `Config.h`: `PROTOCOL_VERSION = 4`; `FIRMWARE_VERSION = "0.7.0"`.
- Delete `SerialSurface.h`, `SerialPayload.cpp`, `SerialHelp.cpp`; their roles move to `Domain.h`,
  `FieldTable`, `Wire`, and the per-domain files.
- `App`/`Build` construction of `SerialLink` is unchanged.

## Documentation

- Rewrite `docs/serial-protocol.md` as the proto 4 surface: new grammar, replies, domains and verbs,
  help pages, errors, the unchanged pressure/state/safety model, and a proto 3 -> 4 migration table.
- Update the serial examples in `README.md` and the `device set log off` / `device set unit <unit>`
  mentions in `docs/routines.md`.
- Add a status note to `docs/serial-protocol-fs.md`: it is still a proposal, written against the old
  grammar, to be rebased on proto 4 when `fs` lands. Do not implement `fs` in this change.

## Out of scope

- The `fs` domain and raw payload framing. The `Domain` interface is deliberately just
  `execute`/`describe`/`help`; when `fs` arrives it can extend the interface with a multi-pass
  payload hook. Its replies will use the rows body for `fs list` and the bytes body for `fs read`,
  and its command body for `fs write`.
- Any change to the vacuum control model, state machine, safety behaviour, or poll rates.

## Verification

- `powershell -File scripts/arduino.ps1 -Task build` compiles cleanly.
- Walk every example in the new protocol doc against the parsers; confirm each reply matches the
  grammar and each `help` page names only accepted verbs.
- Confirm link-loss reset, `device reboot` reply-before-restart, and `device set log off` still
  behave as documented.
- Confirm no `hal/` file includes anything under `app/`.

## Todos

1. Write the proto 4 grammar, reply, domain, help and error sections in `docs/serial-protocol.md`.
2. Add `Domain.h`, `Wire` and `FieldTable` and reduce `SerialLink` to framing and dispatch.
3. Implement the `device`, `vacuum` and `buzzer` domains in their own files and delete the old
   table/help files.
4. Update the README and routines examples and mark the fs proposal as pre-proto-4.
5. Build the sketch and review the doc examples against the implementation.
