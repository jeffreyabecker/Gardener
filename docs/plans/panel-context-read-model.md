# One read model for the panel context

## What this is

`PanelContext` is the whole of what a routine's native modules reach: the panel's own parts, with
the machine's wiring and the links left outside. Eleven virtuals, and the five this plan is about
are the ones a page reads the world through — `config()`, `vacuum()`, `battery()`,
`serialConnected()` and `bluetoothConnected()`. Not one of their call sites does anything but read.

Two of the five hand over an object with a mutating half no page may use, one hands over a whole
INI document for two keys, and two are flags. The plan folds all five into one value,
`SystemState`, assembled out of the owners when it is asked for. The two writes a page could make
into `/cfg/settings.ini` turn out to be reachable from no page at all, and deleting them is what
lets the document leave the boundary instead of being replaced by something else. What is left of it
is two values, so the plan gives them a typed owner, `DeviceSettings`, and the fold asks it for an
enum and a float rather than parsing text on every beat.

## What is wrong now, and where

| Member | Call sites |
| --- | --- |
| `config()` | `Device.cpp` 28-29, 38, 44-45; `Settings.cpp` 184; `DrawTree.cpp` 346; `Routines.cpp` 159 |
| `vacuum()` | `Vacuum.cpp` 25, 32, 63, 88; `DrawTree.cpp` 305; `Routines.cpp` 175 |
| `battery()` | `Routines.cpp` 160 |
| `serialConnected()`, `bluetoothConnected()` | `Routines.cpp` 172 |

Every one of those calls is a getter. `config()` is reached for two keys of the document,
`[device] unit` and `[device] target`; `vacuum()` for `targetPressureKpa`, `currentPressureKpa`,
`startRefusal`, `runState` and `running`; `battery()` for `present`, `percent` and `low`; and the
two flags for the runes the overlay draws. The two other `battery()` callers — `PanelTasks.cpp` 70
and the BLE `describe()` — take it from the concrete `panel::Panel`, not from the context, so the
virtual costs them nothing.

**The surface is wider than the use.** A page holds a live `VacuumSystem`, `Battery` and `Config`,
so it can call `vacuum().service()`, `vacuum().sensor().update()`, `battery().update()` and
`config().set()`. Nothing does, and none of it is a page's to do: the control tick owns the first
three and the wire owns the fourth. The header states the leniency as a policy — "whoever needs the
unit or the target under `[device]` reads and writes that key here" — and that sentence is the
reason the coupling spreads.

**The unit is parsed three times.** `Routines.cpp` 157-159, `DrawTree.cpp` 344-346 and
`Settings.cpp` 182-184 are the same four lines: read `[device] unit` as a `String`, parse it, keep
the `DisplayUnit`. Three copies of one lookup, each building a `String` for the value on its own
beat.

**And the document has no type.** `storage/DeviceConfig.h` is three string constants — `device`,
`unit`, `target` — and every caller spells the section and the key, then parses the text back into
what it means. Neither value has a home: `parseDisplayUnit` is called at six sites, three of them
to re-read the document, and `displayUnitInfo` turns the enum back into a name at eight. What
stands between the document and a caller is a spelling it has to know.

**The readout takes the control law.** `panel::page::pressureReadout` asks for a `VacuumSystem&` to
print two numbers, so `PressureReadout.h` includes `machine/VacuumSystem.h` and the paint holds a
reference into the machine for as long as it is alive.

**And two writes on the boundary are dead.** `device.set_unit` and `device.set_target` are
registered at `Device.cpp` 113 and 115, and no `.be` file under `firmware/data` or `tools/berry`
calls either — the registration lines are the only two occurrences in the tree. The key
`set_target` writes, `[device] target`, is not in the shipped `firmware/data/cfg/settings.ini` and
nothing in the firmware writes it; `home.be` 43 reads it and gets 0 on every device that has not
had the file edited by hand. They are the only reason `config()` cannot simply be removed.

## The shape of the change

One struct, `panel/SystemState.h`, one type per file, holding what the panel reads:

```cpp
struct SystemState {
  RunState runState;
  bool running;
  float currentPressureKpa;
  float targetPressureKpa;
  bool canStart;  // startRefusal() == nullptr; the panel never formats the reason
  uint8_t batteryPercent;
  bool batteryPresent;
  bool batteryLow;
  bool serialConnected;
  bool bluetoothConnected;
  DisplayUnit unit;
  float deviceTargetKpa;
};
```

