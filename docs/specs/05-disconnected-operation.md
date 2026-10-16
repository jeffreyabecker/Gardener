# Spec 05 — Disconnected operation, magic default, break-out

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §6.3, §7.2, §8
and constraint 4.

## 1. Modes, not owners

Control is a mode, not an arrival order:

- **Stand-alone** — the VM runs; the running program commands and draws.
- **Peripheral** — the VM is stopped; the serial system commands and draws.

The plant is always the single writer to the pump and valve; the mode only decides which
task is allowed to command it (spec 01 §2).

## 2. Boot sequence

1. Hardware init and `system_.begin()` (pump off, valve open) — the safe resting state.
2. `ProgramStore::begin()` (seeds `default` and `browser` if missing, spec 02 §4).
3. Load `default`'s persisted params and start `default`.

Boot therefore always lands in a running program; the seeded `default` is passive, so a
stock boot keeps the chamber released until a program or host acts.

## 3. Link-loss policy

Detected by `serialTask` (`(bool)Serial` on the hwcdc CDC). Attaching never changes the
mode. On detach:

| Mode | Action |
| --- | --- |
| Stand-alone | Nothing — the program keeps running. |
| Peripheral | Vent, then switch to stand-alone (run `default`). |

In stand-alone mode `vacuum.host_present()` simply reports false; the device is already
locally controlled. In peripheral mode the controller left, so the device vents and
returns to local control.

## 4. Break-out gestures (stand-alone mode)

From any running program that has not claimed Back (spec 03 §5):

- **Back click** — vent and return to `default` (stop current program → vent → start
  `default`).
- **Back long-press** — vent and run the `browser` program (stop current program → vent →
  run `browser`).

A program that registers `buttons.on(buttons.BACK, fn)` takes these gestures for itself;
the device stops delivering them.

**Holding Back for 5 s is always a hard escape**, claimed or not: the device stops the
program, vents, and runs `default`. A program cannot consume or override it. A program
that claims Back and never yields is likewise stopped and vented by the runaway watchdog
(spec 01 §5).

## 5. Mode transitions

| Event | Effect |
| --- | --- |
| `set mode peripheral` | stop the program, vent, halt the VM |
| `set mode standalone` | start `default` (home) if nothing is running |
| `run <name>` (stand-alone) | stop current program → vent → start `name` |
| `run <name>` (peripheral) | switch to stand-alone, then the same |
| Local Back click | stop → vent → start `default` |
| Peripheral + detach | vent → switch to stand-alone |
| Fault latch / sensor loss | vents and latches regardless of mode; only `reset` clears |

## 6. Safety invariants (unchanged)

- The vent valve is normally open; losing power or control vents the chamber.
- A sensor loss or pump latch mid-run vents and latches `FAULT` no matter who owns the
  run.
- Duty cap and slew limit remain the pump's only protections; they are not relaxed for
  programs.

## 7. Headless operation

`default` is the boot program, so a device with no host is fully self-contained: power on
→ `default` runs; Back long → `browser` → pick a program; Back click → home. No host is
ever required, and no state is lost across a power cycle except the running program
(which restarts as `default`).

## 8. Acceptance criteria

- Attaching USB does not change the mode; the device stays in whatever mode it was in.
- Unplugging USB in peripheral mode vents and switches to stand-alone.
- Unplugging USB in stand-alone mode leaves the running program running.
- Back click from a running program vents and starts `default`; Back long vents and runs
  `browser`.
- `set mode peripheral` stops the program, vents, and halts the VM; `set mode standalone`
  runs `default`.
- Boot with a deleted `default` still boots into the re-seeded passive `default`.
