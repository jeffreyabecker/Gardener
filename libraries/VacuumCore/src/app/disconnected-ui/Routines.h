#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "hal/Button.h"

// The vendored interpreter's VM type, forward declared so its header stops here the way LittleFS.h
// stops at Storage.cpp.
struct bvm;

namespace gardener {

class AppContext;
class Storage;
class StreamLogger;

// The Berry runtime behind the disconnected page: the VM, the routines it has loaded, and the one
// that is active.
//
// One entry per beat. The runtime is the only thing that enters the interpreter, and it does so
// from its own frame — never from inside a Berry call — so there is nothing suspended, no second
// core and no lock. `activate()` runs when a routine becomes active, and `poll(now)` once per pass
// after that.
//
// A routine that raises is dropped and the page falls back to the mark: an error in a script is not
// allowed to leave the glass blank.
class Routines {
 public:
  // The press queue is a fixed ring, because a dropped press is worth a log line and nothing more. The
  // registry is not bounded: how many routines the device has is however many files were put there.
  static constexpr uint8_t kMaxPresses = 8;

  // Creates the VM, installs the native modules, loads every `/routines/*.be` and picks `default`.
  // False only when the interpreter cannot be allocated at all.
  bool begin(AppContext& context, Storage& storage, StreamLogger& logger);

  // The page's way out: leave whichever routine is showing and give the panel back to `default`. The
  // files are not re-read — every routine is still registered — so this is a switch, not a load.
  // False when nothing named `default` is registered, which leaves the mark.
  bool showDefault();

  // One beat: the active routine's poll(now), or nothing when there is no active routine.
  void poll(uint32_t nowMs);

  // The page became visible again: run the active routine's activate(), so a routine re-reads the
  // unit and redraws rather than trusting what it knew before the host attached.
  void enterPage();

  bool active() const { return active_ >= 0; }

  // --- the `routines` module's surface, called from the bindings ---
  // `objectIndex` is the routine object's stack position in the calling native function; the
  // runtime takes its own reference, so the value does not have to stay where it is.
  bool registerRoutine(const char* name, const char* title, int objectIndex);
  bool setActive(const char* name);
  size_t count() const { return entries_.size(); }
  const char* nameAt(size_t index) const;

  // The registered object, pushed onto the VM stack, and whether it has a method of that name. A
  // page that calls the object itself stays in its own Berry frame; the runtime does not re-enter
  // the interpreter from a native call.
  bool pushObject(const char* name) const;
  bool hasMethod(const char* name, const char* method) const;

  // --- the `buttons` module's surface ---
  void pushPress(Button button, Press press);
  bool takePress(Button& button, Press& press);

  // --- the `store` module's surface ---
  // The active routine's own values, kept beside its source in `/routines/<name>.ini`. Read
  // through the file each time rather than cached: the file is the state, so there is no second
  // copy to keep in step.
  //
  // `storeGet` is false when there is no active routine or nothing is stored under the key;
  // `storeSet` is false when there is no active routine or the file would not take the write. The
  // named forms address a registered routine, which is what lets the browser edit the values of a
  // routine that is not the one showing.
  bool storeGet(const char* key, String& out) const;
  bool storeSet(const char* key, const String& value);
  bool storeGet(const char* routine, const char* key, String& out) const;
  bool storeSet(const char* routine, const char* key, const String& value);

 private:
  struct Entry {
    String name;
    String title;
  };

  struct PressEvent {
    Button button;
    Press press;
  };

  bool loadFile(Storage& storage, const String& file);
  bool startActive();
  void reportError(const char* file, const char* what);
  bool invoke(size_t index, const char* method, bool withNow, uint32_t nowMs);
  bool choose(const char* name);
  void selectDefault();
  void applyPending();
  bool storePath(const char* routine, String& out) const;

  bvm* vm_ = nullptr;
  Storage* storage_ = nullptr;
  StreamLogger* logger_ = nullptr;
  // Registration order, and only appended to or replaced in place: a routine's object lives in the VM
  // under its index (`__routine<i>`), so an index has to keep meaning the same routine for as long as
  // the device is up.
  std::vector<Entry> entries_;
  int active_ = -1;

  // A routine that asks to switch routines is asking from inside its own poll; the switch is
  // applied when that call returns rather than re-entering the VM.
  bool insideCall_ = false;
  bool hasPending_ = false;
  String pendingActive_;

  PressEvent presses_[kMaxPresses];
  uint8_t pressHead_ = 0;
  uint8_t pressCount_ = 0;
  bool pressOverflow_ = false;
};

}  // namespace gardener
