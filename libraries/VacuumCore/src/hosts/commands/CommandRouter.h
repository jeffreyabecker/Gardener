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
//
// The line is tokenised in place, so handleLine takes a mutable buffer and the caller
// owns its lifetime.
class CommandRouter {
 public:
  CommandRouter(VacuumControl& control, Pump& pump, PressureSensor& sensor,
                LinkSettings& settings);

  void handleLine(char* line, Print& out, const SystemSnapshot& snapshot);

 private:
  static void emitHelp(Print& out);
  static void reply(Print& out, const char* payload);
  static void fail(Print& out, const char* code);

  VacuumControl& control_;
  Pump& pump_;
  PressureSensor& sensor_;
  LinkSettings& settings_;
  char body_[224];  // one payload buffer, reused: the reply is written before returning
};

}  // namespace eggplan
