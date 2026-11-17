// The serial link's framing: which bytes end a command line, what a line the link cannot serve
// answers, what happens to a half-read line when the controller goes away, and the heartbeat it
// writes on its own clock. The bench is the real surface over fakes — a short reply is enough to see
// that the line reached the grammar; the machine's own commands are in vacuum_commands.cpp.

#include <doctest.h>

#include <Arduino.h>

#include <string>
#include <vector>

#include "Clock.h"
#include "TextStream.h"
#include "fakes/Parts.h"
#include "machine/CommandQueue.h"
#include "machine/Machine.h"
#include "machine/SystemControl.h"
#include "serial/SerialLink.h"

namespace {

using gardener::CommandQueue;
using gardener::Machine;
using gardener::MachineCommand;
using gardener::SerialLink;
using gardener::SystemControl;
using gardener::host::TextStream;
using gardener::test::FakePump;
using gardener::test::FakeSensor;
using gardener::test::FakeValve;
using gardener::test::RecordingLogger;

bool controllerPresent = true;

bool presentFn() { return controllerPresent; }

// The queue the link posts into: everything fits, and a case can see what was posted.
class OpenQueue : public CommandQueue {
 public:
  bool post(const MachineCommand& command) override {
    commands_.push_back(command);
    return true;
  }

  bool take(MachineCommand& command) override {
    if (commands_.empty()) return false;
    command = commands_.front();
    commands_.erase(commands_.begin());
    return true;
  }

 private:
  std::vector<MachineCommand> commands_;
};

// One link over the fakes, with the machine's own grammar as the reply a framing case reads.
struct Bench {
  Bench()
      : machine(logger, &pump, &sensor, &valve), control(machine.vacuum(), queue),
        link(io, &presentFn, logger, control) {
    gardener::host::reset();
    machine.boot();
    link.begin();
  }

  // Types a line and runs one poll, which is what a link pass with a whole command in the buffer
  // does.
  void send(const char* text) {
    io.type(text);
    link.poll(gardener::host::nowMs());
  }

  FakePump pump;
  FakeSensor sensor;
  FakeValve valve;
  RecordingLogger logger;
  OpenQueue queue;
  Machine machine;
  SystemControl control;
  TextStream io;
  SerialLink link;
};

}  // namespace

TEST_CASE("a lowercase verb dispatches") {
  Bench bench;
  bench.send("vent\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("an uppercase verb dispatches") {
  Bench bench;
  bench.send("VENT\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("a mixed case verb dispatches") {
  Bench bench;
  bench.send("VeNt\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("the old domain prefix is just an unknown verb") {
  Bench bench;
  bench.send("vacuum vent\n");
  CHECK_EQ(bench.io.printed(), std::string("err unknown_verb vacuum\n"));
}

TEST_CASE("a comment answers nothing and the next line still dispatches") {
  Bench bench;
  bench.send("# a note about the line below\nvent\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("leading and trailing whitespace is trimmed") {
  Bench bench;
  bench.send("  vent  \n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("a CRLF line leaves no carriage return in the body") {
  Bench bench;
  bench.send("vent\r\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("a token longer than the line maximum is refused") {
  Bench bench;
  std::string line(129, 'v');
  line += "\n";
  bench.send(line.c_str());
  CHECK_EQ(bench.io.printed(), std::string("err bad_argument\n"));
}

TEST_CASE("an unknown verb names the token it could not serve") {
  Bench bench;
  bench.send("Bogus get\n");
  CHECK_EQ(bench.io.printed(), std::string("err unknown_verb bogus\n"));
}

TEST_CASE("the verb's argument reaches the grammar intact") {
  Bench bench;
  bench.sensor.read(12.0f);
  bench.send("set 40\n");
  CHECK_EQ(bench.io.printed(), std::string("ok target=40.0 state=PULL\n"));
}

TEST_CASE("a heartbeat goes out once five seconds have passed") {
  Bench bench;
  bench.link.poll(gardener::host::nowMs());
  CHECK_EQ(bench.io.printed(), std::string(""));

  gardener::host::advance(4999);
  bench.link.poll(gardener::host::nowMs());
  CHECK_EQ(bench.io.printed(), std::string(""));

  gardener::host::advance(1);
  bench.link.poll(gardener::host::nowMs());
  CHECK_EQ(bench.io.printed(),
           std::string("hb pressure_kpa=0.00 target=0.0 state=IDLE duty=0.0 fault=none\n"));
}

TEST_CASE("the heartbeat repeats on its own period and not on every poll") {
  Bench bench;
  gardener::host::advance(5000);
  bench.link.poll(gardener::host::nowMs());
  bench.io.clearPrinted();

  gardener::host::advance(4999);
  bench.link.poll(gardener::host::nowMs());
  CHECK_EQ(bench.io.printed(), std::string(""));

  gardener::host::advance(1);
  bench.link.poll(gardener::host::nowMs());
  CHECK_EQ(bench.io.printed(),
           std::string("hb pressure_kpa=0.00 target=0.0 state=IDLE duty=0.0 fault=none\n"));
}

TEST_CASE("a detached controller gets no heartbeat") {
  Bench bench;
  controllerPresent = false;
  gardener::host::advance(5000);
  bench.link.poll(gardener::host::nowMs());
  CHECK_EQ(bench.io.printed(), std::string(""));

  controllerPresent = true;
  gardener::host::advance(5000);
  bench.link.poll(gardener::host::nowMs());
  CHECK_EQ(bench.io.printed(),
           std::string("hb pressure_kpa=0.00 target=0.0 state=IDLE duty=0.0 fault=none\n"));
}

TEST_CASE("a lost controller is reported and a half-read line is dropped") {
  Bench bench;
  controllerPresent = true;
  bench.io.type("ven");
  bench.link.poll(gardener::host::nowMs());

  controllerPresent = false;
  bench.link.poll(gardener::host::nowMs());
  CHECK(bench.logger.saw("serial: controller detached"));

  controllerPresent = true;
  bench.send("t\n");
  CHECK_EQ(bench.io.printed(), std::string("err unknown_verb t\n"));
}