| Field | Consumed by |
| --- | --- |
| `runState` | the overlay's state cell, and `vacuum.state()` |
| `running`, the two pressures | `pressure_readout`, and the two pressure getters |
| `canStart` | `vacuum.start()`'s answer |
| `batteryPercent`, `batteryPresent`, `batteryLow` | the overlay's pack cell |
| `serialConnected`, `bluetoothConnected` | the overlay's link cell |
| `unit` | the overlay, `drawTree`'s one resolution, `settings`' `displayUnit()` |
| `deviceTargetKpa` | `device.target()` |

Every field has a caller in the table above; there is no field waiting for one. `canStart` is a bool
rather than `startRefusal`'s text because the only caller on this side of the boundary tests it for
null — the serial domain is the one that prints the reason, and it holds the `Panel`.

`virtual SystemState systemState() = 0;` replaces the five. It is **assembled out of the owners on
each call and never stored**: no field anywhere holds the last one, and no beat refreshes a copy.
That is the firmware skill's "reads go through the owner … there is no snapshot or second copy to
keep in step", kept — the struct is a projection, not a mirror — and it is what keeps freshness a
property of the read rather than of the beat.

`PanelContext` goes from eleven virtuals to seven: `display`, `canvas`, `systemState`, `buzzer`,
`library`, `logger`, `post`. `Panel` loses three overrides — `vacuum()` exists only so a context
virtual resolves to `Machine`'s own accessor, and the two flags stop being virtual — while
`logger()` stays one, resolving to `Machine::logger()` the same way. `settings()`, the typed
document, and `battery()` stay as plain accessors for what holds a `Panel` itself: the serial and
BLE unit writes, `PartsTick` and the BLE `describe()`.

`display()` and `canvas()` both stay, and the canvas is why the pair cannot collapse into the
display: `DisplayDevice::rootCanvas()` is a factory that returns a fresh canvas on every call, and a
`Canvas` is neither copyable nor movable because it owns the rectangle of pixels written since the
last flush — the dirty record a `child()` shares with the canvas it came from. There has to be
exactly one root of that record, so the panel builds it once over the display's buffer and
`canvas()` hands out a reference to that one. Fetching a canvas from the display at each use would
give each caller its own empty rectangle, and the beat's one flush would send nothing. What would
let `canvas()` go — a device that holds its root canvas and answers with a reference — is deferred
in [the parking lot](../parking-lot.md).

`drawTree`, `paintOf` and `childrenOf` take `const SystemState&` instead of `PanelContext&`, and
`pressureReadout` takes it too, capturing a copy so the paint owns its numbers. The two flags, the
unit and the pressures then arrive as values, and the drawing path stops including
`panel/PanelContext.h`.

`device.set_unit` and `device.set_target` are deleted with their registration lines.
`device.target()` stays: `home.be` calls it.

Files it touches: `panel/PanelContext.h`, a new `panel/SystemState.h` and `SystemState.cpp`, a
one-line const accessor in `machine/Machine.h`, a new `storage/DeviceSettings.{h,cpp}` in place of
`storage/DeviceConfig.h`, `panel/Panel.{h,cpp}`, `routines/Routines.cpp`,
`BerryHost/src/{Vacuum,Device,Settings,DrawTree}.cpp`, `panel/page/PressureReadout.{h,cpp}`,
`serial/domains/Device.cpp`, `ble/domains/Panel.cpp`, `tools/berry/host/HarnessSession.{h,cpp}`,
`tools/unit/cases/panel_ticks.cpp`, and `Bindings.h`'s list of what `device.*` carries.

Checked: with the five gone, every call site in the table above compiles against one function, and
nothing under `libraries/BerryHost/src/` includes anything under `storage/`, directly or through
`PanelContext.h`.

## Who assembles it, and who publishes

Three things are easy to call one here — supplying the parts, folding them, and publishing — and
only the first two are real.

**Supplying the parts is the context's**, because only a concrete context knows what it holds. A
fake is legitimately a panel with a `VacuumSystem`, a pack and a document and no machine, so it
supplies what it has. **The fold is the type's**, one function:

```cpp
// panel/SystemState.h, defined in SystemState.cpp
SystemState readSystemState(const VacuumSystem& vacuum, const Battery& battery,
                            const DeviceSettings& settings, bool serialConnected,
                            bool bluetoothConnected);
```

