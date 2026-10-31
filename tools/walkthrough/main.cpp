// The host entry point for the walkthrough tool: reads a command file, drives the panel the device
// would have through the session the check drives, and writes the frames as a GIF.
//
// The tool writes no Berry. `load` and `send` are the calls a launch and a press already take — the
// registry and the stack for a page, the press queue for a gesture — and the three verbs that are
// not the page's are the host's: a wait is beats of the session, a rate is the chamber's model, and
// an emit is the capture written.
//
// The whole file is read before anything is drawn, and every load is resolved against the image
// before the first frame is captured, so a misspelled scene, an unknown routine and a setting key
// nobody declared all fail with a line number rather than a half-written picture.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <any>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Clock.h"
#include "HarnessSession.h"
#include "Script.h"
#include "berry.h"
#include "bindings/BerryScene.h"
#include "bindings/BindingContext.h"
#include "machine/MachineCommand.h"
#include "panel/page/RoutineRegistry.h"
#include "panel/page/Scene.h"
#include "panel/page/SettingsRows.h"
#include "panel/page/Ux.h"
#include "storage/SettingKind.h"

namespace {

using gardener::HarnessSession;
using gardener::SettingValues;
using gardener::panel::page::Scene;
using gardener::panel::page::SettingsRow;
using gardener::panel::page::Ux;
using gardener::walkthrough::Command;

const char* DefaultData = "firmware/data";
const char* DefaultOutput = "build/walk.gif";

// One beat of device time a frame, so a walk plays at the speed it was walked. The check's own
// hundred centiseconds is the check's: it slows a capture down for a reader who is studying it.
constexpr int DefaultDelayCs = 25;

// The scene the image sets as its browser: a global rather than a registry entry, so it is reached
// by name the way the interpreter reaches `__rootScene`.
const char* BrowserGlobal = "browserScene";

// What a load names and what it builds the page with. The panel's root, the browser and a routine
// are the three cases; a routine also carries the set its page is launched with.
struct Target {
  enum class Kind { Home, Browser, Routine };

  Kind kind = Kind::Home;
  std::string key;
  SettingValues values;
};

bool imageHasBrowser(HarnessSession& session) {
  bvm* vm = session.vm();
  const int base = be_top(vm);
  const bool there = be_getglobal(vm, BrowserGlobal) && be_isinstance(vm, -1);
  be_pop(vm, be_top(vm) - base);
  return there;
}

std::unique_ptr<Scene> browserScene(HarnessSession& session) {
  bvm* vm = session.vm();
  const int base = be_top(vm);
  std::unique_ptr<Scene> scene;
  if (be_getglobal(vm, BrowserGlobal) && be_isinstance(vm, -1)) {
    scene = std::make_unique<gardener::bindings::BerryScene>(vm, be_absindex(vm, -1),
                                                             session.bindings());
  }
  be_pop(vm, be_top(vm) - base);
  return scene;
}

// The stop a launch posts before its page: the browser vents whatever was running under it, and a
// load is that same switch.
void stopMachine(HarnessSession& session) {
  session.bindings().post(
      gardener::machine::MachineCommand{gardener::machine::MachineCommand::Op::Stop});
}

// The setting a key names in a record's declaration, or nullptr.
const SettingsRow* declaredRow(const std::vector<SettingsRow*>& rows, const std::string& key) {
  const String wanted(key.c_str());
  for (const SettingsRow* row : rows) {
    if (row->key() == wanted) return row;
  }
  return nullptr;
}

// What a load names: the key the registry holds, and the set its page is built with. Nothing is
// built here, so a file that never reaches its load still costs no page; what is checked is that
// the name resolves and every setting key is one the routine declares.
//
// The set starts from the routine's own document, which is what a launch reads, and each pair
// overrides one key through the row's own parse and accept — the same conversion a saved set takes,
// so a walk and a document cannot disagree about what a value means.
bool resolve(const Command& command, HarnessSession& session, Target& target, std::string& error) {
  gardener::panel::page::RoutineRegistry& registry = session.routines().registry();

  if (command.scene == "home") {
    target.kind = Target::Kind::Home;
  } else if (command.scene == "browser") {
    if (!imageHasBrowser(session)) {
      error = "the image sets no browser scene";
      return false;
    }
    target.kind = Target::Kind::Browser;
  } else {
    target.kind = Target::Kind::Routine;
    target.key = command.scene;
    const String key(command.scene.c_str());
    if (registry.entry(key) == nullptr) {
      error = "no routine '" + command.scene + "'";
      return false;
    }
    target.values = registry.loadSettings(key);
  }

  if (command.settings.empty()) return true;
  if (target.kind != Target::Kind::Routine) {
    error = "load " + command.scene + " takes no settings";
    return false;
  }

  const std::vector<SettingsRow*>* rows = registry.settingsRows(String(target.key.c_str()));
  if (rows == nullptr) {
    error = "no routine '" + target.key + "'";
    return false;
  }
  for (const std::pair<std::string, std::string>& setting : command.settings) {
    const SettingsRow* row = declaredRow(*rows, setting.first);
    if (row == nullptr) {
      error = "'" + target.key + "' does not declare '" + setting.first + "'";
      return false;
    }
    std::any value = row->parse(String(setting.second.c_str()));
    if (!row->accept(value)) {
      error = "'" + target.key + "' does not take '" + setting.second + "' for '" + setting.first +
              "'";
      return false;
    }
    target.values[row->key()] = std::move(value);
  }
  return true;
}

// One page on top: the root first, then a launch's stop, then the page — the path a browser row
// takes. The beat after it is the panel's next frame, which is the one that draws what was pushed.
bool loadPage(const Target& target, HarnessSession& session, std::string& error) {
  Ux& ux = session.routines().ux();
  ux.popToRoot();
  if (target.kind == Target::Kind::Home) {
    session.beat(1);
    return true;
  }

  stopMachine(session);
  std::unique_ptr<Scene> page;
  if (target.kind == Target::Kind::Browser) {
    page = browserScene(session);
    if (!page) {
      error = "the image sets no browser scene";
      return false;
    }
  } else {
    page = session.routines().registry().activate(String(target.key.c_str()), target.values);
    if (!page) {
      error = "'" + target.key + "' answered no page";
      return false;
    }
  }
  if (!ux.push(std::move(page))) {
    error = "the stack would not take '" + target.key + "'";
    return false;
  }
  session.beat(1);
  return true;
}

}  // namespace

