#include <Arduino.h>

#include <vector>

#include "app/AppContext.h"
#include "app/Logger.h"
#include "app/routines/bindings/Module.h"
#include "app/routines/bindings/Modules.h"
#include "app/storage/Ini.h"
#include "app/storage/Storage.h"

namespace gardener {
namespace berry {
namespace {

constexpr const char* kJinglePath = "/modules/jingles.ini";
constexpr const char* kJingleSection = "jingles";
constexpr char kFieldSeparator = '|';

struct Jingle {
  String id;
  String title;
  String label;
  String notes;
};

std::vector<Jingle> jingles;

const Jingle* findJingle(const char* id) {
  if (id == nullptr) return nullptr;
  for (const Jingle& jingle : jingles) {
    if (jingle.id == id) return &jingle;
  }
  return nullptr;
}

void report(const char* what, const String& id) {
  if (gLogger == nullptr) return;
  String message = "jingles: ";
  message += what;
  if (!id.isEmpty()) {
    message += ' ';
    message += id;
  }
  gLogger->error(message.c_str());
}

// `/modules/jingles.ini` is data, not a `.be` module: the resolver reads it once when the module is
// installed, the same way the runtime reads the module files, so a change takes effect at the next
// reset. An entry is `<id>=<title>|<label>|<notes>`.
void loadJingles() {
  jingles.clear();
  if (gContext == nullptr) return;

  String text;
  if (!gContext->storage().read(kJinglePath, text)) {
    report("missing", "");
    return;
  }

  Ini ini;
  ini.parse(text);
  for (uint8_t i = 0; i < ini.size(); i++) {
    if (ini.sectionAt(i) != kJingleSection) continue;

    const String& value = ini.valueAt(i);
    const int titleEnd = value.indexOf(kFieldSeparator);
    const int labelEnd = titleEnd < 0 ? -1 : value.indexOf(kFieldSeparator, titleEnd + 1);
    if (titleEnd < 0 || labelEnd < 0) {
      report("malformed entry", ini.keyAt(i));
      continue;
    }

    Jingle jingle;
    jingle.id = ini.keyAt(i);
    jingle.title = value.substring(0, titleEnd);
    jingle.label = value.substring(titleEnd + 1, labelEnd);
    jingle.notes = value.substring(labelEnd + 1);
    jingle.title.trim();
    jingle.label.trim();
    jingle.notes.trim();

    if (jingle.id.isEmpty() || jingle.title.isEmpty() || jingle.label.isEmpty() ||
        jingle.notes.isEmpty()) {
      report("empty field", ini.keyAt(i));
      continue;
    }
    jingles.push_back(jingle);
  }

  if (jingles.empty()) report("no entries", "");
}

// --- the module -----------------------------------------------------------

int jinglesIds(bvm* vm) {
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (const Jingle& jingle : jingles) {
    be_pushstring(vm, jingle.id.c_str());
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
    be_pushstring(vm, jingle->title.c_str());
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
    be_pushstring(vm, jingle->label.c_str());
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

// The selected jingle, or the first one when nothing valid has been chosen yet. The settings read
// is a direct one, like device.target(): the identifier is a small preferences field, not a file.
int jinglesCurrent(bvm* vm) {
  String selected;
  if (gContext != nullptr) selected = gContext->settings().jingle();
  const Jingle* jingle = findJingle(selected.c_str());
  if (jingle == nullptr && !jingles.empty()) jingle = &jingles.front();
  if (jingle == nullptr) {
    be_pushnil(vm);
  } else {
    be_pushstring(vm, jingle->notes.c_str());
  }
  be_return(vm);
}

int jinglesDefaultId(bvm* vm) {
  if (jingles.empty()) {
    be_pushnil(vm);
  } else {
    be_pushstring(vm, jingles.front().id.c_str());
  }
  be_return(vm);
}

const bnfuncinfo kJinglesLibrary[] = {
    {"ids", jinglesIds},
    {"title", jinglesTitle},
    {"label", jinglesLabel},
    {"notes", jinglesNotes},
    {"current", jinglesCurrent},
    {"default_id", jinglesDefaultId},
    {nullptr, nullptr},
};

}  // namespace

void installJinglesModule(bvm* vm) {
  loadJingles();
  installModule(vm, "jingles", kJinglesLibrary);
}

}  // namespace berry
}  // namespace gardener
