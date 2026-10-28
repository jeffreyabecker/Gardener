#include "Build.h"

#if GARDENER_PANEL

#include <Arduino.h>

#include <vector>

#include "panel/BuzzerPlayer.h"
#include "core/Logger.h"
#include "panel/PanelContext.h"
#include "panel/Units.h"
#include "routines/Routines.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"
#include "storage/Config.h"
#include "storage/DeviceConfig.h"
#include "storage/Jingles.h"

namespace gardener {
namespace berry {
namespace {

// The five fields a `unit` record carries, left on the stack as the record itself so a caller can
// answer one or push it onto a list.
int pushUnitRecord(bvm* vm, DisplayUnit unit) {
  const DisplayUnitInfo& info = displayUnitInfo(unit);
  const int record = beginRecord(vm, "unit");
  if (record < 0) return record;
  setText(vm, record, "name", info.name);
  setText(vm, record, "suffix", info.suffix);
  setReal(vm, record, "per_kpa", info.perKpa);
  setReal(vm, record, "whole_step", info.wholeStep);
  setInt(vm, record, "decimals", (bint)displayDecimals(unit));
  return record;
}

// The settings the device keeps as itself rather than as a routine's own values. `units()` is the
// table `panel/Units.h` holds, one `unit` record per entry, so a page cycles exactly what
// `device set unit` accepts and reads a unit's label and scale without carrying a second list.
// `unit()` is the one in force, out of that same table, which is what a page formats and steps a
// target with.
int deviceUnits(bvm* vm) {
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (uint8_t i = 0; i < DisplayUnitCount; i++) {
    if (pushUnitRecord(vm, static_cast<DisplayUnit>(i)) < 0) be_return_nil(vm);
    be_data_push(vm, list);
    be_pop(vm, 1);
  }
  be_pop(vm, 1);
  be_return(vm);
}

int deviceUnit(bvm* vm) {
  DisplayUnit unit = DisplayUnit::Kpa;
  parseDisplayUnit(context->config().get<String>(DeviceSection, UnitKey, "").c_str(), unit);
  if (pushUnitRecord(vm, unit) < 0) be_return_nil(vm);
  be_return(vm);
}

int deviceSetUnit(bvm* vm) {
  DisplayUnit unit = DisplayUnit::Kpa;
  const bool accepted =
      be_isstring(vm, 1) && parseDisplayUnit(argText(vm, 1), unit);
  if (accepted) {
    context->config().set(DeviceSection, UnitKey, displayUnitInfo(unit).name);
    context->config().save();
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
  context->config().save();
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

// The two ways a controller can reach the device, as the interface owner last reported them: the
// band's serial and Bluetooth icons light while each link is up, whether or not it has control.
int deviceLinks(bvm* vm) {
  const int record = beginRecord(vm, "links");
  if (record < 0) be_return_nil(vm);
  setFlag(vm, record, "serial", context->serialConnected());
  setFlag(vm, record, "bluetooth", context->bluetoothConnected());
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
  context->config().save();
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

// A sequence of note/duration pairs, the same notation the jingle library holds: false when it is
// malformed, and nothing is played.
int devicePlaySequence(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "device.play_sequence(notes)");
  }
  const char* sequence = argText(vm, 1);
  const bool ok = BuzzerPlayer::validate(sequence);
  if (ok) context->buzzer().play(sequence);
  be_pushbool(vm, ok);
  be_return(vm);
}

// A tone with a duration; 0 ms means until silence(). A frequency outside the machine's range is
// clamped rather than dropped, and a non-positive one is silence.
int deviceTone(bvm* vm) {
  if (be_top(vm) < 2) {
    be_raise(vm, "type_error", "device.tone(hz, duration_ms)");
  }

  float hz = (float)argReal(vm, 1);
  const bint duration = argInt(vm, 2);
  if (hz <= 0.0f || duration < 0) {
    context->buzzer().off();
    be_return_nil(vm);
  }
  if (hz < BuzzerMinHz) hz = BuzzerMinHz;
  if (hz > BuzzerMaxHz) hz = BuzzerMaxHz;
  context->buzzer().tone((uint16_t)hz, (uint32_t)duration);
  be_return_nil(vm);
}

// Silence, and stop whatever is playing.
int deviceSilence(bvm* vm) {
  context->buzzer().off();
  be_return_nil(vm);
}

// `millis()`, the clock a page times against. The device's rather than the machine's: a page that
// touches no machine call still has one.
int deviceNowMs(bvm* vm) {
  be_pushint(vm, (bint)millis());
  be_return(vm);
}

// One `[I]` diagnostic record, written when the device's log set includes `I`. What a page reports
// lands where the firmware's own lines do, which is why the sink is the panel's logger rather than
// anything of the VM's.
int deviceLog(bvm* vm) {
  if (be_top(vm) >= 1 && be_isstring(vm, 1) && logger != nullptr) {
    logger->info(argText(vm, 1));
  }
  be_return_nil(vm);
}

// A pull, not a callback: the queue is drained by the routine when it is ready, so no Berry code
// runs while a native call is on the stack.
int deviceTakeGesture(bvm* vm) {
  String gesture;
  if (routines == nullptr || !routines->takePress(gesture)) be_return_nil(vm);
  be_pushstring(vm, gesture.c_str());
  be_return(vm);
}

// Whether a path names a document under /cfg: it is relative, it is not empty, and no segment of it
// climbs. A page that names a document must not reach one the store does not own.
bool underConfig(const String& path) {
  if (path.length() == 0 || path[0] == '/') return false;
  int start = 0;
  while (start <= (int)path.length()) {
    int end = path.indexOf('/', (unsigned int)start);
    if (end < 0) end = (int)path.length();
    if (path.substring((unsigned int)start, (unsigned int)end) == "..") return false;
    start = end + 1;
  }
  return true;
}

// device.config(path): a Config over /cfg/<path>, read once and held, so a page can keep a document
// and read or write it with the class's own methods rather than a call per value.
int deviceConfig(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "device.config(path)");
  }
  const String path(argText(vm, 1));
  if (!underConfig(path)) {
    be_raise(vm, "value_error", "device.config(path): a path under /cfg");
  }

  String document("/cfg/");
  document += path;
  pushConfigDocument(vm, document.c_str());
  be_return(vm);
}

const bnfuncinfo DeviceLibrary[] = {
    {"unit", deviceUnit},
    {"units", deviceUnits},
    {"set_unit", deviceSetUnit},
    {"target", deviceTarget},
    {"set_target", deviceSetTarget},
    {"bluetooth", deviceBluetooth},
    {"links", deviceLinks},
    {"jingle", deviceJingle},
    {"set_jingle", deviceSetJingle},
    {"all_jingles", deviceAllJingles},
    {"play_current_jingle", devicePlayCurrentJingle},
    {"play_sequence", devicePlaySequence},
    {"tone", deviceTone},
    {"silence", deviceSilence},
    {"take_gesture", deviceTakeGesture},
    {"log", deviceLog},
    {"now_ms", deviceNowMs},
    {"config", deviceConfig},
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
