// The `fs` commands over the store the link is handed: one path per command, a folder listing, a
// declared body for `write`, and the refusals a reserved, missing or non-directory path earns. The
// store is the suite's in-memory Library, which is the surface the grammar speaks to.

#include <doctest.h>

#include <Arduino.h>

#include <string>

#include "Clock.h"
#include "TextStream.h"
#include "core/SystemState.h"
#include "fakes/MemoryLibrary.h"
#include "fakes/Parts.h"
#include "machine/CommandQueue.h"
#include "machine/Machine.h"
#include "panel/parts/BuzzerPlayer.h"
#include "serial/BoardFacts.h"
#include "serial/SerialLink.h"
#include "storage/DeviceSettings.h"

namespace {

using gardener::BoardFacts;
using gardener::CommandQueue;
using gardener::DeviceSettings;
using gardener::Machine;
using gardener::MachineCommand;
using gardener::SerialLink;
using gardener::SystemState;
using gardener::host::TextStream;
using gardener::panel::parts::BuzzerPlayer;
using gardener::test::FakeBattery;
using gardener::test::FakeBuzzer;
using gardener::test::FakePump;
using gardener::test::FakeSensor;
using gardener::test::FakeValve;
using gardener::test::MemoryLibrary;
using gardener::test::RecordingLogger;

// The board's facts, as the sketch's own pair answer them.
const char* chipModel() { return "ESP32-S3"; }
uint32_t heapFacts() { return 123456; }

class OpenQueue : public CommandQueue {
 public:
  bool post(const MachineCommand&) override { return true; }
  bool take(MachineCommand&) override { return false; }
};

// The wire over a store the case fills: two folders, the device's document and two routines.
struct Bench {
  Bench()
      : machine(logger, &pump, &sensor, &valve), settings(&library, state), player(buzzerPart),
        link(io, nullptr, logger, queue,
             gardener::SerialParts{machine, board, battery, settings, player, library}) {
    gardener::host::reset();
    library.put("/cfg/settings.ini", "a=on\n");
    library.put("/routines/0_first.be", "import x\n");
    library.put("/routines/1_second.be", "import y\n");
    link.begin();
  }

