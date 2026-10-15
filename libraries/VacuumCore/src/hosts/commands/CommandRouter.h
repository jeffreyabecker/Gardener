#pragma once

#include <Arduino.h>

#include "core/Snapshot.h"

namespace eggplan {

class PressureSensor;
class Pump;
class VacuumControl;

// The line grammar. It parses text into action and writes one reply line per command to
// the stream it is handed, and it encodes nothing else: the payload format lives in
// services/SnapshotText. It reads the machine through the snapshot rather than reaching
// into a module for state, and it is the only place that turns text into action.
class CommandRouter {
 public:
  CommandRouter(VacuumControl& control, Pump& pump, PressureSensor& sensor,
                LinkSettings& settings);

  void handleLine(const String& line, Print& out, const SystemSnapshot& snapshot);

 private:
  void emitHelp(Print& out);
  static void reply(Print& out, const String& text);
  static void fail(Print& out, const String& text);

  VacuumControl& control_;
  Pump& pump_;
  PressureSensor& sensor_;
  LinkSettings& settings_;
};

}  // namespace eggplan
