#pragma once

#include <Arduino.h>

#include "core/ReplyQueue.h"
#include "core/Snapshot.h"

namespace eggplan {

class PressureSensor;
class ProgramStore;
class Pump;
class ScriptApi;
class ScriptRuntime;
class VacuumControl;

// The line grammar, shared by the BLE command characteristic and a future serial
// console.
//
// It returns lines and encodes nothing: the wire format lives in
// services/SnapshotText and the program frames in services/ProgramUploader. It reads
// the machine through the snapshot and asks the UI for things through UiSettings, so
// it never reaches into either of them.
class CommandRouter {
 public:
  CommandRouter(ProgramStore& store, ScriptApi& api, ScriptRuntime& runtime,
                VacuumControl& control, Pump& pump, PressureSensor& sensor,
                TelemetrySettings& telemetry, UiSettings& ui);

  void handleLine(const String& line, ReplyQueue& out, const SystemSnapshot& snapshot);

 private:
  void emitHelp(ReplyQueue& out);
  void emitList(ReplyQueue& out, const SystemSnapshot& snapshot);
  void emitCat(const String& name, uint32_t offset, uint32_t length, ReplyQueue& out);
  void emitLog(int lines, ReplyQueue& out);
  void cmdRun(const String& name, const String& args, ReplyQueue& out);

  static void reply(ReplyQueue& out, const String& text);
  static void fail(ReplyQueue& out, const String& text);

  ProgramStore& store_;
  ScriptApi& api_;
  ScriptRuntime& runtime_;
  VacuumControl& control_;
  Pump& pump_;
  PressureSensor& sensor_;
  TelemetrySettings& telemetry_;
  UiSettings& ui_;
};

}  // namespace eggplan
