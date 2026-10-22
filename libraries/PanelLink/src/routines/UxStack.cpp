#include "routines/UxStack.h"

#include <berry.h>
#include <stdio.h>

#include <Arduino.h>

#include "Logger.h"
#include "PanelTasks.h"

namespace gardener {
namespace {

void sceneKey(char* out, size_t size, size_t slot) {
  snprintf(out, size, "__scene%u", (unsigned int)slot);
}

void callbackKey(char* out, size_t size, size_t slot) {
  snprintf(out, size, "__scene%ucb", (unsigned int)slot);
}

// The deferred operation's holding cells, named so the collector can see what waits there.
constexpr const char* kPendingScene = "__pendingScene";
constexpr const char* kPendingArgs = "__pendingArgs";
constexpr const char* kPendingCallback = "__pendingCallback";
constexpr const char* kPendingValue = "__pendingValue";

// A scene has to look like one before it is kept: the native side still checks, so a push of a
// bare object fails with the missing method named rather than on the first poll.
bool objectHasMethod(bvm* vm, int objectIndex, const char* name) {
  const int object = be_absindex(vm, objectIndex);
  const bool present = be_getmember(vm, object, name);
  const bool function = present && be_isfunction(vm, -1);
  be_pop(vm, 1);  // be_getmember always leaves a slot, nil when the member is not there
  return function;
}

// Copies the value at `index` — or a nil when index is 0 — into a global.
void holdGlobal(bvm* vm, int index, const char* name) {
  if (index > 0) {
    be_pushvalue(vm, index);
  } else {
    be_pushnil(vm);
  }
  be_setglobal(vm, name);
  be_pop(vm, 1);  // be_setglobal leaves the value it stored on the stack
}

}  // namespace

void UxStack::begin(bvm* vm, Logger& logger) {
  vm_ = vm;
  logger_ = &logger;
}

bool UxStack::boot() {
  if (vm_ == nullptr) return false;
  const int base = be_top(vm_);
  const bool found = be_getglobal(vm_, "__rootScene");
  if (!found) {
    be_pop(vm_, be_top(vm_) - base);
    logger_->warn("vm: no root scene, showing the mark");
    return false;
  }
  if (!be_isinstance(vm_, -1)) {
    be_pop(vm_, be_top(vm_) - base);
    logger_->warn("vm: root scene is not an instance, showing the mark");
    return false;
  }
  const int scene = be_absindex(vm_, -1);
  const bool ok = applyPush(scene, 0, 0);
  be_pop(vm_, be_top(vm_) - base);
  return ok;
}

void UxStack::showDefault() {
  pressHead_ = 0;
  pressCount_ = 0;
  pressOverflow_ = false;
  pending_ = Pending::None;
  clear();
  boot();
}

// --- the `ux` surface ------------------------------------------------------

bool UxStack::push(int sceneIndex, int argsIndex, int callbackIndex) {
  if (vm_ == nullptr || sceneIndex <= 0) return false;
  if (insideCall_) {
    holdGlobal(vm_, sceneIndex, kPendingScene);
    holdGlobal(vm_, argsIndex, kPendingArgs);
    holdGlobal(vm_, callbackIndex, kPendingCallback);
    pending_ = Pending::Push;
    return true;
  }
  return applyPush(sceneIndex, argsIndex, callbackIndex);
}

bool UxStack::applyPush(int sceneIndex, int argsIndex, int callbackIndex) {
  if (!objectHasMethod(vm_, sceneIndex, "activate") ||
      !objectHasMethod(vm_, sceneIndex, "poll")) {
    logger_->error("vm: a scene needs activate() and poll()");
    return false;
  }

  const size_t slot = takeSlot();
  char key[24];
  sceneKey(key, sizeof key, slot);
  holdGlobal(vm_, sceneIndex, key);
  callbackKey(key, sizeof key, slot);
  holdGlobal(vm_, callbackIndex, key);

  frames_.push_back({slot, callbackIndex > 0});
  activateTop(argsIndex);
  return true;
}

bool UxStack::pop(int valueIndex) {
  if (vm_ == nullptr) return false;
  if (insideCall_) {
    holdGlobal(vm_, valueIndex, kPendingValue);
    pending_ = Pending::Pop;
    return true;
  }
  return applyPop(valueIndex);
}

bool UxStack::applyPop(int valueIndex) {
  // The root stays: it is what pop_to_root() is for.
  if (frames_.size() <= 1) return false;
  const Frame removed = frames_.back();
  frames_.pop_back();
  deliverValue(removed, valueIndex);
  releaseSlot(removed.slot);
  return true;
}

bool UxStack::popToRoot(int valueIndex) {
  if (vm_ == nullptr) return false;
  if (insideCall_) {
    holdGlobal(vm_, valueIndex, kPendingValue);
    pending_ = Pending::PopToRoot;
    return true;
  }
  return applyPopToRoot(valueIndex);
}

bool UxStack::applyPopToRoot(int valueIndex) {
  if (frames_.size() <= 1) return true;
  while (frames_.size() > 1) {
    releaseSlot(frames_.back().slot);
    frames_.pop_back();
  }
  // The value has nowhere to go: the root has no callback, so it re-activates and the value is
  // dropped.
  activateTop(0);
  return true;
}

bool UxStack::top() const {
  if (vm_ == nullptr || frames_.empty()) return false;
  char key[24];
  sceneKey(key, sizeof key, frames_.back().slot);
  const bool found = be_getglobal(vm_, key);
  // be_getglobal pushes a nil when the name is not there; callers only want a stack slot when the
  // object was found.
  if (!found) be_pop(vm_, 1);
  return found;
}

// --- the beat --------------------------------------------------------------

void UxStack::activateTop(int argsIndex) {
  if (!invokeTop("activate", false, 0, argsIndex)) clear();
}

bool UxStack::invokeTop(const char* method, bool withNow, uint32_t nowMs, int argsIndex) {
  if (vm_ == nullptr || frames_.empty()) return false;
  const int base = be_top(vm_);
  const Frame& frame = frames_.back();
  char key[24];
  sceneKey(key, sizeof key, frame.slot);

  bool ok = false;
  if (!be_getglobal(vm_, key)) {
    logger_->error("vm: scene object missing");
  } else if (!be_getmethod(vm_, -1, method)) {
    String message = "vm: scene has no ";
    message += method;
    logger_->error(message.c_str());
  } else {
    be_pushvalue(vm_, -2);  // self
    int argc = 1;
    if (withNow) {
      be_pushint(vm_, (bint)nowMs);
      argc++;
    } else {
      // activate() always takes one argument: the push's args, or nil when it carried none and the
      // scene reads its file through store.
      if (argsIndex > 0) {
        be_pushvalue(vm_, argsIndex);
      } else {
        be_pushnil(vm_);
      }
      argc++;
    }
    insideCall_ = true;
    const int rc = be_pcall(vm_, argc);
    insideCall_ = false;
    ok = rc == BE_OK;
    if (!ok) reportError(method);
  }

  be_pop(vm_, be_top(vm_) - base);
  return ok;
}

// The value a popped scene returned, handed to the pusher's callback — the callback stored on the
// frame that was just removed — or a re-activation of the scene below when it was pushed without
// one. The callback is the pusher's: a closure that reaches the scene that pushed, so nothing else
// about the pop is retained.
void UxStack::deliverValue(const Frame& removed, int valueIndex) {
  if (!removed.hasCallback) {
    activateTop(0);
    return;
  }

  const int base = be_top(vm_);
  char key[24];
  callbackKey(key, sizeof key, removed.slot);
  if (!be_getglobal(vm_, key) || !be_isfunction(vm_, -1)) {
    be_pop(vm_, be_top(vm_) - base);
    logger_->error("vm: scene callback is missing");
    activateTop(0);
    return;
  }

  // The callback is on the stack; its argument goes above it.
  if (valueIndex > 0) {
    be_pushvalue(vm_, valueIndex);
  } else {
    be_pushnil(vm_);
  }
  insideCall_ = true;
  const int rc = be_pcall(vm_, 1);
  insideCall_ = false;
  if (rc != BE_OK) reportError("callback");
  be_pop(vm_, be_top(vm_) - base);
}

void UxStack::poll(uint32_t nowMs) {
  if (vm_ == nullptr || frames_.empty()) return;
  if (!invokeTop("poll", true, nowMs, 0)) clear();
  applyPending();
}

void UxStack::enterPage() {
  if (vm_ == nullptr || frames_.empty()) return;
  if (!invokeTop("activate", false, 0, 0)) clear();
  applyPending();
}

// A failed call leaves two values: the exception and its argument. The argument is the message,
// and both are dropped so a broken scene leaves the VM as it found it.
void UxStack::reportError(const char* method) {
  String message = "vm: ";
  message += method;
  message += ": ";
  if (be_isstring(vm_, -2)) {
    message += be_tostring(vm_, -2);
    message += ": ";
  }
  const char* detail = be_tostring(vm_, -1);
  message += detail != nullptr ? detail : "error";
  logger_->error(message.c_str());
  be_pop(vm_, 2);
}

void UxStack::applyPending() {
  if (pending_ == Pending::None || insideCall_) return;
  const Pending op = pending_;
  pending_ = Pending::None;

  const int base = be_top(vm_);
  if (op == Pending::Push) {
    be_getglobal(vm_, kPendingScene);
    const int scene = be_absindex(vm_, -1);
    be_getglobal(vm_, kPendingArgs);
    const int args = be_absindex(vm_, -1);
    be_getglobal(vm_, kPendingCallback);
    const int callback = be_absindex(vm_, -1);
    // Clear the held copies first, so an activate() that queues another op does not lose its own.
    holdGlobal(vm_, 0, kPendingScene);
    holdGlobal(vm_, 0, kPendingArgs);
    holdGlobal(vm_, 0, kPendingCallback);
    applyPush(scene, args, callback);
  } else {
    be_getglobal(vm_, kPendingValue);
    const int value = be_absindex(vm_, -1);
    holdGlobal(vm_, 0, kPendingValue);
    if (op == Pending::Pop) {
      applyPop(value);
    } else {
      applyPopToRoot(value);
    }
  }
  be_pop(vm_, be_top(vm_) - base);
}

// --- slots -----------------------------------------------------------------

size_t UxStack::takeSlot() {
  if (!freeSlots_.empty()) {
    const size_t slot = freeSlots_.back();
    freeSlots_.pop_back();
    return slot;
  }
  return nextSlot_++;
}

void UxStack::releaseSlot(size_t slot) {
  if (vm_ != nullptr) {
    char key[24];
    sceneKey(key, sizeof key, slot);
    be_pushnil(vm_);
    be_setglobal(vm_, key);
    be_pop(vm_, 1);
    callbackKey(key, sizeof key, slot);
    be_pushnil(vm_);
    be_setglobal(vm_, key);
    be_pop(vm_, 1);
  }
  freeSlots_.push_back(slot);
}

void UxStack::clear() {
  for (const Frame& frame : frames_) releaseSlot(frame.slot);
  frames_.clear();
}

// --- the gesture queue -----------------------------------------------------

void UxStack::pushPress(const String& gesture) {
  if (gesture.isEmpty()) return;

  if (pressCount_ == kMaxPresses) {
    // The oldest gesture goes so the newest still arrives, and the log says it once rather than on
    // every drop.
    if (!pressOverflow_) {
      pressOverflow_ = true;
      logger_->warn("vm: button queue full, dropping the oldest");
    }
    pressHead_ = (uint8_t)((pressHead_ + 1) % kMaxPresses);
    pressCount_--;
  }

  const uint8_t tail = (uint8_t)((pressHead_ + pressCount_) % kMaxPresses);
  presses_[tail] = gesture;
  pressCount_++;
}

bool UxStack::takePress(String& gesture) {
  if (pressCount_ == 0) return false;
  gesture = presses_[pressHead_];
  presses_[pressHead_] = "";
  pressHead_ = (uint8_t)((pressHead_ + 1) % kMaxPresses);
  pressCount_--;
  return true;
}

// A stop the scene did not make itself — control handed to or from an external link — is told to
// the scene after the fact. The panel task bumps stopSeq on the hand-off, which vents the run, and
// one bump is one true here. The scene that is showing when it happens asks and abandons its run.
bool UxStack::takeStopped() {
  if (lastStopSeq_ == stopSeq) return false;
  lastStopSeq_ = stopSeq;
  return true;
}

}  // namespace gardener
