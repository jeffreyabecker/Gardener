#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "hal/Button.h"

struct bvm;

namespace gardener {

class Logger;

// The scene stack behind the disconnected page: the scenes that are pushed and popped, the one on
// top, and the per-beat drive the firmware gives it. Only the top scene is activated and polled; a
// pop hands the returned value to the scene below's callback, or re-activates it when it has none.
//
// Scenes live in VM globals so the collector can see them: each frame's object sits under
// `__scene<i>` and its optional callback under `__scene<i>cb`. A push or pop asked for from inside
// a scene's own poll is deferred until that Berry call returns, which is what keeps the runtime
// from re-entering the interpreter inside a native call.
//
// A scene that raises in activate() or poll() is dropped with the whole stack: the page falls back
// to the mark, exactly as a routine that raised always has.
class UxStack {
 public:
  static constexpr uint8_t kMaxPresses = 8;

  void begin(bvm* vm, Logger& logger);

  // Reads the well-known `__rootScene` global and pushes it as the root. False when nothing is
  // there, which leaves the mark.
  bool boot();

  // --- the `ux` module's surface ---
  // `sceneIndex`, `argsIndex`, `callbackIndex` and `valueIndex` are stack positions in the calling
  // native function; 0 means the argument was absent (nil).
  bool push(int sceneIndex, int argsIndex, int callbackIndex);
  bool pop(int valueIndex);
  bool popToRoot(int valueIndex);
  // Pushes the top scene onto the VM stack; false when the stack is empty, so the caller answers
  // nil itself.
  bool top() const;
  int depth() const { return static_cast<int>(frames_.size()); }
  bool active() const { return !frames_.empty(); }

  // --- the beat ---
  void poll(uint32_t nowMs);
  void enterPage();
  // Leave whichever scenes are showing and give the panel back to the root: the clear-and-reboot
  // the device's go-home gesture and a controller hand-off both use.
  void showDefault();

  // --- the press queue and the stop notice ---
  void pushPress(Button button, Press press);
  bool takePress(Button& button, Press& press);
  bool takeStopped();

 private:
  struct Frame {
    size_t slot;
    bool hasCallback;
  };

  struct PressEvent {
    Button button;
    Press press;
  };

  bool invokeTop(const char* method, bool withNow, uint32_t nowMs, int argsIndex);
  void activateTop(int argsIndex);
  void deliverValue(const Frame& removed, int valueIndex);
  void clear();
  bool applyPush(int sceneIndex, int argsIndex, int callbackIndex);
  bool applyPop(int valueIndex);
  bool applyPopToRoot(int valueIndex);
  void applyPending();
  void reportError(const char* method);

  size_t takeSlot();
  void releaseSlot(size_t slot);

  bvm* vm_ = nullptr;
  Logger* logger_ = nullptr;
  std::vector<Frame> frames_;
  std::vector<size_t> freeSlots_;
  size_t nextSlot_ = 0;

  // One deferred operation at a time. A scene asks to push or pop from inside its own poll; the
  // change is applied when that call returns rather than re-entering the VM.
  enum class Pending : uint8_t { None, Push, Pop, PopToRoot };
  bool insideCall_ = false;
  Pending pending_ = Pending::None;

  PressEvent presses_[kMaxPresses];
  uint8_t pressHead_ = 0;
  uint8_t pressCount_ = 0;
  bool pressOverflow_ = false;
  // The stopSeq the last takeStopped() answered, so one bump is one true.
  uint32_t lastStopSeq_ = 0;
};

}  // namespace gardener
