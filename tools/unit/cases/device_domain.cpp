// The `device` domain: the firmware's own fields, the board facts it is handed, the gauge and the
// document handed in beside them, the level set it writes through the logger, and the capability
// byte the composition derives.

#include <doctest.h>

#include <Arduino.h>

#include <string>

#include "TextStream.h"
#include "core/SystemState.h"
#include "fakes/MemoryLibrary.h"
#include "fakes/Parts.h"
#include "firmwareversion.h"
#include "machine/CommandQueue.h"
#include "machine/Machine.h"
#include "serial/BoardFacts.h"
#include "serial/Domains.h"
#include "serial/SerialLink.h"
#include "storage/DeviceSettings.h"

namespace {

using gardener::BoardFacts;
using gardener::CommandQueue;
using gardener::DeviceSettings;
using gardener::Machine;
using gardener::MachineCommand;
using gardener::SystemState;
using gardener::test::RecordingLogger;
using gardener::SerialLink;
using gardener::host::TextStream;
using gardener::test::FakeBattery;
using gardener::test::FakePump;
using gardener::test::FakeSensor;
using gardener::test::FakeValve;
using gardener::test::MemoryLibrary;

// The board's facts, as the sketch's own pair answer them.
const char* chipModel() { return "ESP32-S3"; }
uint32_t heapFacts() { return 123456; }

class OpenQueue : public CommandQueue {
 public:
  bool post(const MachineCommand&) override { return true; }
  bool take(MachineCommand&) override { return false; }
};

// The domain over a machine, a gauge and a document the case owns.
struct Bench {
  explicit Bench(bool energiseWhenOpen) : valve(energiseWhenOpen, nullptr), machine(logger, &pump,
                                                                                 &sensor, &valve),
                                          link(io, nullptr, logger, queue) {
    machine.boot();
    settings.begin();
    const BoardFacts board{&chipModel, &heapFacts};
    link.registerDomain(
        gardener::createDeviceDomain(machine, link.outbox(), board, battery, settings));
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
  FakeBattery battery;
  SystemState state;
  MemoryLibrary library;
  DeviceSettings settings{&library, state};
  SerialLink link;
};

std::string deviceFields(unsigned int caps, const std::string& gauge = "batt_v=3.70 batt_pct=50",
                         const char* unit = "kpa") {
  std::string expected = "ok proto=1 fw=";
  expected += gardener::FirmwareVersion;
  expected += " chip=ESP32-S3 heap=123456 ";
  expected += gauge;
  expected += " log=IWE unit=";
  expected += unit;
  expected += " caps=";
  expected += std::to_string(caps);
  expected += "\n";
  return expected;
}

}  // namespace

TEST_CASE("device get answers the firmware, the board and the level set") {
  Bench bench(false);
  bench.send("device get\n");
  CHECK_EQ(bench.reply(), deviceFields(4));
}

TEST_CASE("a fail-safe valve puts FailSafeValve in the capability byte") {
  Bench bench(false);
  bench.send("device get\n");
  CHECK(bench.reply().find("caps=4") != std::string::npos);
}

TEST_CASE("a valve that needs power to hold does not claim the fail-safe bit") {
  Bench bench(true);
  bench.send("device get\n");
  CHECK_EQ(bench.reply(), deviceFields(0));
}

TEST_CASE("a device with no gauge reports both battery fields as absent") {
  Bench bench(false);
  bench.battery.setPresent(false);
  bench.send("device get\n");
  CHECK_EQ(bench.reply(), deviceFields(4, "batt_v=- batt_pct=-"));
}

TEST_CASE("device set unit writes the document and answers the token") {
  Bench bench(false);
  bench.send("device set unit psi\n");
  CHECK_EQ(bench.reply(), std::string("ok unit=psi\n"));
  CHECK_EQ(bench.library.writes(), 1);
  bench.send("device get\n");
  CHECK_EQ(bench.reply(), deviceFields(4, "batt_v=3.70 batt_pct=50", "psi"));
}

TEST_CASE("a unit this firmware does not hold is refused and changes nothing") {
  Bench bench(false);
  bench.send("device set unit furlongs\n");
  CHECK_EQ(bench.reply(), std::string("err bad_argument furlongs\n"));
  CHECK_EQ(bench.library.writes(), 0);
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
