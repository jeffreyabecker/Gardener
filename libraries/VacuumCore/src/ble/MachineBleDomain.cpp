#include "ble/MachineBleDomain.h"

#if GARDENER_BLUETOOTH

#include <math.h>
#include <string.h>

#include "Machine.h"
#include "MachineCommand.h"
#include "VacuumSystem.h"
#include "ble/BleLink.h"
#include "firmwareversion.h"

namespace gardener {
namespace {

constexpr uint8_t kBleProtocolVersion = 1;

// The two rates; everything else is sent on change. 5 Hz is about as fast as
// a phone screen needs.
constexpr uint32_t kFastRateMs = 200;

// The opcodes a controller writes to `command`, and the results it is
// notified. The machine's answers fill the bottom of the range and the
// protocol's refusals sit at the top, so a client can tell which of the two
// spoke without knowing the list. `0x01` is retired: a start is valid at a
// target of 0 now, so "no target" is never answered.
enum : uint8_t { kOpStart = 0x01, kOpStop = 0x02, kOpResetFault = 0x03, kOpReboot = 0x04 };
enum : uint8_t {
  kResultApplied = 0x00,
  kResultNoReading = 0x02,
  kResultFault = 0x03,
  kResultUnknownOpcode = 0xFF,
};

// Why a write was refused, in the byte `write_result` carries after the field's suffix.
enum : uint8_t {
  kWriteOutOfRange = 0x01,
  kWriteNotANumber = 0x02,
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

const BleDomain::Spec MachineBleDomain::kSpecs[] = {
    {0x0009, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 1},                              // state
    {0x0002, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 4},                              // pressure_kpa
    {0x0003, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY, 4}, // target
    {0x0004, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 4},                              // duty
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
};

MachineBleDomain::MachineBleDomain(Machine* app) : app_(app) {}

uint8_t MachineBleDomain::specCount() const { return Count; }

BleDomain::Spec MachineBleDomain::specAt(uint8_t index) const { return kSpecs[index]; }

void MachineBleDomain::bind(uint8_t index, NimBLECharacteristic* characteristic) {
  characteristics_[index] = characteristic;
}

void MachineBleDomain::describe(uint8_t index) {
  VacuumSystem& vacuum = app_->vacuum();
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
    case Duty:
      setFloat(chr, vacuum.pump().dutyPercent());
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
      setByte(chr, kBleProtocolVersion);
      break;
    case Sensor:
      setByte(chr, plausibleReading(vacuum) ? 1 : 0);
      break;
    case Floor:
      setFloat(chr, CONTROL_FLOOR_KPA);
      break;
    case Caps:
      setByte(chr, app_->capabilities());
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
      setUint32(chr, app_->uptimeMs());
      break;
    case Count:
      break;
  }
}

void MachineBleDomain::write(uint8_t index, const NimBLEAttValue& value) {
  switch (index) {
    case Target:
      if (value.size() != sizeof(float)) return;
      {
        float pressureKpa = 0.0f;
        memcpy(&pressureKpa, value.data(), sizeof(pressureKpa));
        target_.value = pressureKpa;
        target_.has = true;
      }
      break;
    case Command:
      if (value.size() != 1) return;
      command_.value = value.data()[0];
      command_.has = true;
      break;
    case HoldS:
      if (value.size() != sizeof(uint16_t)) return;
      {
        uint16_t seconds = 0;
        memcpy(&seconds, value.data(), sizeof(seconds));
        hold_.value = seconds;
        hold_.has = true;
      }
      break;
    default:
      break;  // the characteristic's properties refuse a write to anything else
  }
}

// The loop's half of a write: the target and an opcode are applied here rather
// than in the callback, so nothing from the radio task touches the machine.
void MachineBleDomain::applyPending(BleLink& link) {
  if (target_.has) {
    target_.has = false;
    const float pressureKpa = target_.value;
    if (!isfinite(pressureKpa)) {
      refuse(link, Target, kWriteNotANumber);
    } else if (pressureKpa < 0.0f || pressureKpa > CONTROL_FLOOR_KPA) {
      refuse(link, Target, kWriteOutOfRange);
    } else {
      // 0 is the ambient target, which releases the chamber; anything else is a setpoint the
      // vacuum system accepts or refuses against the same floor.
      const MachineCommand set{MachineCommand::Op::SetTarget, pressureKpa};
      link.post(set);
    }
    // Read back rather than echo: a refused value leaves the target where it
    // was, and the read is the answer.
    describe(Target);
    const float applied = app_->vacuum().targetPressureKpa();
    if (applied != lastTarget_) {
      lastTarget_ = applied;
      link.notify(characteristics_[Target]);
    }
  }

  if (hold_.has) {
    hold_.has = false;
    const uint16_t seconds = hold_.value;
    const uint16_t maxSeconds = static_cast<uint16_t>(CONTROL_MAX_RUN_MS / 1000);
    if (seconds > maxSeconds) {
      refuse(link, HoldS, kWriteOutOfRange);
    } else {
      // 0 asks for the compiled maximum, which is what a start with no hold gets.
      requestedHoldS_ = seconds;
    }
    describe(HoldS);
    const uint16_t applied =
        app_->vacuum().running() ? static_cast<uint16_t>(app_->vacuum().holdSeconds()) : 0;
    if (applied != lastHoldS_) {
      lastHoldS_ = applied;
      link.notify(characteristics_[HoldS]);
    }
  }

  if (command_.has) {
    command_.has = false;
    const uint8_t result = execute(link, command_.value);
    setByte(characteristics_[Command], result);
    link.notify(characteristics_[Command]);
  }
}

// A refused write is not silence: the field's suffix and the reason are notified, while the
// read-back stays the authority on what the machine holds.
void MachineBleDomain::refuse(BleLink& link, uint8_t index, uint8_t reason) {
  NimBLECharacteristic* chr = characteristics_[WriteResult];
  if (chr == nullptr) return;
  const uint16_t suffix = kSpecs[index].suffix;
  const uint8_t payload[3] = {static_cast<uint8_t>(suffix & 0xFF),
                              static_cast<uint8_t>(suffix >> 8), reason};
  chr->setValue(payload, sizeof(payload));
  link.notify(chr);
}

// Runs one opcode. `start` reports the same refusals the serial link reports,
// from the same VacuumSystem::startRefusal(), so the two surfaces cannot
// disagree about why a run will not start. Every applied opcode is queued for
// the control task; only the refusal and the result byte are answered here.
uint8_t MachineBleDomain::execute(BleLink& link, uint8_t opcode) {
  VacuumSystem& vacuum = app_->vacuum();
  switch (opcode) {
    case kOpStop: {
      const MachineCommand stop{MachineCommand::Op::Stop};
      link.post(stop);
      return kResultApplied;
    }
    case kOpStart: {
      const char* refusal = vacuum.startRefusal();
      if (refusal != nullptr) {
        if (strcmp(refusal, "latch") == 0) return kResultFault;
        if (strcmp(refusal, "no_reading") == 0) return kResultNoReading;
        return kResultUnknownOpcode;
      }
      const MachineCommand start{MachineCommand::Op::Start, (float)requestedHoldS_};
      link.post(start);
      return kResultApplied;
    }
    case kOpResetFault: {
      const MachineCommand reset{MachineCommand::Op::ResetFault};
      link.post(reset);
      return kResultApplied;
    }
    case kOpReboot: {
      const MachineCommand reboot{MachineCommand::Op::Reboot};
      link.post(reboot);
      return kResultApplied;
    }
    default:
      return kResultUnknownOpcode;
  }
}

// Sends whatever is due. A value only moves when the machine moves it, so the
// read and the notification are one description of the same field.
void MachineBleDomain::publish(uint32_t nowMs, BleLink& link, bool baseline) {
  if (!link.connected()) return;

  VacuumSystem& vacuum = app_->vacuum();
  publishOnChange(State, static_cast<uint8_t>(vacuum.runState()), lastState_, baseline, link);
  publishOnChange(Sensor, plausibleReading(vacuum) ? 1 : 0, lastSensor_, baseline, link);
  publishOnChange(Target, vacuum.targetPressureKpa(), lastTarget_, baseline, link);
  publishOnChange(Floor, CONTROL_FLOOR_KPA, lastFloor_, baseline, link);

  const uint16_t holdS = vacuum.running() ? static_cast<uint16_t>(vacuum.holdSeconds()) : 0;
  if (baseline || holdS != lastHoldS_) {
    lastHoldS_ = holdS;
    describe(HoldS);
    link.notify(characteristics_[HoldS]);
  }

  const char* fault = vacuum.faultCode();
  if (baseline || lastFault_ != fault) {
    lastFault_ = fault;
    describe(Fault);
    link.notify(characteristics_[Fault]);
  }

  if (nowMs - lastFastMs_ >= kFastRateMs) {
    lastFastMs_ = nowMs;
    describe(PressureKpa);
    link.notify(characteristics_[PressureKpa]);
    describe(Duty);
    link.notify(characteristics_[Duty]);
    describe(UptimeMs);
    link.notify(characteristics_[UptimeMs]);
  }
}

void MachineBleDomain::publishOnChange(uint8_t index, uint8_t value, uint8_t& last, bool force,
                                       BleLink& link) {
  if (!force && value == last) return;
  last = value;
  describe(index);
  link.notify(characteristics_[index]);
}

void MachineBleDomain::publishOnChange(uint8_t index, float value, float& last, bool force,
                                       BleLink& link) {
  if (!force && value == last) return;
  last = value;
  describe(index);
  link.notify(characteristics_[index]);
}

void MachineBleDomain::poll(uint32_t nowMs, BleLink& link, bool baseline) {
  applyPending(link);
  publish(nowMs, link, baseline);
}

}  // namespace gardener

#endif  // GARDENER_BLUETOOTH
