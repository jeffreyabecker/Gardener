#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "machine/SystemControl.h"

namespace gardener::gatt {

// The three packets the Bluetooth link carries, and nothing of the board: a status into bytes, a
// write into a command, an outcome into a reply. Little-endian throughout, floats as IEEE-754
// binary32, and every field written to its offset rather than by reinterpreting a struct, whose
// padding is the compiler's.
constexpr size_t StatusSize = 18;
constexpr size_t CommandSize = 5;  // a set, op and target; vent and reset are one byte
constexpr size_t ReplySize = 2;

// The op byte of a command: each command has one spelling, so a five-byte vent is BadCommand.
enum class Op : uint8_t { Set = 1, Vent = 2, Reset = 3 };

// The result byte of a reply: ServeResult's own values first, then the link's own refusal, so a
// client that has the machine's table has most of this one.
enum class Result : uint8_t {
  Ok = 0,
  Released = 1,
  BelowRange = 2,
  AboveRange = 3,
  NotReady = 4,
  BadCommand = 5,
};

// The refusal byte, beside the result and for the same reason: the machine names its refusals in
// words that its log reads back, and the air carries codes.
enum class Refusal : uint8_t { None = 0, Latch = 1, NoReading = 2, Unknown = 3 };

static_assert(
    static_cast<uint8_t>(ServeResult::Ok) == static_cast<uint8_t>(Result::Ok) &&
        static_cast<uint8_t>(ServeResult::Released) == static_cast<uint8_t>(Result::Released) &&
        static_cast<uint8_t>(ServeResult::BelowRange) == static_cast<uint8_t>(Result::BelowRange) &&
        static_cast<uint8_t>(ServeResult::AboveRange) == static_cast<uint8_t>(Result::AboveRange) &&
        static_cast<uint8_t>(ServeResult::NotReady) == static_cast<uint8_t>(Result::NotReady),
    "the result byte writes ServeResult's own values");
static_assert(static_cast<uint8_t>(RunState::Idle) == 0 &&
                  static_cast<uint8_t>(RunState::Pulling) == 1 &&
                  static_cast<uint8_t>(RunState::Holding) == 2 &&
                  static_cast<uint8_t>(RunState::Venting) == 3 &&
                  static_cast<uint8_t>(RunState::Fault) == 4,
              "the state byte writes RunState's own values");

inline void putU32(uint8_t* out, uint32_t value) {
  out[0] = (uint8_t)value;
  out[1] = (uint8_t)(value >> 8);
  out[2] = (uint8_t)(value >> 16);
  out[3] = (uint8_t)(value >> 24);
}

inline uint32_t getU32(const uint8_t* in) {
  return (uint32_t)in[0] | ((uint32_t)in[1] << 8) | ((uint32_t)in[2] << 16) |
         ((uint32_t)in[3] << 24);
}

inline void putF32(uint8_t* out, float value) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof bits);
  putU32(out, bits);
}

inline float getF32(const uint8_t* in) {
  const uint32_t bits = getU32(in);
  float value;
  memcpy(&value, &bits, sizeof value);
  return value;
}

// The code a fault word is sent as. A word the table does not hold is the last code rather than a
// zero: a client must not read an unknown latch as a healthy machine.
inline uint8_t faultCode(const char* fault) {
  if (!fault || fault[0] == '\0') return 0;
  if (strcmp(fault, "sensor") == 0) return 1;
  if (strcmp(fault, "vent_timeout") == 0) return 2;
  if (strcmp(fault, "overshoot") == 0) return 3;
  if (strcmp(fault, "no_progress") == 0) return 4;
  return 5;
}

// The code a refusal word is sent as, asked only where the answer was NotReady.
inline uint8_t refusalCode(const char* refusal) {
  if (!refusal) return static_cast<uint8_t>(Refusal::Unknown);
  if (strcmp(refusal, "latch") == 0) return static_cast<uint8_t>(Refusal::Latch);
  if (strcmp(refusal, "no_reading") == 0) return static_cast<uint8_t>(Refusal::NoReading);
  return static_cast<uint8_t>(Refusal::Unknown);
}

// The status packet: the reading, the target, the pump, the state and the latch, in that order.
struct Status {
  uint8_t bytes[StatusSize];
};

inline Status encodeStatus(const SystemStatus& status) {
  Status packet;
  putU32(packet.bytes, status.uptimeMs);
  putF32(packet.bytes + 4, status.pressureKpa);
  putF32(packet.bytes + 8, status.targetKpa);
  putF32(packet.bytes + 12, status.dutyPercent);
  packet.bytes[16] = static_cast<uint8_t>(status.state);
  packet.bytes[17] = faultCode(status.fault);
  return packet;
}

// One command write: vent and reset are the op byte alone, and a set carries its target after it.
struct Command {
  Op op = Op::Set;
  float targetKpa = 0.0f;
};

// True when the write is one of the three spellings; false is the link's own BadCommand.
inline bool decodeCommand(const uint8_t* data, size_t length, Command& command) {
  if (length == 1) {
    if (data[0] == static_cast<uint8_t>(Op::Vent)) {
      command.op = Op::Vent;
      return true;
    }
    if (data[0] == static_cast<uint8_t>(Op::Reset)) {
      command.op = Op::Reset;
      return true;
    }
    return false;
  }

  if (length == CommandSize && data[0] == static_cast<uint8_t>(Op::Set)) {
    command.op = Op::Set;
    command.targetKpa = getF32(data + 1);
    return true;
  }

  return false;
}

// The reply: what the machine or the link answered, and why a set was refused.
struct Reply {
  uint8_t bytes[ReplySize];
};

inline Reply encodeReply(Result result, Refusal refusal) {
  Reply packet;
  packet.bytes[0] = static_cast<uint8_t>(result);
  packet.bytes[1] = static_cast<uint8_t>(refusal);
  return packet;
}

}  // namespace gardener::gatt