int hostMain(int argc, char** argv) {
  const char* data = DefaultData;
  const char* scriptPath = nullptr;
  const char* outPath = DefaultOutput;
  int delayCs = DefaultDelayCs;
  int scale = gardener::RenderScale;
  float ceilingKpa = gardener::ChamberCeilingKpa;
  bool verbose = false;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
      data = argv[++i];
    } else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
      scriptPath = argv[++i];
    } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
      outPath = argv[++i];
    } else if (strcmp(argv[i], "--delay-cs") == 0 && i + 1 < argc) {
      delayCs = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
      scale = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--ceiling-kpa") == 0 && i + 1 < argc) {
      ceilingKpa = (float)atof(argv[++i]);
    } else if (strcmp(argv[i], "--verbose") == 0) {
      verbose = true;
    } else {
      fprintf(stderr, "unknown argument %s\n", argv[i]);
      return 2;
    }
  }

  if (scriptPath == nullptr) {
    fprintf(stderr, "no script; a walkthrough is a file of commands\n");
    return 2;
  }
  if (delayCs < 1 || delayCs > 65535) {
    fprintf(stderr, "--delay-cs takes a frame delay in centiseconds, above zero\n");
    return 2;
  }
  if (scale < 1 || scale > 16) {
    fprintf(stderr, "--scale takes an enlargement of one or more\n");
    return 2;
  }
  if (!(ceilingKpa > 0.0f)) {
    fprintf(stderr, "--ceiling-kpa takes the chamber's deepest depth in kPa, above zero\n");
    return 2;
  }

  std::vector<Command> commands;
  std::string error;
  if (!gardener::walkthrough::readScript(scriptPath, commands, error)) {
    printf("[E] %s\n", error.c_str());
    return 2;
  }

  HarnessSession session(data, ceilingKpa);
  if (!session.begin()) {
    fprintf(stderr, "no data image at %s\n", data);
    return 2;
  }

  // Every load is resolved before the capture is armed: a scene the image does not hold, a key the
  // registry does not know and a setting nobody declared all fail with nothing on disk.
  std::vector<Target> targets(commands.size());
  for (size_t i = 0; i < commands.size(); i++) {
    if (commands[i].verb != Command::Verb::Load) continue;
    if (!resolve(commands[i], session, targets[i], error)) {
      printf("[E] line %d: %s\n", commands[i].line, error.c_str());
      return 2;
    }
  }

  session.beginCapture(outPath);

  for (size_t i = 0; i < commands.size(); i++) {
    const Command& command = commands[i];
    // emit writes the frames and is the run's last line, which the script reader has already held
    // the file to.
    if (command.verb == Command::Verb::Emit) break;
    switch (command.verb) {
      case Command::Verb::Load:
        if (!loadPage(targets[i], session, error)) {
          printf("[E] line %d: %s\n", command.line, error.c_str());
          return 1;
        }
        break;
      case Command::Verb::Send:
        // The strip under the picture names the press, and the queue under the page takes it.
        session.pressGesture(command.gesture.c_str());
        session.routines().pushPress(String(command.gesture.c_str()));
        break;
      case Command::Verb::Wait:
        session.beat(command.beats);
        break;
      case Command::Verb::SetRate:
        session.setChamberRates(command.pullKpaPerSecond, command.ventKpaPerSecond);
        break;
      case Command::Verb::Emit:
        break;
    }
  }

  const int frames = session.capturedFrames();
  const bool ok = session.endCapture((uint16_t)delayCs, scale);

  if (verbose || session.errorLines() > 0) fputs(session.logger().text().c_str(), stdout);

  // A capture with nothing in it writes nothing, which the session does not call a failure because
  // a case that never beats is not one. A walk is a picture, so here it is.
  if (frames == 0) {
    printf("[E] walk: nothing was drawn, so there is no picture to write\n");
    return 1;
  }
  if (!ok) {
    printf("[E] walk: cannot write %s\n", outPath);
    return 1;
  }
  printf("[I] walk: %d frames, %.1f s of device time, saved to %s\n", frames,
         (double)gardener::host::nowMs() / 1000.0, outPath);
  return session.errorLines() == 0 ? 0 : 1;
}