Every read it needs is `const`, so the fold is a pure function of its arguments, and each
implementation's whole body is one call into it: `Panel` with `Machine::vacuum()`, `*battery_`,
`settings_` and the two `Machine` flags, `HarnessPanel` with `true, true`, `PageContext` with
`false, false`. The field-to-source mapping — which field comes from which part, how `running` is
derived, that `canStart` is `startRefusal() == nullptr` — then exists once. Leaving the mapping in
each implementation was the first version of this plan and it does not survive review: three copies
of it, one of them compiled by nothing that runs. It is a read and not a command, so
`systemState()` is `const`, which costs one line in `machine/Machine.h` — a
`const VacuumSystem& vacuum() const` beside the accessor that is already there.

The definition sits in `SystemState.cpp` rather than in the header so that `PanelContext.h`, which
includes `SystemState.h` for the struct, does not hand `storage/Config.h` to every `BerryHost`
module transitively — the same edge this plan removes at the call sites.

**What changes, and what does not.** `runState` and the two pressures move on the control tick, 50
ms. The pack is sampled every 500 ms and its percent moves over minutes. The two flags are written
on every interface beat, 5 ms, and change on attach or detach. The unit changes only when the wire
writes it, and `deviceTargetKpa` never does. So most of the fold is member reads with nothing to
recompute, and the two that are not are the document's — which the next section turns into member
reads as well. Caching the slow ones was the alternative and it has nothing left to win once the
read is a field. A cache would need an invalidator, and the only writer that could supply one is the
wire's, on another task.

**Nothing publishes it**, and that is the design rather than an omission: there is no published copy
to publish. The state is built where it is consumed, on the panel task — once a beat for the
overlay, once per tree for the drawing — and never stored, and never handed to another task. Its
three consumers pull it: `Routines::updateOverlay`, `drawTree`, and the Berry modules through
`berry::context`.

## One object for the device's document

`storage/DeviceConfig.h` today is three `constexpr const char*` names and nothing else. The device's
document holds `[device] unit` and `[device] target`, and both are reached by spelling the section
and the key and turning the text back into a `DisplayUnit` or a float, at every site that wants one.

What is true instead: `storage/DeviceSettings.{h,cpp}` owns `/cfg/settings.ini` and answers in its
own vocabulary.

```cpp
class DeviceSettings {
 public:
  explicit DeviceSettings(Library* store);
  bool begin();
  DisplayUnit unit() const { return unit_; }
  void setUnit(DisplayUnit unit);
  float targetKpa() const { return targetKpa_; }
  void setTargetKpa(float pressureKpa);
  bool save();
 private:
  Config document_;
  DisplayUnit unit_ = DisplayUnit::Kpa;
  float targetKpa_ = 0.0f;
};
```

`Config` is not what collapses. It is the INI engine two families of document share:
`RoutineSettingStore` and `RoutineRegistry` read `/cfg/routines/*.ini` through it, and `Fs` uses its
`ConfigPath` to refuse a write to it. What changes is the device's use of the engine, not the
engine. The section and the two keys move inside the class, and `DeviceConfig.h` goes.

`unit_` and `targetKpa_` are not a second copy to keep in step. The document is read once, in
`begin()`, and held in memory from then on, so a parsed field is exactly as fresh as the text behind
it; every write goes through the setters, which set both; and nothing else can reach the file, since
the `fs` domain refuses `ConfigPath` at both its write and its remove. That leaves the class one
rule to keep: its document stays private. A caller handed the `Config` could set a key the fields do
not know about, and then the fields would be stale.

`Panel` holds a `DeviceSettings` in place of `config_`. `Panel::config()` becomes
`Panel::settings()`, the BLE domain's `describe` and `poll` and the serial `device` domain's `get`
and `set` call `unit()`, `setUnit()` and `save()` instead of spelling a key, and `Panel::boot()`'s
log line reads `settings_.unit()`.

Checked: `storage/DeviceConfig.h` is gone, and the document's `[device] unit` text is turned into a
`DisplayUnit` in exactly one place, `DeviceSettings::begin()`. The wire still parses the token a
controller sends, which is a different string.

## What was set aside

**Posting the two preference writes as machine commands.** It was the other way to keep `config()`
off the boundary, and it does not work here. It would not make the document single-writer: `unit` is
also written by the interface task (`ble/domains/Panel.cpp` 97, `serial/domains/Device.cpp` 210), so
a command applied by the control tick adds a third writer on the other core rather than removing
one. It would put a LittleFS write inside `ControlTick::tick`, ahead of the sensor update and
`service()`, on the 50 ms beat. And the unit is a display preference, not a machine write, so
`applyMachineCommand` would gain a `storage/Config.h` edge to set it. `Reboot`'s precedent does not
transfer either: the tick parks it and the composition performs it, but the hand-back target there
is the composition's own thread, and there is no channel back to the panel task. With the two ops'
only callers dead, all of this is moot — the writes that remain are the wire's, on the `Panel`.

