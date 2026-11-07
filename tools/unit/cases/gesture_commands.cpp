// The `gesture` domain: one panel word per line, posted where the keys post, and the two safety
// words answered with the machine's commands because a posted word never passes the classifier that
// names them. Nothing here is answered — a word taken or dropped leaves the stream empty — so the
// cases read the queue rather than the wire, and the only reply a line can earn is the link's own
// refusal of one it cannot hold.

#include <doctest.h>

#include <Arduino.h>

#include <string>
#include <vector>

#include "Clock.h"
#include "TextStream.h"
#include "core/SystemState.h"
#include "fakes/MemoryLibrary.h"
#include "fakes/Parts.h"
#include "machine/CommandQueue.h"
#include "machine/Machine.h"
#include "panel/GestureQueue.h"
#include "panel/parts/BuzzerPlayer.h"
#include "serial/BoardFacts.h"
#include "serial/SerialLink.h"
#include "storage/DeviceSettings.h"

namespace {

using gardener::BoardFacts;
using gardener::BuzzerPlayer;
using gardener::CommandQueue;
using gardener::DeviceSettings;
using gardener::GestureQueue;
using gardener::Machine;
using gardener::MachineCommand;
using gardener::SerialLink;
using gardener::SystemState;
using gardener::host::TextStream;
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

// The command queue the link posts into: everything fits, and a case can see what was posted.
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

  size_t count() const { return commands_.size(); }
  MachineCommand at(size_t index) const { return commands_[index]; }

 private:
  std::vector<MachineCommand> commands_;
};

// One link over the fakes, with the panel's queue in front of it so a case can take the word back.
struct Bench {
  Bench()
      : machine(logger, &pump, &sensor, &valve), settings(&library, state), player(buzzerPart),
        link(io, nullptr, logger, queue,
             gardener::SerialParts{machine, board, battery, settings, player, library, presses}) {
    gardener::host::reset();
    machine.boot();
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
  GestureQueue presses;
  DeviceSettings settings;
  FakeBuzzer buzzerPart;
  BuzzerPlayer player;
  const BoardFacts board{&chipModel, &heapFacts};
  SerialLink link;
};

std::string taken(Bench& bench) {
  const char* word = bench.presses.take();
  return word == nullptr ? std::string() : std::string(word);
}

}  // namespace

TEST_CASE("a gesture reaches the queue the page drains") {
  Bench bench;
  bench.send("gesture ok:single\n");
  CHECK_EQ(bench.reply(), std::string(""));
  CHECK_EQ(taken(bench), std::string("ok:single"));
  CHECK(bench.presses.take() == nullptr);
}

TEST_CASE("a gesture is taken in the order it was sent") {
  Bench bench;
  bench.send("gesture ok:single\n");
  bench.send("gesture forward:double+ok:single\n");
  CHECK_EQ(taken(bench), std::string("ok:single"));
  CHECK_EQ(taken(bench), std::string("forward:double+ok:single"));
  CHECK(bench.presses.take() == nullptr);
}

TEST_CASE("a domain with no word answers a bad argument and posts nothing") {
  Bench bench;
  bench.send("gesture\n");
  CHECK_EQ(bench.reply(), std::string("err bad_argument\n"));
  CHECK(bench.presses.take() == nullptr);
}

TEST_CASE("a line over the maximum is refused and posts nothing") {
  Bench bench;
  const std::string line =
      "gesture " + std::string((size_t)gardener::CommandLineMax + 1, 'o') + "\n";
  bench.send(line.c_str());
  CHECK_EQ(bench.reply(), std::string("err bad_argument\n"));
  CHECK(bench.presses.take() == nullptr);
}

TEST_CASE("a chord out of button order is dropped") {
  Bench bench;
  bench.send("gesture ok:single+up:single\n");
  CHECK_EQ(bench.reply(), std::string(""));
  CHECK(bench.presses.take() == nullptr);
}

TEST_CASE("a repeated button is dropped") {
  Bench bench;
  bench.send("gesture ok:single+ok:double\n");
  CHECK_EQ(bench.reply(), std::string(""));
  CHECK(bench.presses.take() == nullptr);
}

TEST_CASE("a part naming no button is dropped") {
  Bench bench;
  bench.send("gesture select:single\n");
  CHECK_EQ(bench.reply(), std::string(""));
  CHECK(bench.presses.take() == nullptr);
}

TEST_CASE("a part naming no form is dropped") {
  Bench bench;
  bench.send("gesture ok:triple\n");
  CHECK_EQ(bench.reply(), std::string(""));
  CHECK(bench.presses.take() == nullptr);
}

TEST_CASE("a form that is not whole seconds is dropped") {
  Bench bench;
  bench.send("gesture ok:2s\n");
  CHECK_EQ(bench.reply(), std::string(""));
  CHECK(bench.presses.take() == nullptr);
}

TEST_CASE("a hold's whole seconds are a form of their own") {
  Bench bench;
  bench.send("gesture cancel:7\n");
  CHECK_EQ(taken(bench), std::string("cancel:7"));
}

TEST_CASE("a cmd: word is carried whole") {
  Bench bench;
  bench.send("gesture cmd:stop\n");
  CHECK_EQ(taken(bench), std::string("cmd:stop"));
}

TEST_CASE("a cmd: word with no name is dropped") {
  Bench bench;
  bench.send("gesture cmd:\n");
  CHECK_EQ(bench.reply(), std::string(""));
  CHECK(bench.presses.take() == nullptr);
}

TEST_CASE("a cmd: word inside a chord is dropped") {
  Bench bench;
  bench.send("gesture cmd:stop+ok:single\n");
  CHECK_EQ(bench.reply(), std::string(""));
  CHECK(bench.presses.take() == nullptr);
}

TEST_CASE("system:reboot posts the restart and not the word") {
  Bench bench;
  bench.send("gesture system:reboot\n");
  REQUIRE_EQ(bench.queue.count(), 1u);
  CHECK(bench.queue.at(0).op == MachineCommand::Op::Reboot);
  CHECK(bench.presses.take() == nullptr);
}

TEST_CASE("system:quit stops and vents and leaves the page") {
  Bench bench;
  bench.send("gesture system:quit\n");
  REQUIRE_EQ(bench.queue.count(), 1u);
  CHECK(bench.queue.at(0).op == MachineCommand::Op::Stop);
  CHECK(bench.presses.take() == nullptr);
}

TEST_CASE("a system: word that is not one of the two is dropped") {
  Bench bench;
  bench.send("gesture system:panic\n");
  CHECK_EQ(bench.reply(), std::string(""));
  CHECK(bench.queue.count() == 0);
  CHECK(bench.presses.take() == nullptr);
}
