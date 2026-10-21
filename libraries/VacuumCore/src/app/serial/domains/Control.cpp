#include "app/serial/Domains.h"

#include <Arduino.h>

#include "app/MachineContext.h"
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
  explicit ControlDomain(MachineContext& app) : app_(app) {}

  const char* name() const override { return "control"; }
  void execute(Stream& io) override;

 private:
  void ownedReply(Stream& io) const;
  void acquire(Stream& io, const String& args);
  void release(Stream& io, const String& args);

  MachineContext& app_;
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

// The caller's view of the stack: who is on top, named rather than "owned by me or not".
void ControlDomain::ownedReply(Stream& io) const {
  io.print("ok control=");
  io.print(linkName(app_.ownership().owner()));
  io.print('\n');
}

void ControlDomain::acquire(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    io.print("err bad_argument ");
    io.print(firstToken(args));
    io.print('\n');
    return;
  }

  // A claim always succeeds and takes the top; the control beat vents the hand-over.
  app_.ownership().acquire(Link::Serial);
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
  // `vacuum stop`, which the control beat applies when the top changes. Releasing without owning
  // is a no-op.
  app_.ownership().release(Link::Serial);
  ownedReply(io);
}

}  // namespace

Domain* createControlDomain(MachineContext& app) { return new ControlDomain(app); }

}  // namespace gardener
