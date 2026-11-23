// The Bluetooth link's packets: the status written field by field, the two command spellings and
// every malformed write, and the code tables the machine's words are sent as. The encoding is the
// part of the link a host can hold, so it has cases; the stack itself needs the board.

#include <doctest.h>

#include <stddef.h>
#include <stdint.h>

#include "GattValues.h"

namespace {

using gardener::RunState;
using gardener::SystemStatus;
namespace gatt = gardener::gatt;

}  // namespace

TEST_CASE("a status packet writes every field at its own little-endian offset") {
  SystemStatus status;
  status.uptimeMs = 0x01020304;
  status.pressureKpa = 12.5f;
  status.targetKpa = 40.0f;
  status.dutyPercent = 100.0f;
  status.state = RunState::Holding;
  status.fault = "sensor";
  status.holdTimeMs = 0x0A0B0C0D;

  const gatt::Status packet = gatt::encodeStatus(status);
  CHECK_EQ(gatt::StatusSize, static_cast<size_t>(22));
  const uint8_t expected[gatt::StatusSize] = {
      0x04, 0x03, 0x02, 0x01,  // uptime_ms
      0x00, 0x00, 0x48, 0x41,  // pressure_kpa, 12.5f
      0x00, 0x00, 0x20, 0x42,  // target, 40.0f
      0x00, 0x00, 0xC8, 0x42,  // duty_percent, 100.0f
      2,                       // state, Holding
      1,                       // fault, sensor
      0x0D, 0x0C, 0x0B, 0x0A,  // hold_time_ms
  };
  for (size_t i = 0; i < gatt::StatusSize; i++) {
    CHECK_EQ(static_cast<int>(packet.bytes[i]), static_cast<int>(expected[i]));
  }
}

TEST_CASE("every fault word has a code, and an unknown one is not healthy") {
  CHECK_EQ(static_cast<int>(gatt::faultCode(nullptr)), 0);
  CHECK_EQ(static_cast<int>(gatt::faultCode("")), 0);
  CHECK_EQ(static_cast<int>(gatt::faultCode("sensor")), 1);
  CHECK_EQ(static_cast<int>(gatt::faultCode("vent_timeout")), 2);
  CHECK_EQ(static_cast<int>(gatt::faultCode("overshoot")), 3);
  CHECK_EQ(static_cast<int>(gatt::faultCode("no_progress")), 4);
  CHECK_EQ(static_cast<int>(gatt::faultCode("something else")), 5);
}

TEST_CASE("a refusal word is a code and anything else is unknown") {
  CHECK_EQ(static_cast<int>(gatt::refusalCode("latch")), 1);
  CHECK_EQ(static_cast<int>(gatt::refusalCode("no_reading")), 2);
  CHECK_EQ(static_cast<int>(gatt::refusalCode(nullptr)), 3);
  CHECK_EQ(static_cast<int>(gatt::refusalCode("something else")), 3);
}

TEST_CASE("a command is one byte for vent and reset and five for a set") {
  gatt::Command command;

  const uint8_t vent[] = {2};
  REQUIRE(gatt::decodeCommand(vent, sizeof vent, command));
  CHECK(command.op == gatt::Op::Vent);

  const uint8_t reset[] = {3};
  REQUIRE(gatt::decodeCommand(reset, sizeof reset, command));
  CHECK(command.op == gatt::Op::Reset);

  const uint8_t set[] = {1, 0x00, 0x00, 0x20, 0x42};
  REQUIRE(gatt::decodeCommand(set, sizeof set, command));
  CHECK(command.op == gatt::Op::Set);
  CHECK_EQ(command.targetKpa, 40.0f);
}

TEST_CASE("a write that is neither spelling is refused and moves nothing") {
  gatt::Command command;

  const uint8_t set[] = {1, 0x00, 0x00, 0x20, 0x42};
  CHECK_FALSE(gatt::decodeCommand(set, 1, command));
  CHECK_FALSE(gatt::decodeCommand(set, 4, command));
  CHECK_FALSE(gatt::decodeCommand(set, 6, command));

  const uint8_t vent[] = {2, 0, 0, 0, 0};
  CHECK_FALSE(gatt::decodeCommand(vent, sizeof vent, command));

  const uint8_t unknown[] = {9};
  CHECK_FALSE(gatt::decodeCommand(unknown, sizeof unknown, command));

  const uint8_t two[] = {1, 0};
  CHECK_FALSE(gatt::decodeCommand(two, sizeof two, command));
}

TEST_CASE("a reply is the result and the refusal, in that order") {
  const gatt::Reply reply = gatt::encodeReply(gatt::Result::NotReady, gatt::Refusal::Latch);
  CHECK_EQ(static_cast<int>(reply.bytes[0]), 4);
  CHECK_EQ(static_cast<int>(reply.bytes[1]), 1);
}
