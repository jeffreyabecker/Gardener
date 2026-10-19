# Domain-first serial protocol

Status: implemented as `proto=4`. This is the design record; `docs/serial-protocol.md` is the
normative surface.

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
- The dispatcher reads only the domain token. Everything after it belongs to that domain, which
  tokenizes its own verb and arguments — and, where it defines one, its body.
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
- `help` is a domain with its own grammar: `help`, `help <domain>`, `help <domain> <topic>`.

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

`help` is a domain like every other. `HelpDomain` tokenizes its own optional domain and topic
arguments, looks the target up in the domain list, and calls its `describe()` or `help(topic)`. Each
domain exposes those two methods; `HelpDomain` exposes them too, so `help help` works. `HelpDomain`
writes the top-level page; a target domain's `help(topic)` writes its own page.

- `help` -> `ok help domains body=rows rows=N`, rows `domain=<name> topics=<n> about="<text>"`,
  `end help domains body=rows rows=N`.
- `help <domain>` -> `ok help <domain> body=rows rows=N`, rows
  `topic=<name> args="<syntax>" about="<text>"`, `end help <domain> body=rows rows=N`. For example,
  the `device` rows include `topic=get args="all|<field>"` and
  `topic=set args="log <on|off>|unit <unit>"`.
- `help <domain> <topic>` -> `ok help <domain> topic=<topic> body=rows rows=N`, then `arg=`,
  `reply=` and `error=` rows, then `end help <domain> topic=<topic> body=rows rows=N`.
  `help device set` describes both forms, `log` and `unit`.
- `help <domain>` for a domain that does not exist is `err help <domain> unknown_domain`; an unknown
  topic under a real domain is `err help <domain> unknown_topic token=<topic>`.
- Every topic a page names is a topic the domain accepts, so a topic taken from `help` never
  produces `unknown_topic`.

### Errors

| Code | Meaning |
| --- | --- |
| `unknown_domain` | the first token is not a domain (dispatcher-level) |
| `unknown_verb` | the domain does not accept that verb |
| `unknown_topic` | the help domain does not have that topic |
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
    virtual void execute(Stream& input) = 0;  // stream is positioned after the domain token
    virtual void describe() = 0;               // one row for `help`
    virtual void help(const char* topic) = 0;  // null = the topic list
  };
  ```

  `DomainContext { AppContext& app; Stream& out; bool& logEnabled; }` is built by `SerialLink` and
  passed to every domain at construction. The command stream is an argument to `execute`, not part
  of the context: the domain reads and tokenizes everything after its own name, including a body.
  `out` is the reply side of the port, so the domain formats its own reply.

  There is no separate `Items`, `FieldTable` or `Reply` class. `Domain.h` holds the item builders
  (`addItem`/`addItemU32`/`addItemFloat`/`addItemFlag`), the `FieldSpec` table and the
  `findField`/`readField`/`readFields` helpers, plus the opt-in
  `replyOk`/`replyErr`/`replyRow`/`replyEnd` helpers. A domain that needs a different reply shape —
  the future `fs read` with its raw body — writes to `ctx.out` itself.

  `DomainList` holds the domains in help order and offers `find`, `count` and `at`. `SerialLink`
  owns it; `HelpDomain` holds a reference to it and is constructed last with the list, then added
  to it.
- `core/Parse.h` — the strict scalar parsers (`parseFloat`, `parseU32`, `parseFlag`) moved out of
  the deleted `LineGrammar`. It parses values, not command lines.
- `app/serial/Domains.h` plus `app/serial/Domains/Device.cpp`, `Vacuum.cpp`, `Buzzer.cpp`,
  `Help.cpp` — one private class per domain with a factory. Each class tokenizes its own verbs,
  owns its help text and its reply shape, and lives in its own file. `HelpDomain`'s factory also
  takes the `DomainList&`.
- `SerialLink` reads the first token of each command, finds the domain in the list, and hands it
  the command stream positioned immediately after that token. The command stream is the receive
  buffer and the port behind one `Stream`. `SerialLink` does not parse verbs, fields or arguments,
  and it does not format domain replies; it writes only the link's own errors and diagnostics. It
  owns the receive buffer, the domain list, host presence and the diagnostics gate. It constructs
  the domains in help order and holds them for the device's lifetime. Host presence stays here
  because no domain exposes it on the wire.

### Changed pieces

- Delete `LineGrammar.h/.cpp`. There is no shared command tokenizer: each domain reads and
  tokenizes its own stream, including quoted arguments and a body. `Tokens`, `splitValue`,
  `same`/`sameFold` and `lower` go with it; case folding uses `String::toLowerCase` and comparison
  uses `String::equalsIgnoreCase`. The strict `parseFloat`, `parseU32` and `parseFlag` helpers move
  to `core/Parse.h` because more than one domain needs them.
- `Items` is not a class: the item builders and the opt-in reply helpers live in `Domain.h`, and
  each domain formats its own reply text with them.
- The battery readers move from the vacuum table to the device table: `device get all` owns
  `batt_v` and `batt_pct`, and `vacuum get all` no longer has `batt`.
- `Config.h`: `PROTOCOL_VERSION = 4`; `FIRMWARE_VERSION = "0.7.0"`.
- Delete `SerialSurface.h`, `SerialPayload.cpp`, `SerialHelp.cpp` and the old line grammar. The
  serial directory is `Domain.h`, `Domains.h`, `SerialLink.h/.cpp` and `Domains/`.
- `App`/`Build` construction of `SerialLink` is unchanged.

## Documentation

- Rewrite `docs/serial-protocol.md` as the proto 4 surface: new grammar, replies, domains and verbs,
  help pages, errors, the unchanged pressure/state/safety model, and a proto 3 -> 4 migration table.
- Update the serial examples in `README.md` and the `device set log off` / `device set unit <unit>`
  mentions in `docs/routines.md`.
- Add a status note to `docs/serial-protocol-fs.md`: it is still a proposal, written against the old
  grammar, to be rebased on proto 4 when `fs` lands. Do not implement `fs` in this change.

## Out of scope

- The `fs` domain and raw payload framing. `execute(Stream&)` already gives it the stream, so it
  can read its own header and its `<bytes>` body without a new interface hook. Its replies will use
  the rows body for `fs list` and the bytes body for `fs read`.
- Any change to the vacuum control model, state machine, safety behaviour, or poll rates.

## Verification

- `powershell -File scripts/arduino.ps1 -Task build` compiles cleanly.
- Walk every example in the new protocol doc against the parsers; confirm each reply matches the
  grammar and each `help` page names only accepted topics.
- Confirm every domain consumes exactly its command, including a body, and leaves the stream at the
  next domain token.
- Confirm `help` is dispatched through the same path as every other domain, including `help help`.
- Confirm link-loss reset, `device reboot` reply-before-restart, and `device set log off` still
  behave as documented.
- Confirm no `hal/` file includes anything under `app/`.

## Todos

1. Write the proto 4 grammar, reply, domain, help and error sections in `docs/serial-protocol.md`.
2. Add `Domain.h` (including `DomainList`, `FieldSpec`, the item builders and the opt-in reply
   helpers) and `core/Parse.h`; reduce `SerialLink` to reading the domain token, handing the
   command stream to the domain, and writing only its own errors and diagnostics.
3. Implement the `device`, `vacuum`, `buzzer` and `help` domains in their own files and delete the
   old table/help files.
4. Update the README and routines examples and mark the fs proposal as pre-proto-4.
5. Build the sketch and review the doc examples against the implementation.
