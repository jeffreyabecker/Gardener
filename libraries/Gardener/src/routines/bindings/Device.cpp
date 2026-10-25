#include "Build.h"

#if GARDENER_PANEL

#include <vector>

#include "BuzzerPlayer.h"
#include "Logger.h"
#include "PanelContext.h"
#include "panel/Units.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"
#include "storage/Config.h"
#include "storage/DeviceConfig.h"
#include "storage/Jingles.h"

namespace gardener {
namespace berry {
namespace {

// The settings the device keeps as itself rather than as a routine's own values. `units()` is the
// table `panel/Units.h` holds, one `unit` record per entry, so a page cycles exactly what
// `device set unit` accepts and reads a unit's label and scale without carrying a second list.
int deviceUnits(bvm* vm) {
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (uint8_t i = 0; i < DisplayUnitCount; i++) {
    const DisplayUnit unit = static_cast<DisplayUnit>(i);
    const DisplayUnitInfo& info = displayUnitInfo(unit);
    const int record = beginRecord(vm, "unit");
    if (record < 0) be_return_nil(vm);
    setText(vm, record, "name", info.name);
    setText(vm, record, "suffix", info.suffix);
    setReal(vm, record, "per_kpa", info.perKpa);
    setReal(vm, record, "whole_step", info.wholeStep);
    setInt(vm, record, "decimals", (bint)displayDecimals(unit));
    be_data_push(vm, list);
    be_pop(vm, 1);
  }
  be_pop(vm, 1);
  be_return(vm);
}

int deviceSetUnit(bvm* vm) {
  DisplayUnit unit = DisplayUnit::Kpa;
  const bool accepted =
      be_isstring(vm, 1) && parseDisplayUnit(argText(vm, 1), unit);
  if (accepted) {
    context->config().set(DeviceSection, UnitKey, displayUnitInfo(unit).name);
  }
  be_pushbool(vm, accepted);
  be_return(vm);
}

// The target the default program pulls to; 0 when none has been set. The page clamps a new value to
// the machine's bounds before it writes it, and the default page clamps again when it restores one.
int deviceTarget(bvm* vm) {
  const float target = context->config().get<float>(DeviceSection, TargetKey, 0.0f);
  be_pushreal(vm, target > 0.0f ? target : 0.0f);
  be_return(vm);
}

int deviceSetTarget(bvm* vm) {
  context->config().set(DeviceSection, TargetKey, (float)argReal(vm, 1));
  be_return_nil(vm);
}

// The jingle library the panel read at boot, taken once when this module is installed: the entries
// point into the document, so they stay valid until the next parse. A section that is not a jingle,
// and a sequence this build cannot play, are reported at boot rather than when a page asks.
std::vector<const Jingle*> entries;

const Jingle* findJingle(const char* id) {
  if (id == nullptr) return nullptr;
  for (const Jingle* jingle : entries) {
    if (jingle->key == id) return jingle;
  }
  return nullptr;
}

void report(const char* what, const String& id) {
  if (logger == nullptr) return;
  String message = "jingles: ";
  message += what;
  if (!id.isEmpty()) {
    message += ' ';
    message += id;
  }
  logger->error(message.c_str());
}

// `/cfg/jingles.ini` is data, not a `.be` module: the panel reads it at boot and this binding
// takes the entries when it is installed, so a change takes effect at the next reset.
void takeJingleLibrary() {
  entries.clear();

  const JinglesConfig& library = context->jingles();
  for (const String& section : library.incompleteSections()) report("incomplete entry", section);
  for (const Jingle& jingle : library.jingles()) {
    if (!BuzzerPlayer::validate(jingle.notes.c_str())) {
      report("bad notes", jingle.key);
      continue;
    }
    entries.push_back(&jingle);
  }

  if (entries.empty()) report("no entries", "");
}

// The selected jingle, or the first one when nothing valid has been chosen yet.
const Jingle* currentJingle() {
  const String id = context->config().get<String>(DeviceSection, JingleKey, "");
  const Jingle* jingle = findJingle(id.c_str());
  if (jingle == nullptr && !entries.empty()) jingle = entries.front();
  return jingle;
}

// The radio is a build fact, not a preference: the row reports whether this image has one, and
// nothing a page can call changes that.
int deviceBluetooth(bvm* vm) {
  be_pushbool(vm, GARDENER_BLUETOOTH != 0);
  be_return(vm);
}

// The selected jingle's identifier, or "" when none has been chosen. The list resolves an empty or
// unknown value to its first entry; the system page writes one of the identifiers it reads there.
int deviceJingle(bvm* vm) {
  const String id = context->config().get<String>(DeviceSection, JingleKey, "");
  be_pushstring(vm, id.c_str());
  be_return(vm);
}

int deviceSetJingle(bvm* vm) {
  if (!be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "device.set_jingle(id)");
  }
  context->config().set(DeviceSection, JingleKey, String(argText(vm, 1)));
  be_return_nil(vm);
}

// Every jingle this build can play, as the `jingle` record /system/records.be declares, in file
// order. A page that wants one entry's sequence reads the list rather than asking for it by name.
int deviceAllJingles(bvm* vm) {
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (const Jingle* jingle : entries) {
    const int record = beginRecord(vm, "jingle");
    if (record < 0) be_return_nil(vm);
    setText(vm, record, "key", jingle->key.c_str());
    setText(vm, record, "full_title", jingle->full_title.c_str());
    setText(vm, record, "short_title", jingle->short_title.c_str());
    setText(vm, record, "notes", jingle->notes.c_str());
    be_data_push(vm, list);
    be_pop(vm, 1);
  }
  be_pop(vm, 1);
  be_return(vm);
}

// The finish jingle a routine sounds: the selected one, played on the panel's buzzer. False when the
// library is empty, so the sequence did not start.
int devicePlayCurrentJingle(bvm* vm) {
  const Jingle* jingle = currentJingle();
  if (jingle == nullptr) {
    be_pushbool(vm, false);
    be_return(vm);
  }
  context->buzzer().play(jingle->notes.c_str());
  be_pushbool(vm, true);
  be_return(vm);
}

const bnfuncinfo DeviceLibrary[] = {
    {"units", deviceUnits},
    {"set_unit", deviceSetUnit},
    {"target", deviceTarget},
    {"set_target", deviceSetTarget},
    {"bluetooth", deviceBluetooth},
    {"jingle", deviceJingle},
    {"set_jingle", deviceSetJingle},
    {"all_jingles", deviceAllJingles},
    {"play_current_jingle", devicePlayCurrentJingle},
    {nullptr, nullptr},
};

}  // namespace

void installDeviceModule(bvm* vm) {
  takeJingleLibrary();
  installModule(vm, "device", DeviceLibrary);
}

}  // namespace berry
}  // namespace gardener
#endif
