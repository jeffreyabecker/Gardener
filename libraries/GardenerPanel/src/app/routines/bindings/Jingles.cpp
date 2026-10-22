#include <Arduino.h>

#include <vector>

#include "app/PanelContext.h"
#include "app/BuzzerPlayer.h"
#include "app/Logger.h"
#include "app/routines/bindings/Module.h"
#include "app/routines/bindings/Modules.h"
#include "app/storage/Storage.h"

namespace gardener {
namespace berry {
namespace {

constexpr const char* kJinglePath = "/modules/jingles.txt";
constexpr char kFieldSeparator = '|';
constexpr char kCommentStart = '#';

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

// `/modules/jingles.txt` is data, not a `.be` module: the resolver reads it once when the module is
// installed, the same way the runtime reads the module files, so a change takes effect at the next
// reset. A line is `<id>|<title>|<label>|<notes>`, the file is not a settings document, and a `#`
// only starts a comment at the start of a line so a sharp stays a note. The sequence is handed to
// the player's own parser here, so a jingle that could not be played is reported at boot rather
// than when a routine asks for it.
void loadJingles() {
  jingles.clear();
  if (gContext == nullptr) return;

  String text;
  if (!gContext->storage().read(kJinglePath, text)) {
    report("missing", "");
    return;
  }

  int start = 0;
  while (start <= (int)text.length()) {
    int end = text.indexOf('\n', (unsigned int)start);
    if (end < 0) end = (int)text.length();
    String line = text.substring((unsigned int)start, (unsigned int)end);
    start = end + 1;
    line.trim();
    if (line.length() == 0 || line[0] == kCommentStart) continue;

    const int idEnd = line.indexOf(kFieldSeparator);
    const int titleEnd = idEnd < 0 ? -1 : line.indexOf(kFieldSeparator, (unsigned int)idEnd + 1);
    const int labelEnd =
        titleEnd < 0 ? -1 : line.indexOf(kFieldSeparator, (unsigned int)titleEnd + 1);
    if (labelEnd < 0) {
      report("malformed entry", idEnd < 0 ? line : line.substring(0, (unsigned int)idEnd));
      continue;
    }

    Jingle jingle;
    jingle.id = line.substring(0, (unsigned int)idEnd);
    jingle.title = line.substring((unsigned int)idEnd + 1, (unsigned int)titleEnd);
    jingle.label = line.substring((unsigned int)titleEnd + 1, (unsigned int)labelEnd);
    jingle.notes = line.substring((unsigned int)labelEnd + 1);
    jingle.id.trim();
    jingle.title.trim();
    jingle.label.trim();
    jingle.notes.trim();

    if (jingle.id.isEmpty() || jingle.title.isEmpty() || jingle.label.isEmpty() ||
        jingle.notes.isEmpty()) {
      report("empty field", jingle.id);
      continue;
    }
    if (!BuzzerPlayer::validate(jingle.notes.c_str())) {
      report("bad notes", jingle.id);
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
const Jingle* currentJingle() {
  String selected;
  if (gContext != nullptr) selected = gContext->settings().jingle();
  const Jingle* jingle = findJingle(selected.c_str());
  if (jingle == nullptr && !jingles.empty()) jingle = &jingles.front();
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
  gContext->buzzer().play(jingle->notes.c_str());
  be_pushbool(vm, true);
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
    {"play_current", jinglesPlayCurrent},
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
