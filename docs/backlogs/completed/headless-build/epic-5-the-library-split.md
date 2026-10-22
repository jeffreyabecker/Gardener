# Epic 5 — The library split

One `AppContext` graph becomes two: a machine context and a panel context, with
the panel files then moved out into a library that depends on the machine's.
Phase A did the split inside `VacuumCore`, so the build kept working with the
panel code where it was; Phase B moved the files.

## What changed

- **Two graphs.** `MachineContext` holds the sensor, pump, vent, vacuum system,
  ownership stack, capability record and config. `PanelContext` holds the
  buttons, display, buzzer, battery, overlay, routines, storage, settings and the
  store bus, and reaches the machine through `machine()`. `AppContext` became a
  holder of the two by reference.
- **Everything re-routed.** The control and interface beats read the machine
  context; the panel beat and the pages read the panel context. The battery
  update, the buzzer tick and the store drain moved to the panel beat, and
  `TickService` was deleted.
- **The wire split by owner.** The machine registers `control`, `vacuum` and its
  BLE domain; the panel registers `device`, `fs`, `buzzer` and its own BLE domain
  with `unit` and `batt`. `Tone`, `Play` and `Off` left the machine's command
  queue, and the buzzer domain and the VM's buzzer bindings call the panel's
  player directly.
- **The boot and the tasks split.** `machineBoot` brings the machine's parts up,
  `panelBoot` brings the panel up, and each side starts its own tasks.
- **The files moved.** 62 of them: the panel's HAL interfaces and drivers, the
  overlay, the pages, the fonts and assets, routines and all nine bindings and
  the binding surface, the buzzer player, storage, ini and settings, the store
  bus, the panel's serial domains with `PanelDomains.h`, and `PanelBleDomain`.
  `Tasks.h` kept only the machine's two tasks and no button header.
- **`VacuumCore` verified clean.** It includes nothing panel-shaped and links
  only the ESP32 core and NimBLE — a sketch including only `VacuumCore.h`
  compiles and links at 566521 bytes with no Adafruit, Berry or `GardenerPanel`
  reference in the build.

## What did not land

Three items were hardware checks and were not done: that the full device still
boots after the split, that both wire surfaces are unchanged, and that the panel
still drives by default. The behavioural pass over both builds is the parking
lot's "Verify the two builds on hardware".

## Verification

Both sketches build after every item, and a clean build of each carried through
to the end of the epic. The full device is 873089 bytes (66%) with 30612 bytes of
static RAM; the machine-alone build is 568897 bytes (43%) with 29516 bytes.
