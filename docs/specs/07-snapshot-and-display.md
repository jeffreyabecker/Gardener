# Spec 07 — Snapshot extensibility and program drawing

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

struct SystemSnapshot {
  PlantState plant;               // the whole plant readout (simplified VacuumSystem)

  bool batteryPresent = false;
  bool batteryLow = false;
  float batteryVolts = 0.0f;
  uint8_t batteryPercent = 0;
  uint32_t uptimeMs = 0;

  ProgramStatus program;
  uint8_t programCount;
  ProgramInfo programs[24];       // sorted, default first (spec 02 §3)
  uint8_t stateCount;
  ProgramStatePair state[8];      // program state table
};
```

There is no program-overlay or screen struct: programs draw with commands (§3), not by
publishing a screen into the snapshot.

Publication rules:

- The plant publishes a fresh snapshot every 50 ms tick.
- The program list (`programs[]`, `programCount`) is rebuilt by `ProgramStore::refresh()`
  and republished when the store changes; the plant fills the rest every tick.
- `program` and `state[]` are filled by the plant from the VM's stop/start and
  `state_set` reports; the VM does not write the snapshot directly.

## 3. Display ownership and program drawing

- The `Display` instance (I2C + framebuffer) stays on the plant task. It is **never**
  handed to the VM, the serial task, or a program.
- Programs draw through the same **draw commands** the host uses: each
  `screen.draw_<op>(…)` call becomes a `Command { kind = Draw, … }` in the command mailbox,
  and `screen.flush()` becomes a `Flush` command. The plant applies them to the buffer in
  arrival order and pushes on flush, so host drawing and program drawing are serialized by
  one owner and can never race.
- `AppDisplay` composites the battery on every flush, so the battery (like the splash)
  remains the device's and cannot be erased.
- Program draw calls are no-ops while a host is attached: the host owns the glass
  (spec 04 §8), and a program launched over the wire runs headless.

## 4. Program state table

Programs publish their own telemetry through a fixed-capacity table, without any C++
change:

- `program.state_set("label", 42.5)` / `program.state_set("label", "text")`.
- Up to 8 keys, each key ≤ 11 chars, value ≤ 11 chars (canonical).
- Setting an existing key replaces it; a ninth key replaces the oldest (FIFO) and logs a
  `[W]` record.
- The table surfaces in `status` as `s0=key s0v=value …` (spec 06 §4) and is available to
  the program via `program.state_get`.

## 5. Rendering pipeline

Local mode (no host):

- RUN: the program draws. The plant drains `Draw`/`Flush` commands and, on `Flush`,
  composites the battery and pushes to the glass. No device chrome is drawn.
- BROWSER / PARAMS: the device draws with `ui.render()` and flushes through `AppDisplay`.

Host mode (host attached):

- The plant applies the host's serial `draw`/`flush` commands; the UI render step is
  skipped and program draw calls are dropped.

Both sources share one buffer and one flush path, so the frame is always one owner's.

## 6. Acceptance criteria

- `status` on the wire reports the program name, running flag, host flag, and the state
  table keys while a program runs.
- `screen.draw_text(…)` output reaches the glass within one plant tick of `screen.flush()`.
- The battery cannot be erased by a program, a host, or a page.
- Serial `draw`/`flush` and program `draw`/`flush` never race: the plant is the single
  drawer.
