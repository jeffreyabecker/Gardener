#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "routines/UxStack.h"

// The vendored interpreter's VM type, forward declared so its header stops here.
struct bvm;

namespace gardener {

class PanelContext;
class Logger;
class Library;

// The Berry runtime behind the disconnected page: the VM, the modules and scenes it has loaded,
// and the scene stack that is showing. The stack owns the top scene and is driven one entry per
// beat from this class's own frame — never from inside a Berry call — so there is nothing
// suspended, no second core and no lock.
//
// The registry the browser drives is a Berry class (`/system/registry.be`); this side keeps only
// what the firmware has to: the VM, the files it loads, the scene stack it polls, the gesture
// queue, and the `store` surface that reads and writes a routine's own document.
class Routines {
 public:
  // Creates the VM, installs the native modules, loads the files `/system/order` names, then every
  // `/modules/*.be` and then every `/routines/*.be`, and pushes the root scene. The system and the
  // modules come first so a page can use what one of them defines. False only when the interpreter
  // cannot be allocated at all.
  bool begin(PanelContext& context);

  // The interpreter itself, for the host harness, which loads its own driver into the same VM after
  // the image has loaded.
  bvm* vm() const { return vm_; }

  // The page's way out: leave whichever scenes are showing and give the panel back to the root.
  // False when no root scene is there, which leaves the mark.
  bool showDefault();

  // One beat: the top scene's poll(now), or nothing when the stack is empty.
  void poll(uint32_t nowMs);

  // The page became visible: run the top scene's activate(), so a scene re-reads the unit and
  // redraws rather than trusting what it knew before.
  void enterPage();

  bool active() const { return stack_.active(); }

  // --- the `ux` module's surface, called from the bindings ---
  // Positions on the VM stack in the calling native function; 0 means the argument was absent.
  bool push(int sceneIndex, int argsIndex, int callbackIndex) {
    return stack_.push(sceneIndex, argsIndex, callbackIndex);
  }
  bool pop(int valueIndex) { return stack_.pop(valueIndex); }
  bool popToRoot(int valueIndex) { return stack_.popToRoot(valueIndex); }
  bool top() const { return stack_.top(); }
  int depth() const { return stack_.depth(); }

  // --- the `buttons` module's surface ---
  void pushPress(const String& gesture) { stack_.pushPress(gesture); }
  bool takePress(String& gesture) { return stack_.takePress(gesture); }

  // --- the `store` module's surface ---
  // A routine's own values, kept in the routine's own document by the settings source the context
  // gave. The name is a plain word: the document is named after it, so a path separator would
  // address another file.
  bool storeGet(const char* routine, const char* key, String& out);
  bool storeSet(const char* routine, const char* key, const String& value);

 private:
  size_t loadSystem(size_t& found);
  size_t loadAll(const char* folder, size_t& found);
  bool loadFile(const String& path);
  void reportError(const char* file, const char* what);

  bvm* vm_ = nullptr;
  Logger* logger_ = nullptr;
  // The files the interpreter loads and the routine documents it reads and writes, from the context
  // at begin(), so nothing here names a filesystem or a path.
  Library* library_ = nullptr;
  UxStack stack_;
};

}  // namespace gardener
