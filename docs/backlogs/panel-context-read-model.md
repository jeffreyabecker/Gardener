# Panel context read model

The steps of [one read model for the panel context](../plans/panel-context-read-model.md), in the
order they land. Each one leaves the tree building and the checks the plan named still passing.

## The document has a type

- [x] `DeviceSettings` owns `/cfg/settings.ini` and answers `unit()` and `targetKpa()`, with a case
      over the in-memory store: no `[device]` section, a name it does not hold, and a set and save
      that reads back with a key it does not own still in the file.

## The panel reads one value

- [ ] `PanelContext` replaces `config()`, `vacuum()`, `battery()` and the two connection flags with
      `systemState()`, and `Panel`, `Routines`, `PressureReadout` and the two fakes fold through
      `readSystemState`.
- [ ] The four `BerryHost` modules read the state, and `device.set_unit` and `device.set_target` are
      deleted with their registration lines.
- [ ] `Panel` holds the settings object, the serial and BLE domains call its named members, and
      `storage/DeviceConfig.h` is deleted.
- [ ] A case drives two states through the overlay and compares the frame it sent.

## Afterwards

- [ ] The parking lot's Berry host dependency line counts two modules rather than four.
- [ ] This backlog and its plan are deleted.
