# Spec 07 — Snapshot and display ownership

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §9, §10 and
constraints 5 and 6.

## 1. Snapshot

`SystemSnapshot` is plain, copyable, pointer-free data — the cross-task view of what the
machine *is*. The plant rewrites one object each tick; `serialTask` and `vmTask` read its
fields directly (spec 01 §3). No lock — a read may span two adjacent ticks, which is fine
for a readout.

```cpp
struct SystemSnapshot {
  PlantState plant;               // the whole plant readout (simplified VacuumSystem)

  bool batteryPresent = false;
  bool batteryLow = false;
  float batteryVolts = 0.0f;
  uint8_t batteryPercent = 0;
  uint32_t uptimeMs = 0;

  ProgramStatus program;          // what the VM is doing, written by the runtime
  uint8_t stateCount;
  ProgramStatePair state[8];      // program state table
};
```

- The plant rewrites the object every tick. `ProgramStatus` and `state[]` are the two
  fields it does not own: the runtime writes the first, the running program the second
  (spec 03 §6).
- **No program list.** What is *stored* is not what the machine *is*, and the store already
  answers for it on demand: `ProgramStore::count()`/`at()` back both `list` on the wire and
  the Berry `programs.*` verbs (spec 02 §7). A copy here would be a second source of truth —
  `ProgramInfo::running` would restate `ProgramStatus`, `isDefault` would restate the
  `default` name — for no gain.

## 2. Program state table

- `program.state_set("label", 42.5)` writes directly into `snapshot_.state[]` — the VM
  writes, the plant reads it into `status`. Up to 8 keys, key and value each ≤ 11 chars.
- Setting an existing key replaces it; a ninth key replaces the oldest (FIFO) and logs a
  `[W]` record.
- The table surfaces in `status` as `s0=key s0v=value …` (spec 06 §4);
  `program.state_get` reads it back.

## 3. Display ownership

- `Display` draws and flushes are done by the active controller and by nobody else: the
  program (VM) in stand-alone mode at its own pace, the serial task in peripheral mode with
  the `draw`/`flush` commands. No commands, no queue — a `screen.draw_*` call is a direct
  `Display` call, and `screen.flush()` pushes the frame (the battery is composited by
  `Overlay`).
- The modes are mutually exclusive (spec 01 §2), so the VM and the serial task never both
  touch the display.

## 4. Acceptance criteria

- `status` reports the program name, running flag, host flag, and the state table keys
  while a program runs.
- A program's drawing reaches the glass when the program flushes, with only the battery
  composited on top.
- `Wire` is touched by exactly two tasks — the plant (sensor reads every tick) and the
  active controller (display flushes) — and the interleaving corrupts neither (spec 01 §3).
- The battery and splash cannot be erased by a program or a host.
