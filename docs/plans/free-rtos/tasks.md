# Tasks, boot and timing

Paths are relative to the repo root. This is the skeleton the other sub-plans fill in: how the three
tasks come up, what each loop body is, and the cadences.

## Boot

`setup()` keeps the existing single-threaded bring-up in [App::begin](libraries/VacuumCore/src/app/App.cpp)
unchanged — Wire1 and the panel, the sensor, pump, vent, battery, buzzer, storage, settings,
routines, links — because that order and its boot report are the device's contract. Then, still in
`setup()`, the three queues are created and the three tasks are spawned. `loop()` becomes
`vTaskDelete(NULL)`, which frees the Arduino loop task.

`buildAppContext()` keeps constructing every part once and never deleting them; the parts are the
same object graph, just now reached from three task bodies instead of one `App::update()`.

## Task creation

| Task | Core | Priority | Stack | Wake |
|---|---|---|---|---|
| Control | 1 | 2 | 4096 | `vTaskDelayUntil`, 50 ms |
| Interface | 0 | 1 | 4096 | `vTaskDelay`, 5 ms poll |
| Panel / VM | 0 | 0 | 8192 | `vTaskDelay`, 10 ms |

- Control sits above the Arduino loop's default priority (1) so the safety tick is never preempted by
  a link or the screen; core 1 is where the loop used to run.
- Interface and Panel share core 0 with the NimBLE host task. Panel is the lowest priority because a
  routine's `poll` may take its time; it may starve, it may not starve the wire or the machine.
- Panel's stack is 8 KiB because the Berry interpreter runs inside it; the other two are 4 KiB.
- Pin them with `xTaskCreatePinnedToCore`. Queue handles are plain globals in `app/Tasks.cpp`, read
  by the task bodies and written nowhere after boot.

## Loop bodies

### Control (core 1)

```cpp
for (;;) {
  const uint32_t now = millis();
  drainMachineCommands();          // apply queued writes to VacuumSystem / BuzzerPlayer
  context_.sensor().update(now);
  context_.pump().update(now);
  context_.battery().update(now);
  context_.vacuum().service(now);
  context_.buzzer().update(now);
  buttonsTick(now);                // settle + classify; emit ButtonEvents; publish okHeldMs
  watchBackHold(now);              // Back 3 s -> vacuum.stop() + increment stopSeq
  watchResetHold(now);             // Back+OK 5 s -> safeReboot()
  logRunChanges(now);
  vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(cfg::CONTROL_PERIOD_MS));
}
```

The 50 ms period comes from `cfg::CONTROL_PERIOD_MS`; the PID's `dt` stays the tick time it already
assumes.

### Interface (core 0)

```cpp
for (;;) {
  const uint32_t now = millis();
  storeServer_.drain();             // Settings / Storage writes, including the store round-trips
  link_->poll(now);                // serial: parse, reply, enqueue machine writes
  ble_->poll(now);                 // BLE: applyPending / publish, enqueue machine writes
  vTaskDelay(pdMS_TO_TICKS(5));
}
```

Serial parsing is the same non-blocking `available()` loop as today, so the 5 ms delay only bounds
the polling rate, not the reply latency (a queued machine write still lands on the next 50 ms control
tick).

### Panel / VM (core 0)

```cpp
for (;;) {
  const uint32_t now = millis();
  drainButtonEvents();             // route presses: to the VM when a routine is active, else log
  if (context_.routines().active()) {
    context_.routines().poll(now);
  } else {
    if (now - lastFrameMs_ >= cfg::UI_REFRESH_MS) { lastFrameMs_ = now; drawFrame(); }
    watchDefaultHold(now);         // OK 3 s -> vacuum stop via machine queue + showDefault()
  }
  vTaskDelay(pdMS_TO_TICKS(10));
}
```

The static pages redraw at `cfg::UI_REFRESH_MS`; a routine's `poll(now)` runs on the 10 ms beat, which
is the "once per pass" contract from [docs/specs/routines.md](docs/specs/routines.md) with a fixed
beat instead of "as fast as `loop()` returned".

## What `App::update()` becomes

The monolithic [App::update](libraries/VacuumCore/src/app/App.cpp) is deleted. Its lines map to the
three bodies above:

- `sensor/pump/battery/vacuum/buzzer update`, the two hold watches, run logging → Control.
- `link_->poll`, `ble_->poll`, link-lost handling, `StoreServer::drain` → Interface.
- `watchResetHold` stays Control; `watchDefaultHold` and the screen swap move to Panel.

`App` itself shrinks to `begin()` plus the task-spawning glue; the three loop bodies live in
`app/Tasks.cpp`, which is the only new file.
