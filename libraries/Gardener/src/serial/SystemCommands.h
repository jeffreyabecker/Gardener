#pragma once

#include <Arduino.h>

#include "machine/SystemControl.h"
#include "serial/CommandPlugin.h"

namespace gardener {

// The machine's lines on the serial wire, under the sys prefix: the status, the target and the run
// it is granted, the release and the latch's reset, and the status the link repeats as a heartbeat.
// Every decision behind a reply — the bounds, the ambient target, the refusal — is the control's;
// this plugin parses the argument and formats the answer.
class SystemCommands : public CommandPlugin {
 public:
  explicit SystemCommands(SystemControl& control) : control_(control) {}

  const char* prefix() const override { return "sys"; }
  void serve(Print& io, const String& args) override;
  void heartbeat(Print& io) override;

 private:
  // The status line both `get` and the heartbeat write, so the two cannot drift apart.
  void writeStatus(Print& io, const char* prefix) const;

  SystemControl& control_;
};

}  // namespace gardener
