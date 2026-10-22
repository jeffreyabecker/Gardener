#pragma once

#include <Arduino.h>

#include "Ownership.h"
#include "serial/Domain.h"

namespace gardener {

class Logger;
class Machine;

// The serial interface to the controller. It reads the first token of each command, finds that
// domain and hands it the stream at that point; everything after the domain token, including any
// body, belongs to the domain. Replies are the domains' business: this class owns only the link's
// own errors and diagnostics.
class SerialLink : public Link {
 public:
  static constexpr uint8_t Code = 1;

  // True while a host has the port open. Null means the stream cannot tell (a plain UART), in which
  // case a lost link is simply never reported.
  using PresenceFn = bool (*)();

  SerialLink(Machine& machine, Stream& stream, PresenceFn present, Logger& logger);

  const char* name() const override { return "serial"; }
  uint8_t code() const override { return Code; }

  // A claim cancels a departure the stack has not applied yet: the port is gone, then it is not,
  // and the host has taken the machine again.
  void claimed() override { linkLost_ = false; }

  // The surface is whatever the build registers: the machine adds its domains, and a panel build
  // adds its own, so the link itself names none of them.
  void registerDomain(Domain* domain) { domains_.add(domain); }

  void begin();
  void poll(uint32_t nowMs);

  bool controllerAttached() const;

 private:
  // Hands the stream to the domain named by `token`. False when there is no such domain, in which
  // case the caller discards the rest of the line.
  bool dispatchDomain(const String& token);
  // The dispatcher's own errors: an unknown domain, or a header it could not accept.
  void linkError(const char* code, const String& detail);

  Machine& machine_;
  Stream& stream_;
  PresenceFn present_;
  Logger& logger_;
  DomainList domains_;
  // The domain token being assembled. It never holds a command or a body.
  String buffer_;
  // The rest of a line that was already answered, or a comment, is being thrown away.
  bool swallowing_ = false;
  bool attached_ = false;
  // The port went away while the link was not on top, so it could not leave the stack yet.
  bool linkLost_ = false;
};

}  // namespace gardener
