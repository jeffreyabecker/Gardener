#include <math.h>
#include <string.h>

#include "ble/BleLink.h"
#include "core/Battery.h"
#include "core/Units.h"
#include "core/firmwareversion.h"
#include "machine/Machine.h"
#include "machine/MachineCommand.h"
#include "machine/VacuumSystem.h"
#include "storage/DeviceSettings.h"

namespace gardener {
namespace {

constexpr uint8_t BleProtocolVersion = 1;

// The two rates; everything else is sent on change. 5 Hz is about as fast as
// a phone screen needs, and the gauge moves far more slowly than that.
constexpr uint32_t FastRateMs = 200;
constexpr uint32_t BattRateMs = 10000;

// The opcodes a controller writes to `command`, and the results it is
// notified. The machine's answers fill the bottom of the range and the
// protocol's refusals sit at the top, so a client can tell which of the two
// spoke without knowing the list. `0x01` is retired: a start is valid at a
// target of 0 now, so "no target" is never answered.
enum : uint8_t { OpStart = 0x01, OpStop = 0x02, OpResetFault = 0x03, OpReboot = 0x04 };
enum : uint8_t {
  ResultApplied = 0x00,
  ResultNoReading = 0x02,
  ResultFault = 0x03,
  ResultUnknownOpcode = 0xFF,
};

// Why a write was refused, in the byte `write_result` carries after the field's suffix.
enum : uint8_t {
  WriteOutOfRange = 0x01,
  WriteNotANumber = 0x02,
};

// Whether a reading can be trusted, which is what `sensor` reports and what a
// run needs.
bool plausibleReading(VacuumSystem& vacuum) {
  return vacuum.sensor().present() && vacuum.sensor().readingValid();
}

static_assert(static_cast<uint8_t>(RunState::Idle) == 0 &&
                  static_cast<uint8_t>(RunState::Pulling) == 1 &&
                  static_cast<uint8_t>(RunState::Holding) == 2 &&
                  static_cast<uint8_t>(RunState::Venting) == 3 &&
                  static_cast<uint8_t>(RunState::Fault) == 4,
              "RunState is the `state` characteristic's encoding");

}  // namespace

// The table, in the order the link creates the characteristics: the machine's fields, then the
// panel's `unit` and `batt`.
const BleLink::Spec BleLink::Specs[] = {
    {0x0009, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 1},                              // state
    {0x0002, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 4},                              // pressure_kpa
    {0x0003, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY, 4}, // target
    {0x0005, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 12},                             // fault
    {0x0007, NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY, 1},                         // command
    {0x0008, NIMBLE_PROPERTY::READ, 16},                                                       // fw
    {0x0001, NIMBLE_PROPERTY::READ, 1},                                                        // ble_proto
    {0x000C, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 1},                              // sensor
    {0x000D, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 4},                              // floor
    {0x000E, NIMBLE_PROPERTY::READ, 1},                                                        // caps
    {0x0014, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY, 2}, // hold_s
    {0x0015, NIMBLE_PROPERTY::READ, 2},                                                        // max_run_s
    {0x0016, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 3},                              // write_result
    {0x0017, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 4},                              // uptime_ms
    {0x000A, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC, 1},                           // unit
    {0x0006, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 1},                              // batt
};

void BleLink::describe(uint8_t index) {
  VacuumSystem& vacuum = machine_.vacuum();
  NimBLECharacteristic* chr = characteristics_[index];
  switch (index) {
    case State:
      setByte(chr, static_cast<uint8_t>(vacuum.runState()));
      break;
    case PressureKpa:
      setFloat(chr, vacuum.currentPressureKpa());
      break;
    case Target:
      setFloat(chr, vacuum.targetPressureKpa());
      break;
    case Fault:
      chr->setValue(vacuum.faultCode());
      break;
    case Command:
      break;  // the last result, written when an opcode is applied
    case Firmware:
      chr->setValue(FirmwareVersion);
      break;
    case Protocol:
      setByte(chr, BleProtocolVersion);
      break;
    case Sensor:
      setByte(chr, plausibleReading(vacuum) ? 1 : 0);
      break;
    case Floor:
      setFloat(chr, CONTROL_FLOOR_KPA);
      break;
    case Caps:
      setByte(chr, machine_.capabilities());
      break;
    case HoldS:
      // The hold granted to the run, not the one a write asked for: 0 while nothing is running.
      setUint16(chr, vacuum.running() ? (uint16_t)vacuum.holdSeconds() : 0);
      break;
    case MaxRunS:
      setUint16(chr, (uint16_t)(CONTROL_MAX_RUN_MS / 1000));
      break;
    case WriteResult:
      break;  // the last refusal, written when a write is turned away
    case UptimeMs:
      setUint32(chr, machine_.uptimeMs());
      break;
    case Unit:
      setByte(chr, static_cast<uint8_t>(settings_.unit()));
      break;
    case Batt:
      setByte(chr, battery_.present() ? battery_.percent() : 0xFF);
      break;
    case Count:
      break;
  }
}

void BleLink::write(uint8_t index, const NimBLEAttValue& value) {
  switch (index) {
    case Target:
      if (value.size() != sizeof(float)) return;
      {
        float pressureKpa = 0.0f;
        memcpy(&pressureKpa, value.data(), sizeof(pressureKpa));
        targetWrite_.value = pressureKpa;
        targetWrite_.has = true;
      }
      break;
    case Command:
      if (value.size() != 1) return;
      commandWrite_.value = value.data()[0];
      commandWrite_.has = true;
      break;
    case HoldS:
      if (value.size() != sizeof(uint16_t)) return;
      {
        uint16_t seconds = 0;
        memcpy(&seconds, value.data(), sizeof(seconds));
        holdWrite_.value = seconds;
        holdWrite_.has = true;
      }
      break;
    case Unit:
      if (value.size() != 1) return;
      unitWrite_.value = value.data()[0];
      unitWrite_.has = true;
      break;
    default:
      break;  // the characteristic's properties refuse a write to anything else
  }
}

// The loop's half of a write: the target and an opcode are applied here rather
// than in the callback, so nothing from the radio task touches the machine.
void BleLink::applyPending() {
  if (targetWrite_.has) {
    targetWrite_.has = false;
    const float pressureKpa = targetWrite_.value;
    if (!isfinite(pressureKpa)) {
      refuse(Target, WriteNotANumber);
    } else if (pressureKpa < 0.0f || pressureKpa > CONTROL_FLOOR_KPA) {
      refuse(Target, WriteOutOfRange);
    } else {
      // 0 is the ambient target, which releases the chamber; anything else is a setpoint the
      // vacuum system accepts or refuses against the same floor.
      const MachineCommand set{MachineCommand::Op::SetTarget, pressureKpa};
      post(set);
    }
    // Read back rather than echo: a refused value leaves the target where it
    // was, and the read is the answer.
    describe(Target);
    const float applied = machine_.vacuum().targetPressureKpa();
    if (applied != lastTarget_) {
      lastTarget_ = applied;
      notify(characteristics_[Target]);
    }
  }

  if (holdWrite_.has) {
    holdWrite_.has = false;
    const uint16_t seconds = holdWrite_.value;
    const uint16_t maxSeconds = static_cast<uint16_t>(CONTROL_MAX_RUN_MS / 1000);
    if (seconds > maxSeconds) {
      refuse(HoldS, WriteOutOfRange);
    } else {
      // 0 asks for the compiled maximum, which is what a start with no hold gets.
      requestedHoldS_ = seconds;
    }
    describe(HoldS);
    const uint16_t applied =
        machine_.vacuum().running() ? static_cast<uint16_t>(machine_.vacuum().holdSeconds()) : 0;
    if (applied != lastHoldS_) {
      lastHoldS_ = applied;
      notify(characteristics_[HoldS]);
    }
  }

  if (commandWrite_.has) {
    commandWrite_.has = false;
    const uint8_t result = execute(commandWrite_.value);
    setByte(characteristics_[Command], result);
    notify(characteristics_[Command]);
  }

  if (unitWrite_.has) {
    unitWrite_.has = false;
    // Not notified: one central at a time means the client that wrote it
    // already knows. The read-back is the answer.
    if (unitWrite_.value < DisplayUnitCount) {
      const DisplayUnit unit = static_cast<DisplayUnit>(unitWrite_.value);
      settings_.setUnit(unit);
      settings_.save();
    }
    describe(Unit);
  }
}

// A refused write is not silence: the field's suffix and the reason are notified, while the
// read-back stays the authority on what the machine holds.
void BleLink::refuse(uint8_t index, uint8_t reason) {
  NimBLECharacteristic* chr = characteristics_[WriteResult];
  if (chr == nullptr) return;
  const uint16_t suffix = Specs[index].suffix;
  const uint8_t payload[3] = {static_cast<uint8_t>(suffix & 0xFF),
                              static_cast<uint8_t>(suffix >> 8), reason};
  chr->setValue(payload, sizeof(payload));
  notify(chr);
}

// Runs one opcode. `start` reports the same refusals the serial link reports,
// from the same VacuumSystem::startRefusal(), so the two surfaces cannot
// disagree about why a run will not start. Every applied opcode is queued for
// the control task; only the refusal and the result byte are answered here.
uint8_t BleLink::execute(uint8_t opcode) {
  VacuumSystem& vacuum = machine_.vacuum();
  switch (opcode) {
    case OpStop: {
      const MachineCommand stop{MachineCommand::Op::Stop};
      post(stop);
      return ResultApplied;
    }
    case OpStart: {
      const char* refusal = vacuum.startRefusal();
      if (refusal != nullptr) {
        if (strcmp(refusal, "latch") == 0) return ResultFault;
        if (strcmp(refusal, "no_reading") == 0) return ResultNoReading;
        return ResultUnknownOpcode;
      }
      const MachineCommand start{MachineCommand::Op::Start, (float)requestedHoldS_};
      post(start);
      return ResultApplied;
    }
    case OpResetFault: {
      const MachineCommand reset{MachineCommand::Op::ResetFault};
      post(reset);
      return ResultApplied;
    }
    case OpReboot: {
      const MachineCommand reboot{MachineCommand::Op::Reboot};
      post(reboot);
      return ResultApplied;
    }
    default:
      return ResultUnknownOpcode;
  }
}

// Sends whatever is due. A value only moves when the machine moves it, so the
// read and the notification are one description of the same field.
void BleLink::publish(uint32_t nowMs, bool baseline) {
  // The gauge is the panel's own field and a slow reading: it refreshes on its own rate whether or
  // not a central is connected, so a read always answers something recent.
  if (nowMs - lastBattMs_ >= BattRateMs) {
    lastBattMs_ = nowMs;
    describe(Batt);
    notify(characteristics_[Batt]);
  }

  if (!connected()) return;

  VacuumSystem& vacuum = machine_.vacuum();
  publishOnChange(State, static_cast<uint8_t>(vacuum.runState()), lastState_, baseline);
  publishOnChange(Sensor, plausibleReading(vacuum) ? 1 : 0, lastSensor_, baseline);
  publishOnChange(Target, vacuum.targetPressureKpa(), lastTarget_, baseline);
  publishOnChange(Floor, CONTROL_FLOOR_KPA, lastFloor_, baseline);

  const uint16_t holdS = vacuum.running() ? static_cast<uint16_t>(vacuum.holdSeconds()) : 0;
  if (baseline || holdS != lastHoldS_) {
    lastHoldS_ = holdS;
    describe(HoldS);
    notify(characteristics_[HoldS]);
  }

  const char* fault = vacuum.faultCode();
  if (baseline || lastFault_ != fault) {
    lastFault_ = fault;
    describe(Fault);
    notify(characteristics_[Fault]);
  }

  if (nowMs - lastFastMs_ >= FastRateMs) {
    lastFastMs_ = nowMs;
    describe(PressureKpa);
    notify(characteristics_[PressureKpa]);
    describe(UptimeMs);
    notify(characteristics_[UptimeMs]);
  }
}

void BleLink::publishOnChange(uint8_t index, uint8_t value, uint8_t& last, bool force) {
  if (!force && value == last) return;
  last = value;
  describe(index);
  notify(characteristics_[index]);
}

void BleLink::publishOnChange(uint8_t index, float value, float& last, bool force) {
  if (!force && value == last) return;
  last = value;
  describe(index);
  notify(characteristics_[index]);
}

}  // namespace gardener