**A cached state refreshed once a beat.** Cheaper to read and a second copy to keep in step, with
freshness decided by the beat rather than by the caller, which is exactly what the skill's rule
forbids.

**`Panel` holding the only fold, with the fakes handed a value.** Then the harness has to refresh
that value on its own beat, because nothing else does — a stored copy, a publisher, and captures
that go stale. It is the cached state again with a different owner.

**One state a beat, threaded from `Routines` into the draw.** The overlay folds one and `drawTree`
folds another, so a beat folds the parts twice. That is what happens today — the overlay reads the
unit, the pack, the flags and the run state while the tree reads the unit and the vacuum — so
nothing regresses; making it one means a state parameter on `Scene::draw` and on every scene in the
image, which is a change to the scene interface for no observable gain.

**Keeping `config()` for `deviceTargetKpa`, and moving only the other four.** The smaller diff, and
it leaves the document on the boundary for one float — the whole point is that no page reaches the
file.

**Replacing `Config` itself with the typed object.** It would take the routine documents with it:
`RoutineSettingStore` and `RoutineRegistry` read `/cfg/routines/*.ini` through the same engine, and
the keys there are declared by rows at runtime, so no class could name them.

**Leaving `DeviceConfig.h`'s constants and passing a section and a key at each call site.** The
status quo, and the reason `parseDisplayUnit` has six callers.

**Keeping `device.set_unit` and `device.set_target` because a page might want them.** One caller
means no surface. A page that wants the default target set can be given one when it exists.

## The entry points it has to hold on

The overlay (`Routines::updateOverlay`) draws four cells a beat, two of them the links, and its
pack cell is `-1` when no gauge is fitted. `pressure_readout` prints `--` while the machine is not
running and the target either way. `vacuum.get_target_pressure`, `get_current_pressure`, `state`
and `start` answer from values rather than from the system. `settings`' `displayUnit()` and
`drawTree`'s single resolution both take the unit as a number, so a document that holds no
`[device] unit` line still renders in kPa. And the two fakes — `HarnessPanel` and the unit case's
`PageContext` — must fold their own parts through that one function rather than being handed a
state.

## What could go wrong

`unit` and `deviceTargetKpa` are preferences rather than readings, which is the one place the
struct's name is a compromise. If a later reader minds, they leave and the context grows a second
accessor; the cost of that is a second name on the boundary, not a broken shape.

`PanelContext.h` gains an include for `RunState` where it forwards-declares `VacuumSystem` today, so
the header is heavier while the surface is smaller. Keeping `SystemState` in its own header bounds
that to the one include the type needs.

`pressureReadout`'s comment says it prints what is true when it is drawn; a captured copy makes it
print what was true when the tree was built, so the comment has to be reworded. Nothing observable
changes, because `drawTree` builds and paints inside one call.

A missed field is silent: the overlay cell or the readout goes blank or prints zero and the build is
clean. That is what the case in the verification below is for.

`DeviceSettings`' fields are only as good as its privacy. The moment a caller is handed the `Config`
it holds, a key can change under the cached unit and target — so the document stays a private
member, which is a rule and not a style preference.

The rename from `config()` to `settings()` reaches the BLE and serial domains, which the unit suite
does not compile. Only the build sees those two.

## Consequences

`libraries/BerryHost/src/` stops including `storage/Config.h` and `storage/DeviceConfig.h`.
`Vacuum.cpp` keeps `machine/Machine.h` for the two target bounds and `machine/VacuumSystem.h` for
`runStateName`, so the parking lot's "four modules reach past `PanelContext`" becomes two —
`machine/Machine.h` and `panel/parts/BuzzerPlayer.h` — and the line is amended rather than deleted.
This plan does not move the seam into the host library; it shrinks what crosses the seam to one
value and the one command path, which is what makes a seam declared there possible later.

The unit is one function instead of three copies of four lines, and it returns an enum:
`readSystemState` asks `settings.unit()`. The document is touched only in `DeviceSettings::begin()`
and its two setters, so the beats that draw stop reading it — where today the overlay and the tree
each look it up once a beat.

