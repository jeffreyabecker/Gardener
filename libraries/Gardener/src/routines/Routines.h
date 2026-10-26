#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "routines/GestureQueue.h"

// The vendored interpreter's VM type, forward declared so its header stops here.
struct bvm;

namespace gardener {

class PanelContext;
class Logger;
class Library;

// The Berry runtime behind the disconnected page: the VM, the modules and scenes it has loaded, and
// the one entry per beat into the scene stack. The stack itself is `/system/ux.be`; this side calls
// it from its own frame — never from inside a Berry call — so there is nothing suspended, no second
// core and no lock.
//
// The registry the browser drives is a Berry class (`/system/registry.be`) and the scene stack it
// pushes onto is another (`/system/ux.be`); this side keeps only what the firmware has to: the VM,
// the files it loads, the beat that drives the stack, and the gesture queue.
class Routines {
 public:
  // Creates the VM, installs the native modules, loads the files `/system/order` names and then
  // every `/routines/*.be`, and resets the scene stack to the root. The system comes first so a page
  // can use what one of its files defines. False only when the interpreter cannot be allocated at
  // all.
  bool begin(PanelContext& context);

  // The interpreter itself, for the host harness, which loads its own driver into the same VM after
  // the image has loaded.
  bvm* vm() const { return vm_; }

  // The page's way out: leave whichever scenes are showing and give the panel back to the root.
  // False when no root scene is there, which leaves the mark.
  bool showDefault();

  // One beat: the top scene's poll(now). True while a page is showing, so the caller knows whether
  // to draw its own frame.
  bool poll(uint32_t nowMs);

  // The page became visible: run the top scene's activate(), so a scene re-reads the unit and
  // redraws rather than trusting what it knew before. True while a page is showing.
  bool enterPage();

  // Whether a page is showing. The beat takes poll()'s answer instead; this is for the gesture path,
  // which asks once per press.
  bool active() const;

  // --- the `device` module's gesture surface ---
  void pushPress(const String& gesture) { gestures_.push(gesture); }
  bool takePress(String& gesture) { return gestures_.take(gesture); }

 private:
  size_t loadSystem(size_t& found);
  size_t loadAll(const char* folder, size_t& found);
  bool loadFile(const String& path);
  void reportError(const char* file, const char* what);
  bool resetToRoot();
  bool callUx(const char* method, int argIndex) const;

  bvm* vm_ = nullptr;
  Logger* logger_ = nullptr;
  // The files the interpreter loads and the routine documents it reads and writes, from the context
  // at begin(), so nothing here names a filesystem or a path.
  Library* library_ = nullptr;
  GestureQueue gestures_;
};

}  // namespace gardener
