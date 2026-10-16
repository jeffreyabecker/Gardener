# Spec 02 — Program store, manifest and parameters

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §6 and the
storage half of constraint 2.

Owned by `ProgramStore` alone: writes go through it and nothing else touches LittleFS, which is
all the arbitration it needs while there is one loop (spec 01 §3).

## 1. LittleFS layout

```
/programs/<name>.be          a stored program
/programs/.upload.tmp        staging file for an in-flight upload
/programs/.params/<name>     persisted parameter values, one key=value per line
```

There is no separate autostart marker: `default` is the boot program (spec 05).

## 2. Naming, size and count limits

| Constant | Value | Meaning |
| --- | --- | --- |
| `kNameMax` | 31 | max program name length |
| `kMaxBytes` | 64 KiB | max program source size |
| `kMaxCount` | 24 | max stored programs |

- Names match `[A-Za-z0-9_-]+`. Anything else is refused (`err bad_argument` on the wire,
  a status line in the UI).
- The dot files used for staging can never be addressed, because `.` is not in the name
  alphabet.

## 3. `ProgramStore`

```cpp
class ProgramStore {
 public:
  explicit ProgramStore(Storage& storage);

  bool begin();                    // mkdir /programs, clear stale staging, refresh,
                                   // and re-seed `default` if missing
  void refresh();                  // re-scan the store and rebuild programs_[]

  size_t count() const;
  const ProgramInfo* at(size_t index) const;
  int indexOf(const char* name) const;
  bool exists(const char* name) const;
  bool read(const char* name, String& out) const;
  bool remove(const char* name);   // `default` re-seeds the passive source

  // Atomic upload: staged to /programs/.upload.tmp and renamed on commit. The wire sends a
  // whole program in one command (spec 06 §6), so this is called once per upload.
  bool writeBegin(const char* name);
  bool writeAppend(const uint8_t* data, size_t length);
  bool writeCommit(uint32_t expectedSize, uint32_t expectedCrc32, String& error);
  void writeAbort();

  uint32_t usedKb() const;
  uint32_t totalKb() const;
  static bool validName(const char* name);

 private:
  static String pathFor(const char* name);
  static uint32_t crc32(const uint8_t* data, size_t length);
  void seedDefault();              // writes the built-in passive default
  Storage& storage_;
  ProgramInfo programs_[kMaxCount];
  size_t count_ = 0;
  // upload staging state
  bool writing_ = false;
  String writingName_;
  uint32_t writeBytes_ = 0;
  uint32_t writeCrc_ = 0xFFFFFFFFu;
};
```

- `refresh()` sorts alphabetically and pins `default` first.
- `remove("default")` deletes the file and immediately calls `seedDefault()`, so the
  device never boots without a home program.
- Upload commit refuses when the name is invalid, the size does not match, the CRC does
  not match, or the store is full; the staging file is deleted on any failure.

## 4. Reserved programs: `default` and `browser`

- `default` — the boot and home program. Seeded source is a passive readout (never calls
  `vacuum.start` or `vacuum.target`), drawing pressure, state and duty from
  `vacuum.state()` via `screen.draw_text`.
- `browser` — the program selector and params editor. Seeded source lists and navigates
  the store via the `programs` module and launches the selection with `programs.run`.
- Both are seeded in `begin()` and re-seeded after `remove()`, so the device never lacks
  a home or a selector. An owner's upload of a program with either name overwrites the
  seeded source.

## 5. Parameter manifest

Parameters are declared in a comment header at the top of the source, before the first
line that is neither blank nor a `#` comment:

```berry
# @param target number 40.0 min=5 max=150 step=0.5 unit=kPa "Target absolute pressure"
# @param cycles int 10 min=1 max=100 "Pulse cycles"
# @param mode choice hold choices=hold|pulse "Run mode"
# @param beep bool false "Beep on fault"
```

Grammar per line:

```
"# @param" SP name SP type SP default *( SP key=value ) SP quoted-description
name        = ALPHA / "_" *( ALPHA / DIGIT / "_" )
type        = "number" / "int" / "choice" / "bool"
key=value   = "min=" number / "max=" number / "step=" number /
              "unit=" token / "choices=" token *("|" token)
```

