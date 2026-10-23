#include "HarnessSession.h"

#include <berry.h>

#include <cstring>
#include <string>

#include "routines/bindings/Module.h"

namespace {

// One VM and one app, like the firmware's bindings: the native callbacks are plain functions with
// no user data, so they reach the session through this.
using gardener::HarnessSession;

HarnessSession* gSession = nullptr;

// --- the drive -------------------------------------------------------------

int harnessBeat(bvm* vm) {
  gSession->beat((int)be_toint(vm, 1));
  be_pushbool(vm, gSession->active());
  be_return(vm);
}

int harnessPress(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "harness.press(gesture)");
  }
  const char* gesture = be_tostring(vm, 1);
  gSession->pressGesture(gesture);
  gSession->routines().pushPress(gesture);
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

// --- the capture -----------------------------------------------------------

int harnessCaptureStart(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "harness.capture_start(path)");
  }
  gSession->beginCapture(be_tostring(vm, 1));
  be_return_nil(vm);
}

int harnessCaptureFinish(bvm* vm) {
  be_pushbool(vm, gSession->endCapture());
  be_return(vm);
}

int harnessCaptureStem(bvm* vm) {
  be_pushstring(vm, gSession->captureStem().c_str());
  be_return(vm);
}

int harnessIsGif(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "harness.is_gif(path)");
  }
  const char* path = be_tostring(vm, 1);
  const size_t len = std::strlen(path);
  be_pushbool(vm, len >= 4 && std::strcmp(path + len - 4, ".gif") == 0);
  be_return(vm);
}

const bnfuncinfo kHarnessLibrary[] = {
    {"beat", harnessBeat},      {"press", harnessPress},     {"back_hold", harnessBackHold},
    {"reset", harnessReset},    {"save", harnessSave},       {"log", harnessLog},
    {"wrote", harnessWrote},    {"buzzer_sounds", harnessBuzzerSounds},
    {"capture_start", harnessCaptureStart}, {"capture_finish", harnessCaptureFinish},
    {"capture_stem", harnessCaptureStem},   {"is_gif", harnessIsGif},
    {nullptr, nullptr},
};

}  // namespace

void gardener::installHarnessModule(bvm* vm, HarnessSession& session) {
  gSession = &session;
  gardener::berry::installModule(vm, "harness", kHarnessLibrary);
}
