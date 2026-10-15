# Spec 07 — Snapshot extensibility and the ProgramScreen

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §9, §10 and
constraints 5 and 6.

## 1. Snapshot as the cross-task contract

`SystemSnapshot` remains plain, copyable, pointer-free data (spec 01 §3.2). It is the
only view the serial task and the VM task get of the machine. It grows additively; the
wire format is the compatibility boundary, not the struct (firmware is one build).

## 2. New fields

```cpp
struct ProgramInfo {
  char name[32];
  uint32_t size;
  uint8_t paramCount;
  bool isDefault;
  bool running;
};

struct ProgramStatus {
  bool running;
  char name[32];      // "" when none
  char error[48];     // last error, "" when none
  bool hostAttached;
};

struct ProgramStatePair {
  char key[12];
  char value[12];     // canonical text; floats formatted with 2 dp
};

struct ProgramOverlay {
  char lines[5][22];  // ProgramScreen rows, empty rows unused
  uint8_t lineCount;
  float bar;          // < 0 means "no bar"
  char toast[40];
  uint32_t toastUntilMs;
};

struct SystemSnapshot {
  // ... existing pressure/pump/run/battery/device fields unchanged ...

  ProgramStatus program;
  uint8_t programCount;
  ProgramInfo programs[24];       // sorted, default first (spec 02 §3)
  ProgramOverlay overlay;         // written by VM, read by plant
  uint8_t stateCount;
  ProgramStatePair state[8];      // program state table
};
```

Publication rules:

- The plant publishes a fresh snapshot every 50 ms tick.
- The program list (`programs[]`, `programCount`) is rebuilt by `ProgramStore::refresh()`
  and republished when the store changes; the plant fills the rest every tick.
- `overlay` is produced by the VM into the overlay slot (spec 01 §3.3); the plant copies
  it into the snapshot at publish time.

## 3. Program state table

Programs publish their own telemetry through a fixed-capacity table, without any C++
change:

- `program.state_set("label", 42.5)` / `program.state_set("label", "text")`.
- Up to 8 keys, each key ≤ 11 chars, value ≤ 11 chars (canonical).
- Setting an existing key replaces it; a ninth key replaces the oldest (FIFO) and logs a
  `[W]` record.
- The table surfaces in `status` as `s0=key s0v=value …` (spec 06 §4) and is available to
  the program via `program.state_get`.

## 4. Display ownership

- The `Display` instance (I2C + framebuffer) stays on the plant task. It is **never**
  handed to the VM, the serial task, or a program.
- Serial `draw` ops arrive as validated `Command` entries (spec 01 §3.1) and are applied
  by the plant to the shared buffer; `flush` composites the battery and pushes.
- Programs get the **ProgramScreen** instead of a display:

| Berry call | Effect |
| --- | --- |
| `vacuum.screen_clear()` | clear all five rows + bar |
| `vacuum.screen_line(row, text)` | set row 0..4, truncated to 21 chars |
| `vacuum.screen_bar(fraction)` | bar fill 0..1 |
| `vacuum.toast(text, ms)` | centered overlay for `ms` |

The overlay is a small value struct written by the VM into the overlay slot; the plant
composites it between the device header and footer, then `AppDisplay::flush()` puts the
battery on last. A program therefore cannot clobber the battery, the splash, or another
task's frame.

## 5. Rendering pipeline (Local mode)

```
plant tick, Local mode:
  ui.render():
    header(program name + runStateName)
    for each of overlay.lines[]: row(r, text)
    if overlay.bar >= 0: bar(overlay.bar)
    footer(escape hints)
  overlay_.flush():   # AppDisplay
    drawBattery()
    panel.display()
```

In Host mode the plant applies serial `draw`/`flush` instead; the UI render step is
skipped, and the program overlay is not shown (the host draws).

## 6. Stretch: full drawing for programs

If arbitrary program drawing is ever needed, route the existing `draw` op set through the
same `Command` mailbox the serial link uses, so the plant serializes all drawing. The
ProgramScreen covers the readout use case, so this is optional and not in the first cut.

## 7. Acceptance criteria

- `status` on the wire reports the program name, running flag, host flag, and the state
  table keys while a program runs.
- The RUN screen shows program rows, bar, and toast within one plant tick of the VM
  writing them.
- The battery and the header/footer chrome cannot be erased by a program.
- Serial `draw`/`flush` and the ProgramScreen never race: both are serialized by the
  plant.
