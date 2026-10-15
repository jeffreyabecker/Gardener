# Spec 05 — Disconnected operation, magic default, break-out

Status: draft spec. Implements [the plan](../berry-on-device-control.md) §6.3, §7.2, §8
and constraint 4.

## 1. Owner model

One current owner at a time:

- **Local owner** — a program was started from the buttons (or is the boot `default`).
- **Host owner** — a host is attached and gave the last `start`/`run`/`set target`.

The plant executes commands in arrival order and is the single writer to the pump and
valve regardless of owner, so "last command wins" and there is never a second control
path.

## 2. Boot sequence

1. Hardware init and `system_.begin()` (pump off, valve open) — the safe resting state.
2. `ProgramStore::begin()` (seeds `default` and `browser` if missing, spec 02 §4).
3. Load `default`'s persisted params and start `default`.

Boot therefore always lands in a running program; the seeded `default` is passive, so a
stock boot keeps the chamber released until a program or host acts.

## 3. Link-loss policy

Detected by `serialTask` (`(bool)Serial` on the hwcdc CDC). On detach:

| Condition | Action |
| --- | --- |
| A program is running | **Keep running.** The program is the local owner; `vacuum.host_present()` flips false. |
| No program is running | Enqueue `Stop` → vent, as today. |

On attach:

- The panel flips to Host mode (spec 04 §8).
- A host that left while a program ran sees it in `status` (`program=<name> running=1
  host=0`) and may `stop`, `run` another program, or draw the panel.
- The host is authoritative while present: any host `run`/`stop`/`set target` stops the
  current program first, then acts.

## 4. Break-out gestures

From any running program that has not claimed Back (spec 03 §5):

- **Back click** — vent and return to `default` (stop current program → vent → start
  `default`).
- **Back long-press** — vent and run the `browser` program (stop current program → vent →
  run `browser`).

A program that registers `buttons.on(3, fn)` takes these gestures for itself; the device
stops delivering them. A program that claims Back and never yields is still stopped and
vented by the runaway watchdog (spec 01 §5).

## 5. Takeover semantics

| Event | Effect |
| --- | --- |
| Local `run` (the `browser` program) | stop current program/run → vent → start selected program with effective params |
| Host `run <name> [k=v]` | same, plus the host is now present and authoritative |
| Host `stop` | stop current program/run → vent → **idle** (does not start `default`) |
| Local Back click | stop → vent → start `default` |
| Host detach during a run | nothing; program keeps running |
| Fault latch / sensor loss | vents and latches regardless of owner; only `reset` clears |

The deliberate asymmetry: local Back click homes to `default`, while host `stop` leaves
the device idle, because the host is present and decides what happens next.

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

- Unplugging USB while `pulse` runs leaves `pulse` running; `status` (after reattach)
  shows `host=0 running=1 program=pulse`.
- Unplugging USB with no program running vents the chamber.
- Back click from a running program vents and starts `default`; Back long vents and runs
  `browser`.
- Host `stop` during a running program vents and leaves the device idle, not running
  `default`.
- Boot with a deleted `default` still boots into the re-seeded passive `default`.
