# Spec 07 — Snapshot and display ownership

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §9, §10 and
constraints 5 and 6.

## 1. Snapshot

`SystemSnapshot` is plain, copyable, pointer-free data. The plant rewrites one object
each tick; `serialTask` and `vmTask` read its fields directly (spec 01 §3). No lock — a
read may span two adjacent ticks, which is fine for a readout.

```cpp
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

- The plant rewrites `snapshot_` every tick.
- `programs[]` / `programCount` are rebuilt by `ProgramStore::refresh()` when the store
  changes.

## 2. Program state table

- `program.state_set("label", 42.5)` writes directly into `snapshot_.state[]` — the VM
  writes, the plant reads it into `status`. Up to 8 keys, key and value each ≤ 11 chars.
- Setting an existing key replaces it; a ninth key replaces the oldest (FIFO) and logs a
  `[W]` record.
- The table surfaces in `status` as `s0=key s0v=value …` (spec 06 §4);
  `program.state_get` reads it back.

## 3. Display ownership

- `Display` draws and flushes are done by the active controller: the VM in stand-alone
  mode, the serial task in peripheral mode. No commands, no queue — a `screen.draw_*`
  call is a direct `Display` call, and `screen.flush()` pushes the frame (the battery is
  composited by `AppDisplay`).
- The modes are mutually exclusive (spec 01 §2), so the VM and the serial task never both
  touch the display.

## 4. Rendering pipeline

- Stand-alone mode: the program draws and flushes (VM) at its own pace.
- Peripheral mode: the host draws and flushes (serial) with `draw`/`flush` commands.

## 5. Acceptance criteria

- `status` reports the program name, running flag, host flag, and the state table keys
  while a program runs.
- A program's drawing reaches the glass on the plant's next flush, with only the battery
  composited on top.
- The battery and splash cannot be erased by a program, a host, or a page.
- `Wire` is never touched by `serialTask` or `vmTask`.