`storage/DeviceConfig.h` is gone and `storage/Config.h` is unchanged. The device's document is
reached through two getters and two setters, `unit`, `setUnit`, `targetKpa` and `setTargetKpa`,
beside `begin` and `save`; the wire's unit reads stop parsing text they already had, and
`PanelContext.h` no longer hands a caller anything that can write a key.

The routine surface loses two functions: `device.set_unit` and `device.set_target`. No image calls
either, so `Bindings.h`'s list of what `device.*` carries drops the unit it used to set.

`device.target()` keeps reading `[device] target`, a key nothing in the firmware writes and the
shipped document does not carry. That is true today as well — its only writer is a function nothing
calls — and the plan does not decide whether the key should survive.

The race the change does not fix: `Config` is mutated and saved from the interface task and the
panel task with no lock, and `FileLibrary::write` is a bare open, print and close. The change
narrows it — the panel's readers stop walking `sections_`, and the panel's own writes go with the
two deleted functions — but writer against writer is still there, on one object that a lock would
go in if anyone decides to close it.

## Deferred

- One owner for `/cfg/settings.ini`. Two tasks reach it — the interface task, from the BLE domain
  and the serial domain's `set`, and the panel task from the two functions this plan deletes — and
  nothing locks it: the document's `set` can reallocate the vector a reader is walking, and
  `FileLibrary::write` has nothing around it. `DeviceSettings` is one object for the document and
  the place a lock would go, and after this plan its readers no longer walk the vector. Closing the
  rest still means the wire's synchronous `ok unit=…` reply changing shape, so it is its own
  decision and not this plan's.
- The buzzer half of the seam: `device.tone`, `play_sequence` and `silence` are actions, not
  readings, so `buzzer()` stays on the context and `BuzzerPlayer.h` keeps crossing it.
- The two target bounds. `CONTROL_FLOOR_KPA` and `CONTROL_MIN_TARGET_KPA` are read straight from
  `machine/Machine.h` by `vacuum.get_deepest_pressure`, `get_shallowest_pressure` and `clamp`. On
  the state they would be one more pair of fields; off it they are the last machine include in the
  module.
- Whether `[device] target` survives at all, now that nothing writes it.

## How it is known to hold

1. `python tools/unit/check.py` compiles `PanelContext.h`, `Panel.h`, `Panel.cpp`, `Routines.cpp`,
   `PressureReadout.cpp` and `storage/DeviceSettings.cpp`, and runs `panel_ticks`, which drives the
   overlay through `Routines::poll` with the `PageContext` fake. The existing cases — one frame per
   beat whether it drew or not, one send when a page goes up — must stay green with the fake folding
   its own parts through `readSystemState` rather than handing them over.
2. A new case in `tools/unit/cases/panel_ticks.cpp` drives two `PageContext`s whose parts differ in
   one thing each and compares the overlay's `display.frame(0)`: a different unit, a different run
   state, a different pack and a different pair of links must each change the pixels. This is the
   case that catches a dropped field, which nothing else does. `tools/unit/check.py` is the check
   that compiles this component; the Gfx component check the project notes name,
   `tools/gfx/check.py`, is not in the tree.
3. A new case in `tools/unit/cases/device_settings.cpp`, over the same in-memory store the
   `config.cpp` cases use: `begin()` on a document with no `[device]` section reads kPa and 0, a
   unit name the document does not hold reads kPa, and `setUnit` and `setTargetKpa` followed by
   `save()` and a second `begin()` read back what was set — with a key the class does not know still
   in the file.
4. `python scripts/arduino.py --task build` compiles `libraries/BerryHost` and `libraries/Runtime`,
   neither of which the unit suite takes. It is the only compile the four module edits, the two wire
   domains and the `config()` to `settings()` rename get.
5. On the device, `python scripts/arduino.py --task upload`: the overlay still shows the unit, both
   link runes, the run state and the pack, and a run started from a page still draws its reading
   and its target. `device get unit` and the BLE unit characteristic still read and write the
   document, and the unit survives a reboot. An image that called `device.set_unit` would now raise
   a missing-member error, which is the deletion showing.
6. The harness edit in `tools/berry/host/HarnessSession.{h,cpp}` is compiled by nothing that runs:
   the image's host check is in a known broken state and is not run, and `tools/unit/check.py`
   excludes `tools/berry`. The edit is mechanical — the same parts through the same fold — but it
   stays unverified until that check runs again.