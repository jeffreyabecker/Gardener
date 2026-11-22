// The serial link's framing and its table of plugins: which bytes end a command line, which word
// picks a plugin, what a line the link cannot serve answers, what happens to a half-read line when
// the controller goes away, and the heartbeat it writes on its own clock. The bench is the real
// surface over fakes — a short reply is enough to see that the line reached its plugin; the
// machine's own commands are in vacuum_commands.cpp, the network's in wifi_commands.cpp.

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
#include "serial/CommandPlugin.h"
#include "serial/SerialLink.h"
#include "serial/SystemCommands.h"

namespace {

using gardener::CommandPlugin;
using gardener::CommandQueue;
using gardener::Machine;
using gardener::MachineCommand;
using gardener::SerialLink;
using gardener::SystemCommands;
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

// A part with one line and nothing to say at the beat, which is what a second plugin needs to be to
// see the link serve it, refuse it, or stay quiet on its behalf.
class Quiet : public CommandPlugin {
 public:
  explicit Quiet(const char* prefix) : prefix_(prefix) {}

  const char* prefix() const override { return prefix_; }
  void serve(Print& io, const String&) override {
    served_++;
    io.print("ok quiet\n");
  }
  int served() const { return served_; }

 private:
  const char* prefix_;
  int served_ = 0;
};

// One link over the fakes, with the machine's plugin registered as the build registers it.
struct Bench {
  Bench()
      : machine(logger, &pump, &sensor, &valve), control(machine.vacuum(), queue), system(control),
        link(io, &presentFn, logger) {
    gardener::host::reset();
    machine.boot();
    link.add(system);
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
  SystemCommands system;
  TextStream io;
  SerialLink link;
};

}  // namespace

TEST_CASE("a registered prefix reaches its plugin") {
  Bench bench;
  bench.send("sys vent\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("a prefix is matched without regard to case") {
  Bench bench;
  bench.send("SYS VENT\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("a prefix and its word in mixed case are the same command") {
  Bench bench;
  bench.send("SyS VeNt\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("a prefix nothing registered is refused by name") {
  Bench bench;
  bench.send("vacuum vent\n");
  CHECK_EQ(bench.io.printed(), std::string("err unknown_prefix vacuum\n"));
}

TEST_CASE("a bare word is a prefix nothing registered") {
  Bench bench;
  bench.send("vent\n");
  CHECK_EQ(bench.io.printed(), std::string("err unknown_prefix vent\n"));
}

TEST_CASE("a word under a registered prefix is the plugin's own refusal") {
  Bench bench;
  bench.send("sys bogus\n");
  CHECK_EQ(bench.io.printed(), std::string("err bad_argument bogus\n"));
}

TEST_CASE("a comment answers nothing and the next line still dispatches") {
  Bench bench;
  bench.send("# a note about the line below\nsys vent\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("leading and trailing whitespace is trimmed") {
  Bench bench;
  bench.send("  sys vent  \n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("a CRLF line leaves no carriage return in the body") {
  Bench bench;
  bench.send("sys vent\r\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
}

TEST_CASE("a token longer than the line maximum is refused") {
  Bench bench;
  std::string line(129, 'v');
  line += "\n";
  bench.send(line.c_str());
  CHECK_EQ(bench.io.printed(), std::string("err bad_argument\n"));
}

TEST_CASE("an unserved prefix names the word it could not serve, lowercased") {
  Bench bench;
  bench.send("Bogus get\n");
  CHECK_EQ(bench.io.printed(), std::string("err unknown_prefix bogus\n"));
}

TEST_CASE("the word's argument reaches the plugin intact") {
  Bench bench;
  bench.sensor.read(12.0f);
  bench.send("sys set 40\n");
  CHECK_EQ(bench.io.printed(), std::string("ok target=40.00 state=PULL\n"));
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
  CHECK_EQ(bench.io.printed(), std::string("hb uptime_ms=5000 pressure_kpa=0.00 target=0.0 "
                                           "state=IDLE duty=0.0 fault=none\n"));
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
  CHECK_EQ(bench.io.printed(), std::string("hb uptime_ms=10000 pressure_kpa=0.00 target=0.0 "
                                           "state=IDLE duty=0.0 fault=none\n"));
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
  CHECK_EQ(bench.io.printed(), std::string("hb uptime_ms=10000 pressure_kpa=0.00 target=0.0 "
                                           "state=IDLE duty=0.0 fault=none\n"));
}

TEST_CASE("a plugin with no heartbeat writes nothing at the beat") {
  TextStream io;
  RecordingLogger logger;
  SerialLink link(io, nullptr, logger);
  Quiet quiet("quiet");
  link.add(quiet);
  link.begin();

  gardener::host::reset();
  gardener::host::advance(5000);
  link.poll(gardener::host::nowMs());
  CHECK_EQ(io.printed(), std::string(""));
}

TEST_CASE("a prefix registered twice is refused and the first keeps serving") {
  Bench bench;
  Quiet impostor("sys");
  bench.link.add(impostor);
  CHECK(bench.logger.saw("sys is already served"));

  bench.send("sys vent\n");
  CHECK_EQ(bench.io.printed(), std::string("ok state=VENT\n"));
  CHECK_EQ(impostor.served(), 0);
}

TEST_CASE("the ready line names the prefixes the build registered") {
  Bench bench;
  CHECK(bench.logger.saw("serial: interface ready sys"));
}

TEST_CASE("a lost controller is reported and a half-read line is dropped") {
  Bench bench;
  controllerPresent = true;
  bench.io.type("sys ven");
  bench.link.poll(gardener::host::nowMs());

  controllerPresent = false;
  bench.link.poll(gardener::host::nowMs());
  CHECK(bench.logger.saw("serial: controller detached"));

  controllerPresent = true;
  bench.send("t\n");
  CHECK_EQ(bench.io.printed(), std::string("err unknown_prefix t\n"));
}
