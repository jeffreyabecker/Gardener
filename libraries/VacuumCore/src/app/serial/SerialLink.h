#pragma once

#include <Arduino.h>

#include "app/serial/Domain.h"

namespace gardener {

class AppContext;

// The serial interface to the controller. It reads the first token of each command, finds that
// domain and hands it the stream at that point; everything after the domain token, including any
// body, belongs to the domain. Replies are the domains' business: this class owns only the link's
// own errors and diagnostics.
class SerialLink {
 public:
  // True while a host has the port open. Null means the stream cannot tell (a plain UART), in which
  // case a lost link is simply never reported.
  using PresenceFn = bool (*)();

  SerialLink(AppContext* context, Stream& stream, PresenceFn present);

  void begin();
  void poll(uint32_t nowMs);

  bool controllerAttached() const;
  // One shot: the controller went away. App decides what that means (stop and vent); the interface
  // only reports it.
  bool consumeLinkLost();

 private:
  // Hands the stream to the domain named by `token`. False when there is no such domain, in which
  // case the caller discards the rest of the line.
  bool dispatchDomain(const String& token);
  void diag(const char* prefix, const char* text);
  // The dispatcher's own errors: an unknown domain, or a header it could not accept.
  void linkError(const char* code, const String& detail);

  Stream& stream_;
  PresenceFn present_;
  AppContext& context_;
  DomainList domains_;
  // The domain token being assembled. It never holds a command or a body.
  String buffer_;
  // The rest of a line that was already answered, or a comment, is being thrown away.
  bool swallowing_ = false;
  bool attached_ = false;
  bool linkLost_ = false;
};

}  // namespace gardener