| Type | Editor control | Allowed keys | Default value form |
| --- | --- | --- | --- |
| `number` | Up/Down step, repeat ×10 | `min` `max` `step` `unit` | float |
| `int` | Up/Down step, repeat ×10 | `min` `max` | integer |
| `choice` | Up/Down cycles options | `choices` | one of the choices |
| `bool` | Up/Down toggles | — | `true` / `false` |

Parse rules:

- The header ends at the first line that is not blank and does not start with `#`.
- A malformed `@param` line is skipped and logged `[W] params: bad line N`, and the
  program still runs (with that parameter simply absent).
- Duplicate names: first occurrence wins, later ones are logged and skipped.
- A `choice` default not in `choices` falls back to the first choice.
- A numeric default outside `[min,max]` is clamped.

## 6. Parameter model and persistence

`ProgramParams` owns the typed model:

```cpp
class ProgramParams {
 public:
  static constexpr uint8_t kMaxParams = 8;

  struct Param {
    enum class Type : uint8_t { Number, Int, Choice, Bool };
    String name;
    Type type = Type::Number;
    String value;                 // effective value, formatted canonically
    float minV = 0, maxV = 0, stepV = 1;
    String unit;
    String choices;               // "a|b|c", for Choice
    String description;
  };

  // The resolved parameter set handed to a running program (captured at start).
  struct Params {
    Param items[kMaxParams];
    uint8_t count = 0;
  };

  // Parse the header of `source` into `out` (max `maxParams`). Returns the count.
  static uint8_t parseManifest(const String& source, Param* out, uint8_t maxParams);

  // Manifest defaults overlaid with persisted values, validated and coerced.
  static void resolve(const String& source, const String& persistedLines, Params& out);

  static bool validate(const Param& p, const String& value, String& error);
  static String format(const Param& p);     // "key=value"
};
```

Persistence:

- File `/programs/.params/<name>`, one `key=value` per line, written atomically
  (temp + rename) by whichever path asked for it: the `browser` program's editor, or
  `set params <name> k=v …` over the wire.
- Written when a program is run from the `browser` program's params editor or by
  `set params <name> k=v …` over the wire.
- Merge is deterministic:
  1. start from manifest defaults,
  2. overlay persisted values (persisted wins),
  3. drop persisted keys the manifest no longer declares,
  4. validate/coerce each value; an invalid value falls back to its default.
- Parameters are captured at program start and do not change mid-run.

## 7. `ProgramInfo` and who reads the store

The store owns the list. `refresh()` re-scans LittleFS and rebuilds `programs_[]`;
`count()`, `at()` and `indexOf()` answer from it, and nothing copies it anywhere else
(spec 07 §1).

```cpp
struct ProgramInfo {
  char name[32];
  uint32_t size;
  uint8_t paramCount;              // manifest params, counted at refresh
};
```

- `default` is `at(0)`: `refresh()` pins it first. That is what `default=1` on the wire
  (spec 06 §2) and `is_default` in Berry (spec 03 §6.5) report — derived, not stored.
- `running` is derived too: it is the name and flag in `ProgramStatus` (spec 07 §1).
- Reads of the in-RAM index are open to the serial line and to a running program; writes,
  `refresh()` and everything else that touches LittleFS go through `ProgramStore` (spec 01
  §3). A `list` cannot land mid-upload: an upload arrives as one command and runs to
  completion inside the dispatch that carried it (spec 06 §6).

## 8. Acceptance criteria

- Upload of a 60 KiB program with a wrong CRC leaves no partial file behind.
- `remove("default")` leaves a bootable `default` in place immediately.
- A manifest with a bad line still yields a runnable program and a `[W]` log line.
- Persisted params that a re-uploaded manifest no longer declares are dropped silently;
  missing ones fall back to defaults; out-of-range persisted numbers are clamped.
- `default` is first in `list` and in the browser, marked `*`.
