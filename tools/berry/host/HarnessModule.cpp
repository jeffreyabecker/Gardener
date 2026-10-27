#include "HarnessSession.h"

#include <berry.h>

#include <string>

#include "routines/bindings/Module.h"

namespace {

// One VM and one app, like the firmware's bindings: the native callbacks are plain functions with
// no user data, so they reach the session through this.
using gardener::HarnessSession;

HarnessSession* session = nullptr;

// --- the drive -------------------------------------------------------------

int harnessBeat(bvm* vm) {
  session->beat((int)be_toint(vm, 1));
  be_pushbool(vm, session->active());
  be_return(vm);
}

int harnessPress(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "harness.press(gesture)");
  }
  const char* gesture = be_tostring(vm, 1);
  session->pressGesture(gesture);
  session->routines().pushPress(gesture);
  be_return_nil(vm);
}

// The device's Back hold: the stop command panelTask fires.
int harnessBackHold(bvm* vm) {
  session->backHold();
  be_return_nil(vm);
}

int harnessReset(bvm* vm) {
  session->reset();
  be_return_nil(vm);
}

// --- what the driver asks about -------------------------------------------

int harnessSave(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "harness.save(path)");
  }
  be_pushbool(vm, session->saveStill(be_tostring(vm, 1)));
  be_return(vm);
}

int harnessLog(bvm* vm) {
  be_pushstring(vm, session->logger().text().c_str());
  be_return(vm);
}

int harnessWrote(bvm* vm) {
  if (be_top(vm) < 2 || !be_isstring(vm, 1) || !be_isstring(vm, 2)) {
    be_raise(vm, "type_error", "harness.wrote(routine, key)");
  }
  const std::string path = std::string("/cfg/routines/") + be_tostring(vm, 1) + ".ini";
  be_pushbool(vm, session->files().wrote(path, be_tostring(vm, 2)));
  be_return(vm);
}

int harnessWrites(bvm* vm) {
  be_pushint(vm, session->files().writes());
  be_return(vm);
}

int harnessBuzzerSounds(bvm* vm) {
  be_pushint(vm, session->buzzerPart().sounds());
  be_return(vm);
}

// The bytes the interpreter holds, read after a collection; and the largest figure this run has
// sampled at a beat, which is the high-water mark the run reports.
int harnessMem(bvm* vm) {
  be_pushint(vm, (bint)session->heapBytes());
  be_return(vm);
}

int harnessMemPeak(bvm* vm) {
  be_pushint(vm, (bint)session->heapPeak());
  be_return(vm);
}

// --- the capture -----------------------------------------------------------

int harnessCaptureStart(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "harness.capture_start(path)");
  }
  session->beginCapture(be_tostring(vm, 1));
  be_return_nil(vm);
}

int harnessCaptureFinish(bvm* vm) {
  be_pushbool(vm, session->endCapture());
  be_return(vm);
}

int harnessCaptureStem(bvm* vm) {
  be_pushstring(vm, session->captureStem().c_str());
  be_return(vm);
}

// The suite framework's marker: the rows the ux band occupies in a captured frame.
int harnessMarkOverlay(bvm* vm) {
  if (be_top(vm) < 2 || !be_isint(vm, 1) || !be_isint(vm, 2)) {
    be_raise(vm, "type_error", "harness.mark_overlay(top, rows)");
  }
  session->markOverlay((int16_t)be_toint(vm, 1), (int16_t)be_toint(vm, 2));
  be_return_nil(vm);
}

const bnfuncinfo HarnessLibrary[] = {
    {"beat", harnessBeat},      {"press", harnessPress},     {"back_hold", harnessBackHold},
    {"reset", harnessReset},    {"save", harnessSave},       {"log", harnessLog},
    {"wrote", harnessWrote},    {"writes", harnessWrites},   {"buzzer_sounds", harnessBuzzerSounds},
    {"mem", harnessMem},        {"mem_peak", harnessMemPeak},
    {"capture_start", harnessCaptureStart}, {"capture_finish", harnessCaptureFinish},
    {"capture_stem", harnessCaptureStem}, {"mark_overlay", harnessMarkOverlay},
    {nullptr, nullptr},
};

}  // namespace

void gardener::installHarnessModule(bvm* vm, HarnessSession& appSession) {
  session = &appSession;
  gardener::berry::installModule(vm, "harness", HarnessLibrary);
}
