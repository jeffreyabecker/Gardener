# Control task

Paths are relative to the repo root. The control task is the machine's owner: the one writer of
`VacuumSystem`, `Pump`, `PressureSensor`, `Battery`, `BuzzerPlayer`/`Buzzer` and the `Buttons`
debounce state. It is pinned to core 1 above the Arduino loop priority, and it never blocks on the
wire, the flash or the screen.

Files: the body lives in new `app/Tasks.cpp`; `App::update()` is deleted. `VacuumSystem`, `Pump`,
`PressureSensor`, `Battery`, `BuzzerPlayer` and `Buttons` are unchanged — they already expose exactly
the verbs this task needs.

## Tick

The body is in [tasks.md](tasks.md). Each piece:

- `sensor/pump/battery/vacuum/buzzer update` — moved verbatim from `App::update()`, in the same
  order, on a fixed 50 ms beat (`vTaskDelayUntil` with `cfg::CONTROL_PERIOD_MS`). This is the win:
  the PID's `dt` no longer stretches with a slow frame flush or a slow command.
- `drainMachineCommands` — pops every pending `MachineCommand` and applies it (below).
- `buttonsTick` — owns the debounce: `consumePress(Button)` for all four keys, emitting a
  `ButtonEvent` per completed press, and writing `heldMs(Button::Ok)` into the `okHeldMs` scalar the
  panel reads.
- `watchBackHold` — `heldMs(Back) >= 3000` and not yet fired: `vacuum.stop()`, then increment
  `stopSeq`. The `backStopFired_` latch stays, cleared when Back comes up.
- `watchResetHold` — `heldMs(Back) >= 5000 && heldMs(Ok) >= 5000`: `safeReboot(context_)`, exactly
  as today. `safeReboot` owns the sensor and the vacuum system it drives during the vent wait, so it
  stays here and nowhere else.
- `logRunChanges` — moved verbatim; the task holds a `Logger&` to the boot-chosen instance, so run
  transitions write directly (interleaving accepted).

## Applying machine commands

`drainMachineCommands` is the only place `VacuumSystem` and `BuzzerPlayer` are written. Each op:

| Op | Applies |
|---|---|
| `Start` | `vacuum.start()` |
| `Stop` | `vacuum.stop()` |
| `ResetFault` | `vacuum.resetFault()` |
| `SetTarget` | `vacuum.setTarget(value)` |
| `ClearTarget` | `vacuum.clearTarget()` |
| `Reboot` | `safeReboot(context_)` — the release-before-restart loop runs here |
| `Tone` | `buzzer.tone(hz, ms)` |
| `Play` | `buzzer.play(text.c_str())` |
| `Off` | `buzzer.off()` |

`Reboot` moves to this queue because `safeReboot` drives the vacuum system during its vent wait, and
the interface task must not touch that state. Both `device reboot` (serial) and the BLE reboot opcode
now enqueue `Reboot`; the reply leaves the port first, and the restart happens on the next tick.

## The command result is not echoed back

The producer decided the reply before enqueueing (see [queues.md](queues.md) and
[interface-task.md](interface-task.md)). The control task does not answer back: it applies the
command against the live machine, and the next dirty read shows what happened. The one accepted
divergence — a `Start` enqueued because a dirty read said "ready" but refused here because a fault
latched in between — leaves the machine idle and is corrected by the next `vacuum get`.

## Buttons: what stays, what moves

`Buttons` itself is untouched and stays single-owner. The only change is who reads it:

- Press classification (`consumePress`) and the two hold watches now run on the control task, where
  they did on the loop.
- The panel no longer calls `consumePress` or `heldMs` on the shared `Buttons`; it reads the
  `ButtonEvent` queue and the `okHeldMs` scalar instead.
- The ISR-written `raw_` mask is unchanged — it already has one writer (the ISR) and is read with an
  aligned load.
