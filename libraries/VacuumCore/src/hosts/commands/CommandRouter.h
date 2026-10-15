#pragma once

#include <Arduino.h>

#include "core/ReplyQueue.h"
#include "core/Snapshot.h"

namespace eggplan {

class PressureSensor;
class Pump;
class VacuumControl;

// The line grammar. It returns lines and encodes nothing: the wire format lives in
// services/SnapshotText. It reads the machine through the snapshot rather than reaching
// into a module for state, and it is the only place that turns text into action.
class CommandRouter {
 public:
  CommandRouter(VacuumControl& control, Pump& pump, PressureSensor& sensor,
                TelemetrySettings& telemetry);

  void handleLine(const String& line, ReplyQueue& out, const SystemSnapshot& snapshot);

 private:
  void emitHelp(ReplyQueue& out);
  static void reply(ReplyQueue& out, const String& text);
  static void fail(ReplyQueue& out, const String& text);

  VacuumControl& control_;
  Pump& pump_;
  PressureSensor& sensor_;
  TelemetrySettings& telemetry_;
};

}  // namespace eggplan
