# Epic 1 — The stop path

A stop reaches the machine from any link, cannot be dropped by a full command queue, and cannot be
undone by a start queued before it.

## What changed

- **Ungated stop.** `vacuum stop` is dispatched before `requireControl`, and the Bluetooth `0x02`
  opcode is handled before the ownership check, so a bystander on either transport vents the
  chamber. Both answer exactly as an owner's stop does.
- **A queue that delays rather than drops.** `postMachineCommand` returns the send result, and every
  link carries a `CommandOutbox` (`Link` in `Ownership.h`) that holds what the queue could not take
  and retries it at the top of the link's next poll. The queue is still the only way into the
  machine, so the control task is still the one writer.
- **A drained queue is collapsed.** `drainMachineCommands` takes the whole queue at once: a stop
  discards every command queued before it, and repeated target writes reduce to the last one, each
  collapse logged as `queue: dropped <op> <value> before a stop` or `for a later target`.
- **An idempotent stop.** Stopping a machine that is already releasing returns without touching the
  vent timer, so a second stop cannot stretch a release.
- **The panel's cancel hold.** Cancel held for five seconds stops and vents whatever link drives;
  the panel no longer has to own the machine to make it safe.

## Verification

All three builds pass (`gardener`, `--no-enable-bluetooth`, `headless`) and the host harness
compiles and runs. The queued-burst and ownership checks in the backlog are bench checks.
