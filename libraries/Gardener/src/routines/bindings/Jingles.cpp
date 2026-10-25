#include "Build.h"

#if GARDENER_PANEL

#include <Arduino.h>

#include <vector>

#include "PanelContext.h"
#include "BuzzerPlayer.h"
#include "Logger.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"
#include "storage/Config.h"
#include "storage/DeviceConfig.h"
#include "storage/Jingles.h"

namespace gardener {
namespace berry {
namespace {

// The entries this build can play, pointing into the library the panel read at boot. A sequence the
// player refuses is reported once and left out, so nothing a page asks for is unplayable.
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

// `/system/jingles.ini` is data, not a `.be` module: the panel reads it at boot and this binding
// takes the entries when the module is installed, so a change takes effect at the next reset. A
// section the document could not make a jingle of, and a sequence this build cannot play, are
// reported here rather than when a routine asks for one.
void takeJingles() {
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

// --- the module -----------------------------------------------------------

int jinglesIds(bvm* vm) {
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (const Jingle* jingle : entries) {
    be_pushstring(vm, jingle->key.c_str());
    be_data_push(vm, list);
    be_pop(vm, 1);
  }
  be_pop(vm, 1);
  be_return(vm);
}

int jinglesTitle(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "jingles.title(id)");
  }
  const Jingle* jingle = findJingle(argText(vm, 1));
  if (jingle == nullptr) {
    be_pushnil(vm);
  } else {
    be_pushstring(vm, jingle->full_title.c_str());
  }
  be_return(vm);
}

int jinglesLabel(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "jingles.label(id)");
  }
  const Jingle* jingle = findJingle(argText(vm, 1));
  if (jingle == nullptr) {
    be_pushnil(vm);
  } else {
    be_pushstring(vm, jingle->short_title.c_str());
  }
  be_return(vm);
}

int jinglesNotes(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "jingles.notes(id)");
  }
  const Jingle* jingle = findJingle(argText(vm, 1));
  if (jingle == nullptr) {
    be_pushnil(vm);
  } else {
    be_pushstring(vm, jingle->notes.c_str());
  }
  be_return(vm);
}

// The selected jingle, or the first one when nothing valid has been chosen yet. The selection is
// the device's own `[device] jingle` key in /settings.ini.
const Jingle* currentJingle() {
  if (context == nullptr) return nullptr;
  const String id = context->config().get<String>(DeviceSection, JingleKey, "");
  const Jingle* jingle = findJingle(id.c_str());
  if (jingle == nullptr && !entries.empty()) jingle = entries.front();
  return jingle;
}

int jinglesCurrent(bvm* vm) {
  const Jingle* jingle = currentJingle();
  if (jingle == nullptr) {
    be_pushnil(vm);
  } else {
    be_pushstring(vm, jingle->notes.c_str());
  }
  be_return(vm);
}

// The finish jingle a routine sounds: the selected one, played directly on the panel's buzzer.
// False when the library is empty, so the sequence did not start.
int jinglesPlayCurrent(bvm* vm) {
  const Jingle* jingle = currentJingle();
  if (jingle == nullptr) {
    be_pushbool(vm, false);
    be_return(vm);
  }
  context->buzzer().play(jingle->notes.c_str());
  be_pushbool(vm, true);
  be_return(vm);
}

int jinglesDefaultId(bvm* vm) {
  if (entries.empty()) {
    be_pushnil(vm);
  } else {
    be_pushstring(vm, entries.front()->key.c_str());
  }
  be_return(vm);
}

const bnfuncinfo JinglesLibrary[] = {
    {"ids", jinglesIds},
    {"title", jinglesTitle},
    {"label", jinglesLabel},
    {"notes", jinglesNotes},
    {"current", jinglesCurrent},
    {"play_current", jinglesPlayCurrent},
    {"default_id", jinglesDefaultId},
    {nullptr, nullptr},
};

}  // namespace

void installJinglesModule(bvm* vm) {
  takeJingles();
  installModule(vm, "jingles", JinglesLibrary);
}

}  // namespace berry
}  // namespace gardener
#endif
