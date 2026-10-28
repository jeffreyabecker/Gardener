// The machine-alone `device` domain: the firmware's own fields, the board facts it is handed, the
// level set it writes through the logger, and the capability byte the composition derives.

#include <doctest.h>

#include <Arduino.h>

#include <string>

#include "TextStream.h"
#include "fakes/Parts.h"
#include "firmwareversion.h"
#include "machine/CommandQueue.h"
#include "machine/Machine.h"
#include "serial/BoardFacts.h"
#include "serial/Domains.h"
#include "serial/SerialLink.h"

namespace {

using gardener::BoardFacts;
using gardener::CommandQueue;
using gardener::Machine;
using gardener::MachineCommand;
using gardener::test::RecordingLogger;
using gardener::SerialLink;
using gardener::host::TextStream;
using gardener::test::FakePump;
using gardener::test::FakeSensor;
using gardener::test::FakeValve;

// The board's facts, as the sketch's own pair answer them.
const char* chipModel() { return "ESP32-S3"; }
uint32_t heapFacts() { return 123456; }

class OpenQueue : public CommandQueue {
 public:
  bool post(const MachineCommand&) override { return true; }
  bool take(MachineCommand&) override { return false; }
};

// The domain over a machine whose valve has the polarity the case cares about.
struct Bench {
  explicit Bench(bool energiseWhenOpen) : valve(energiseWhenOpen, nullptr), machine(logger, &pump,
                                                                                 &sensor, &valve),
                                          link(io, nullptr, logger, queue) {
    machine.boot();
    const BoardFacts board{&chipModel, &heapFacts};
    link.registerDomain(gardener::createMachineDeviceDomain(machine, link.outbox(), board));
  }

  void send(const char* text) {
    io.clearPrinted();
    io.type(text);
    link.poll(0);
  }

  std::string reply() const { return io.printed(); }

  FakePump pump;
  FakeSensor sensor;
  FakeValve valve;
  RecordingLogger logger;
  OpenQueue queue;
  Machine machine;
  TextStream io;
  SerialLink link;
};

std::string machineFields(unsigned int caps) {
  std::string expected = "ok proto=1 fw=";
  expected += gardener::FirmwareVersion;
  expected += " chip=ESP32-S3 heap=123456 log=IWE caps=";
  expected += std::to_string(caps);
  expected += "\n";
  return expected;
}

}  // namespace

TEST_CASE("device get answers the firmware, the board and the level set") {
  Bench bench(false);
  bench.send("device get\n");
  CHECK_EQ(bench.reply(), machineFields(4));
}

TEST_CASE("a fail-safe valve puts FailSafeValve in the capability byte") {
  Bench bench(false);
  bench.send("device get\n");
  CHECK(bench.reply().find("caps=4") != std::string::npos);
}

TEST_CASE("a valve that needs power to hold does not claim the fail-safe bit") {
  Bench bench(true);
  bench.send("device get\n");
  CHECK_EQ(bench.reply(), machineFields(0));
}

TEST_CASE("device set log writes the levels and answers them canonically") {
  Bench bench(false);
  bench.send("device set log we\n");
  CHECK_EQ(bench.reply(), std::string("ok log=WE\n"));
  CHECK_EQ(std::string(bench.logger.levels().c_str()), std::string("WE"));
}

TEST_CASE("an unknown level is refused and leaves the levels alone") {
  Bench bench(false);
  bench.send("device set log x\n");
  CHECK_EQ(bench.reply(), std::string("err bad_argument x\n"));
  CHECK_EQ(std::string(bench.logger.levels().c_str()), std::string("IWE"));
}

TEST_CASE("a second token refuses before anything applies") {
  Bench bench(false);
  bench.send("device set log we extra\n");
  CHECK_EQ(bench.reply(), std::string("err bad_argument extra\n"));
  CHECK_EQ(std::string(bench.logger.levels().c_str()), std::string("IWE"));
}

TEST_CASE("the set grammar is a space, not an equals sign") {
  Bench bench(false);
  bench.send("device set log=we\n");
  CHECK_EQ(bench.reply(), std::string("err bad_argument\n"));
}

TEST_CASE("an unknown field is refused by name") {
  Bench bench(false);
  bench.send("device set volume 3\n");
  CHECK_EQ(bench.reply(), std::string("err bad_argument volume\n"));
}
