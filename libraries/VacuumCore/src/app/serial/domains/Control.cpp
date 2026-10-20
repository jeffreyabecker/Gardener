#include "app/serial/Domains.h"

#include <Arduino.h>

#include "app/AppContext.h"
#include "app/Ownership.h"
#include "app/Tasks.h"
#include "app/VacuumSystem.h"
#include "app/serial/Domain.h"
#include "core/Config.h"

namespace gardener {
namespace {

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

class ControlDomain : public Domain {
 public:
  explicit ControlDomain(AppContext& app) : app_(app) {}

  const char* name() const override { return "control"; }
  void execute(Stream& io) override;

 private:
  void ownedReply(Stream& io) const;
  void acquire(Stream& io, const String& args);
  void release(Stream& io, const String& args);

  AppContext& app_;
};

void ControlDomain::execute(Stream& io) {
  String line = io.readStringUntil('\n');
  line.trim();
  if (line.length() > cfg::COMMAND_LINE_MAX) {
    io.print("err bad_argument\n");
    return;
  }

  const int split = line.indexOf(' ');
  String verb = split < 0 ? line : line.substring(0, split);
  String args = split < 0 ? String() : line.substring(split + 1);
  args.trim();
  verb.toLowerCase();

  if (verb.isEmpty()) {
    io.print("err bad_argument\n");
  } else if (verb == "get") {
    if (!args.isEmpty()) {
      io.print("err bad_argument ");
      io.print(firstToken(args));
      io.print('\n');
      return;
    }
    ownedReply(io);
  } else if (verb == "acquire") {
    acquire(io, args);
  } else if (verb == "release") {
    release(io, args);
  } else {
    io.print("err unknown_verb ");
    io.print(verb);
    io.print('\n');
  }
}

// The caller's view: a stale record reads as none even though the machine is still held for the link
// that left it.
void ControlDomain::ownedReply(Stream& io) const {
  io.print(app_.ownership().ownedBy(Link::Serial) ? "ok control=owned\n" : "ok control=none\n");
}

void ControlDomain::acquire(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    io.print("err bad_argument ");
    io.print(firstToken(args));
    io.print('\n');
    return;
  }

  if (!app_.ownership().acquire(Link::Serial)) {
    io.print("err busy\n");
    return;
  }
  ownedReply(io);
}

void ControlDomain::release(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    io.print("err bad_argument ");
    io.print(firstToken(args));
    io.print('\n');
    return;
  }

  // Releasing control is the safe exit: the chamber goes onto the same release path as
  // `vacuum stop`, which is the control task's to apply. A link that does not own the device has
  // nothing to release, which is why this is idempotent.
  if (app_.ownership().release(Link::Serial)) {
    const MachineCommand stop{MachineCommand::Op::Stop};
    xQueueSend(machineCommandQueue, &stop, 0);
  }
  io.print("ok control=none\n");
}

}  // namespace

Domain* createControlDomain(AppContext& app) { return new ControlDomain(app); }

}  // namespace gardener
