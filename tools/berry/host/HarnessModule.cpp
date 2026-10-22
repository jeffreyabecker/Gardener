#include "HarnessSession.h"

#include <berry.h>

#include <cstring>
#include <string>

#include "hal/Button.h"
#include "routines/bindings/Module.h"

namespace {

// One VM and one app, like the firmware's bindings: the native callbacks are plain functions with
// no user data, so they reach the session through this.
using gardener::Button;
using gardener::HarnessSession;
using gardener::Press;

HarnessSession* gSession = nullptr;

Button asButton(const char* name) {
  if (name != nullptr && strcmp(name, "down") == 0) return Button::Down;
  if (name != nullptr && strcmp(name, "ok") == 0) return Button::Ok;
  if (name != nullptr && strcmp(name, "back") == 0) return Button::Back;
  return Button::Up;
}

Press asPress(const char* name) {
  if (name != nullptr && strcmp(name, "long") == 0) return Press::Long;
  if (name != nullptr && strcmp(name, "double") == 0) return Press::Double;
  if (name != nullptr && strcmp(name, "chord") == 0) return Press::Chord;
  return Press::Short;
}

// --- the drive -------------------------------------------------------------

int harnessBeat(bvm* vm) {
  gSession->beat((int)be_toint(vm, 1));
  be_pushbool(vm, gSession->active());
  be_return(vm);
}

int harnessPress(bvm* vm) {
  if (be_top(vm) < 2 || !be_isstring(vm, 1) || !be_isstring(vm, 2)) {
    be_raise(vm, "type_error", "harness.press(btn, press)");
  }
  gSession->routines().pushPress(asButton(be_tostring(vm, 1)), asPress(be_tostring(vm, 2)));
  be_return_nil(vm);
}

// The device's Back hold: the notice a scene reads through vacuum.stopped() and the stop command,
// which are what panelTask fires together.
int harnessBackHold(bvm* vm) {
  gSession->backHold();
  be_return_nil(vm);
}

int harnessReset(bvm* vm) {
  gSession->reset();
  be_return_nil(vm);
}

// --- what the driver asks about -------------------------------------------

int harnessSave(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "harness.save(path)");
  }
  be_pushbool(vm, gSession->panel().save(be_tostring(vm, 1)));
  be_return(vm);
}

int harnessLog(bvm* vm) {
  be_pushstring(vm, gSession->logger().text().c_str());
  be_return(vm);
}

int harnessWrote(bvm* vm) {
  if (be_top(vm) < 2 || !be_isstring(vm, 1) || !be_isstring(vm, 2)) {
    be_raise(vm, "type_error", "harness.wrote(routine, key)");
  }
  const std::string path = std::string("/routines/") + be_tostring(vm, 1) + ".cfg";
  be_pushbool(vm, gSession->files().wrote(path, be_tostring(vm, 2)));
  be_return(vm);
}

int harnessBuzzerSounds(bvm* vm) {
  be_pushint(vm, gSession->buzzerPart().sounds());
  be_return(vm);
}

const bnfuncinfo kHarnessLibrary[] = {
    {"beat", harnessBeat},      {"press", harnessPress},     {"back_hold", harnessBackHold},
    {"reset", harnessReset},    {"save", harnessSave},       {"log", harnessLog},
    {"wrote", harnessWrote},    {"buzzer_sounds", harnessBuzzerSounds},
    {nullptr, nullptr},
};

}  // namespace

void gardener::installHarnessModule(bvm* vm, HarnessSession& session) {
  gSession = &session;
  gardener::berry::installModule(vm, "harness", kHarnessLibrary);
}