  void send(const char* text) {
    io.clearPrinted();
    io.type(text);
    link.poll(gardener::host::nowMs());
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
  DeviceSettings settings;
  FakeBuzzer buzzerPart;
  BuzzerPlayer player;
  const BoardFacts board{&chipModel, &heapFacts};
  SerialLink link;
};

}  // namespace

TEST_CASE("list answers the root's folders in name order") {
  Bench bench;
  bench.send("fs list /\n");
  CHECK_EQ(bench.reply(), std::string("ok\ndir cfg\ndir routines\n\n"));
}

TEST_CASE("list marks a folder's files and answers no folders") {
  Bench bench;
  bench.send("fs list /routines\n");
  CHECK_EQ(bench.reply(), std::string("ok\nfile 0_first.be\nfile 1_second.be\n\n"));
}

TEST_CASE("list of a missing path is not found") {
  Bench bench;
  bench.send("fs list /nope\n");
  CHECK_EQ(bench.reply(), std::string("err not_found /nope\n"));
}

TEST_CASE("list of a file is not a directory") {
  Bench bench;
  bench.send("fs list /cfg/settings.ini\n");
  CHECK_EQ(bench.reply(), std::string("err not_ready not_dir\n"));
}

TEST_CASE("read answers the declared length and the text") {
  Bench bench;
  bench.send("fs read /cfg/settings.ini\n");
  CHECK_EQ(bench.reply(), std::string("ok bytes=5\na=on\n"));
}

TEST_CASE("read of a missing file is not found") {
  Bench bench;
  bench.send("fs read /cfg/missing.ini\n");
  CHECK_EQ(bench.reply(), std::string("err not_found /cfg/missing.ini\n"));
}

TEST_CASE("read of a folder is refused as a directory") {
  Bench bench;
  bench.send("fs read /cfg\n");
  CHECK_EQ(bench.reply(), std::string("err not_ready is_dir\n"));
}

TEST_CASE("a path that is not absolute is a bad argument") {
  Bench bench;
  bench.send("fs read cfg/settings.ini\n");
  CHECK_EQ(bench.reply(), std::string("err bad_argument cfg/settings.ini\n"));
}

TEST_CASE("a path with a parent segment is a bad argument") {
  Bench bench;
  bench.send("fs read /cfg/../cfg/settings.ini\n");
  CHECK_EQ(bench.reply(), std::string("err bad_argument /cfg/../cfg/settings.ini\n"));
}

TEST_CASE("write keeps the declared body") {
  Bench bench;
  bench.send("fs write /cfg/new.ini 3\nabc\n");
  CHECK_EQ(bench.reply(), std::string("ok\n"));
  CHECK_EQ(bench.library.textAt("/cfg/new.ini"), std::string("abc"));
}

TEST_CASE("write to the device's own document is refused and leaves it alone") {
  Bench bench;
  bench.send("fs write /cfg/settings.ini 3\nabc\n");
  CHECK_EQ(bench.reply(), std::string("err unsupported path=/cfg/settings.ini reason=reserved\n"));
  CHECK_EQ(bench.library.textAt("/cfg/settings.ini"), std::string("a=on\n"));
}

TEST_CASE("write into a folder that is not there has no parent") {
  Bench bench;
  bench.send("fs write /nope/new.ini 1\nx\n");
  CHECK_EQ(bench.reply(), std::string("err not_ready no_parent\n"));
}

TEST_CASE("write onto a folder is refused as a directory") {
  Bench bench;
  bench.send("fs write /cfg 1\nx\n");
  CHECK_EQ(bench.reply(), std::string("err not_ready is_dir\n"));
}

TEST_CASE("a body past the cap is refused with the maximum") {
  Bench bench;
  bench.send("fs write /cfg/new.ini 65537\n");
  CHECK_EQ(bench.reply(), std::string("err out_of_range field=bytes value=65537 max=65536\n"));
}

TEST_CASE("a body that stops arriving is a bad argument and leaves nothing behind") {
  Bench bench;
  bench.send("fs write /cfg/new.ini 4\nab\n");
  CHECK_EQ(bench.reply(), std::string("err bad_argument\n"));
  CHECK_FALSE(bench.library.has("/cfg/new.ini"));
}

TEST_CASE("mkdir makes a folder the next list answers") {
  Bench bench;
  bench.send("fs mkdir /cfg/sub\n");
  CHECK_EQ(bench.reply(), std::string("ok\n"));
  bench.send("fs list /cfg\n");
  CHECK_EQ(bench.reply(), std::string("ok\nfile settings.ini\ndir sub\n\n"));
}

TEST_CASE("mkdir of a path that exists is refused") {
  Bench bench;
  bench.send("fs mkdir /cfg\n");
  CHECK_EQ(bench.reply(), std::string("err not_ready exists\n"));
}

TEST_CASE("mkdir under a missing parent has no parent") {
  Bench bench;
  bench.send("fs mkdir /nope/sub\n");
  CHECK_EQ(bench.reply(), std::string("err not_ready no_parent\n"));
}

TEST_CASE("delete removes a file") {
  Bench bench;
  bench.send("fs delete /routines/0_first.be\n");
  CHECK_EQ(bench.reply(), std::string("ok\n"));
  CHECK_FALSE(bench.library.has("/routines/0_first.be"));
}

TEST_CASE("delete removes a folder with everything under it") {
  Bench bench;
  bench.send("fs delete /routines\n");
  CHECK_EQ(bench.reply(), std::string("ok\n"));
  CHECK_FALSE(bench.library.has("/routines/0_first.be"));
  CHECK_FALSE(bench.library.has("/routines/1_second.be"));
}

TEST_CASE("delete of the device's own document is refused") {
  Bench bench;
  bench.send("fs delete /cfg/settings.ini\n");
  CHECK_EQ(bench.reply(), std::string("err unsupported path=/cfg/settings.ini reason=reserved\n"));
  CHECK(bench.library.has("/cfg/settings.ini"));
}

TEST_CASE("delete of a missing path is not found") {
  Bench bench;
  bench.send("fs delete /cfg/missing.ini\n");
  CHECK_EQ(bench.reply(), std::string("err not_found /cfg/missing.ini\n"));
}

TEST_CASE("an unknown verb names itself") {
  Bench bench;
  bench.send("fs bogus\n");
  CHECK_EQ(bench.reply(), std::string("err unknown_verb bogus\n"));
}
