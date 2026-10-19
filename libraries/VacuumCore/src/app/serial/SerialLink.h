#pragma once

#include <Arduino.h>

#include "app/serial/Domain.h"

namespace gardener {

class AppContext;

// The serial interface to the controller. It reads the first token of each command, finds that
// domain and hands it the command stream; everything after the domain token belongs to the domain.
// Replies are the domains' business: this class owns only the link's own errors and diagnostics.
class SerialLink {
 public:
  // True while a host has the port open. Null means the stream cannot tell (a plain UART), in which
  // case a lost link is simply never reported.
  using PresenceFn = bool (*)();

  SerialLink(AppContext* context, Stream& stream, PresenceFn present, bool& logEnabled);

  void begin();
  void poll(uint32_t nowMs);

  bool controllerAttached() const;
  // One shot: the controller went away. App decides what that means (stop and vent); the interface
  // only reports it.
  bool consumeLinkLost();

 private:
  void drainCommands();
  bool dispatchOne();
  void diag(const char* prefix, const char* text);
  void writeLine(const char* prefix, const char* text);
  // The dispatcher's own errors: an unknown domain, or a header it could not accept.
  void linkError(const char* code, const String& items);

  Stream& stream_;
  PresenceFn present_;
  bool& logEnabled_;
  DomainContext domainContext_;
  DomainList domains_;
  // Everything the host has sent that has not been dispatched yet. Bytes go in, commands come out.
  String buffer_;
  // An overlong header was answered already and the rest of it is being thrown away.
  bool swallowing_ = false;
  bool attached_ = false;
  bool linkLost_ = false;
};

}  // namespace gardener
