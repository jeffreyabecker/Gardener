#include "ble/MachineBleDomain.h"

#if GARDENER_BLUETOOTH

#include <math.h>
#include <string.h>

#include "Machine.h"
#include "MachineCommand.h"
#include "Ownership.h"
#include "VacuumSystem.h"
#include "ble/BleLink.h"
#include "firmwareversion.h"
#include "storage/MachineSettings.h"

namespace gardener {
namespace {

constexpr uint8_t kBleProtocolVersion = 1;

// The two rates; everything else is sent on change. 5 Hz is about as fast as
// a phone screen needs.
constexpr uint32_t kFastRateMs = 200;

// The opcodes a controller writes to `command`, and the results it is
// notified. The machine's answers fill the bottom of the range and the
// protocol's refusals sit at the top, so a client can tell which of the two
// spoke without knowing the list.
enum : uint8_t { kOpStart = 0x01, kOpStop = 0x02, kOpResetFault = 0x03, kOpReboot = 0x04 };
enum : uint8_t {
  kResultApplied = 0x00,
  kResultNoTarget = 0x01,
  kResultNoReading = 0x02,
  kResultFault = 0x03,
  kResultNotControlling = 0xFE,
  kResultUnknownOpcode = 0xFF,
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
    {0x0001, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 1},                                // state
    {0x0002, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 4},                                // kpa
    {0x0003, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY, 4},   // target
    {0x0004, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 4},                                // duty
    {0x0005, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 12},                               // fault
    {0x0007, NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY, 1},                           // command
    {0x0008, NIMBLE_PROPERTY::READ, 16},                                                         // fw
    {0x0009, NIMBLE_PROPERTY::READ, 1},                                                          // ble_proto
    {0x000B, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY, 1},   // control
    {0x000C, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 1},                                // sensor
    {0x000D, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY, 4},   // floor
    {0x000E, NIMBLE_PROPERTY::READ, 1},                                                          // caps
    {0x000F, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY, 4},   // flow
    {0x0010, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY, 4},   // volume
    {0x0011, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC, 1},                             // valve
    {0x0012, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY, 4},   // pump_max
    {0x0013, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::NOTIFY, 4},   // deadband
};

MachineBleDomain::MachineBleDomain(Machine* app) : app_(app) {}

uint8_t MachineBleDomain::specCount() const { return Count; }

BleDomain::Spec MachineBleDomain::specAt(uint8_t index) const { return kSpecs[index]; }

void MachineBleDomain::bind(uint8_t index, NimBLECharacteristic* characteristic) {
  characteristics_[index] = characteristic;
}

bool MachineBleDomain::owned(const Link& link) const {
  return app_->ownership().ownedBy(link);
}

void MachineBleDomain::describe(uint8_t index) {
  VacuumSystem& vacuum = app_->vacuum();
  NimBLECharacteristic* chr = characteristics_[index];
  switch (index) {
    case State:
      setByte(chr, static_cast<uint8_t>(vacuum.runState()));
      break;
    case Kpa:
      setFloat(chr, vacuum.currentKpa());
      break;
    case Target:
      setFloat(chr, vacuum.targetKpa());
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
    case Control:
      setByte(chr, app_->ownership().owner().code());
      break;
    case Sensor:
      setByte(chr, plausibleReading(vacuum) ? 1 : 0);
      break;
    case Floor:
      setFloat(chr, app_->settings().floorKpa());
      break;
    case Caps:
      setByte(chr, app_->capabilities());
      break;
    case Flow:
      setFloat(chr, app_->settings().pumpFlowLpm());
      break;
    case Volume:
      setFloat(chr, app_->settings().chamberVolumeL());
      break;
    case Valve:
      // The fitted direction, not the stored key: a pending change applies on the next boot.
      setByte(chr, app_->vacuum().vent().energiseWhenOpen() ? 1 : 0);
      break;
    case PumpMax:
      setFloat(chr, app_->settings().pumpMaxPercent());
      break;
    case Deadband:
      setFloat(chr, app_->settings().deadbandKpa());
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
        float kpa = 0.0f;
        memcpy(&kpa, value.data(), sizeof(kpa));
        target_.value = kpa;
        target_.has = true;
      }
      break;
    case Command:
      if (value.size() != 1) return;
      command_.value = value.data()[0];
      command_.has = true;
      break;
    case Control:
      if (value.size() != 1) return;
      control_.value = value.data()[0];
      control_.has = true;
      break;
    case Flow:
      if (value.size() != sizeof(float)) return;
      {
        float lpm = 0.0f;
        memcpy(&lpm, value.data(), sizeof(lpm));
        flow_.value = lpm;
        flow_.has = true;
      }
      break;
    case Volume:
      if (value.size() != sizeof(float)) return;
      {
        float litres = 0.0f;
        memcpy(&litres, value.data(), sizeof(litres));
        volume_.value = litres;
        volume_.has = true;
      }
      break;
    case Floor:
      if (value.size() != sizeof(float)) return;
      {
        float kpa = 0.0f;
        memcpy(&kpa, value.data(), sizeof(kpa));
        floor_.value = kpa;
        floor_.has = true;
      }
      break;
    case Valve:
      if (value.size() != 1) return;
      valve_.value = value.data()[0];
      valve_.has = true;
      break;
    case PumpMax:
      if (value.size() != sizeof(float)) return;
      {
        float percent = 0.0f;
        memcpy(&percent, value.data(), sizeof(percent));
        pumpMax_.value = percent;
        pumpMax_.has = true;
      }
      break;
    case Deadband:
      if (value.size() != sizeof(float)) return;
      {
        float kpa = 0.0f;
        memcpy(&kpa, value.data(), sizeof(kpa));
        deadband_.value = kpa;
        deadband_.has = true;
      }
      break;
    default:
      break;  // the stack refuses a write to anything else before it gets here
  }
}

// The loop's half of a write: ownership, the target and an opcode are applied
// here rather than in the callback, so nothing from the radio task touches the
// machine.
void MachineBleDomain::applyPending(BleLink& link) {
  Ownership& ownership = app_->ownership();

  // Ownership first: every other write is gated on it, and a release is the
  // safe exit.
  if (control_.has) {
    control_.has = false;
    // A claim takes the top, a release leaves it; either way the control beat vents the hand-over.
    if (control_.value != 0) {
      ownership.acquire(link);
    } else {
      ownership.release(link);
    }
    // The caller's view: who is on top, after the write.
    describe(Control);
    link.notify(characteristics_[Control]);
    lastControl_ = ownership.owner().code();
  }

  if (target_.has) {
    target_.has = false;
    const float kpa = target_.value;
    if (owned(link) && isfinite(kpa)) {
      // 0 clears the target; anything else is the vacuum system's to accept or
      // refuse against its own floor, so the two surfaces cannot disagree.
      if (kpa == 0.0f) {
        const MachineCommand clear{MachineCommand::Op::ClearTarget};
        postMachineCommand(clear);
      } else {
        const MachineCommand set{MachineCommand::Op::SetTarget, kpa};
        postMachineCommand(set);
      }
    }
    // Read back rather than echo: a refused value leaves the target where it
    // was, and the read is the answer.
    describe(Target);
    const float applied = app_->vacuum().targetKpa();
    if (applied != lastTarget_) {
      lastTarget_ = applied;
      link.notify(characteristics_[Target]);
    }
  }

  // The two loop settings, applied the same way `target` is: an owner's write
  // retunes the loop, and the read-back is the answer.
  if (flow_.has) {
    flow_.has = false;
    const float lpm = flow_.value;
    if (owned(link) && isfinite(lpm) && lpm > 0.0f) {
      app_->settings().setPumpFlowLpm(lpm);
    }
    describe(Flow);
    const float applied = app_->settings().pumpFlowLpm();
    if (applied != lastFlow_) {
      lastFlow_ = applied;
      link.notify(characteristics_[Flow]);
    }
  }

  if (volume_.has) {
    volume_.has = false;
    const float litres = volume_.value;
    if (owned(link) && isfinite(litres) && litres > 0.0f) {
      app_->settings().setChamberVolumeL(litres);
    }
    describe(Volume);
    const float applied = app_->settings().chamberVolumeL();
    if (applied != lastVolume_) {
      lastVolume_ = applied;
      link.notify(characteristics_[Volume]);
    }
  }

  if (floor_.has) {
    floor_.has = false;
    const float kpa = floor_.value;
    if (owned(link) && isfinite(kpa) && kpa >= CONTROL_MIN_TARGET_KPA) {
      app_->settings().setFloorKpa(kpa);
    }
    describe(Floor);
    const float applied = app_->settings().floorKpa();
    if (applied != lastFloor_) {
      lastFloor_ = applied;
      link.notify(characteristics_[Floor]);
    }
  }

  if (valve_.has) {
    valve_.has = false;
    if (owned(link)) {
      // 0 is normally open, 1 is normally closed. The fitted part is chosen on
      // the next boot, so the characteristic keeps reading what is fitted now.
      app_->settings().setValveNormallyOpen(valve_.value == 0);
    }
    describe(Valve);
  }

  if (pumpMax_.has) {
    pumpMax_.has = false;
    const float percent = pumpMax_.value;
    if (owned(link) && isfinite(percent) && percent > 0.0f &&
        percent <= PUMP_DUTY_MAX_PERCENT) {
      app_->settings().setPumpMaxPercent(percent);
    }
    describe(PumpMax);
    const float applied = app_->settings().pumpMaxPercent();
    if (applied != lastPumpMax_) {
      lastPumpMax_ = applied;
      link.notify(characteristics_[PumpMax]);
    }
  }

  if (deadband_.has) {
    deadband_.has = false;
    const float kpa = deadband_.value;
    if (owned(link) && isfinite(kpa) && kpa > 0.0f) {
      app_->settings().setDeadbandKpa(kpa);
    }
    describe(Deadband);
    const float applied = app_->settings().deadbandKpa();
    if (applied != lastDeadband_) {
      lastDeadband_ = applied;
      link.notify(characteristics_[Deadband]);
    }
  }

  if (command_.has) {
    command_.has = false;
    const uint8_t result = execute(link, command_.value);
    setByte(characteristics_[Command], result);
    link.notify(characteristics_[Command]);
  }
}

// Runs one opcode. `start` reports the same refusals the serial link reports,
// from the same VacuumSystem::startRefusal(), so the two surfaces cannot
// disagree about why a run will not start. Every applied opcode is queued for
// the control task; only the refusal and the result byte are answered here.
uint8_t MachineBleDomain::execute(const Link& link, uint8_t opcode) {
  if (!owned(link)) return kResultNotControlling;

  VacuumSystem& vacuum = app_->vacuum();
  switch (opcode) {
    case kOpStart: {
      const char* refusal = vacuum.startRefusal();
      if (refusal != nullptr) {
        if (strcmp(refusal, "latch") == 0) return kResultFault;
        if (strcmp(refusal, "no_target") == 0) return kResultNoTarget;
        if (strcmp(refusal, "no_reading") == 0) return kResultNoReading;
        return kResultUnknownOpcode;
      }
      const MachineCommand start{MachineCommand::Op::Start};
      postMachineCommand(start);
      return kResultApplied;
    }
    case kOpStop: {
      const MachineCommand stop{MachineCommand::Op::Stop};
      postMachineCommand(stop);
      return kResultApplied;
    }
    case kOpResetFault: {
      const MachineCommand reset{MachineCommand::Op::ResetFault};
      postMachineCommand(reset);
      return kResultApplied;
    }
    case kOpReboot: {
      const MachineCommand reboot{MachineCommand::Op::Reboot};
      postMachineCommand(reboot);
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
  publishOnChange(Control, app_->ownership().owner().code(), lastControl_, baseline, link);
  publishOnChange(Target, vacuum.targetKpa(), lastTarget_, baseline, link);
  publishOnChange(Floor, app_->settings().floorKpa(), lastFloor_, baseline, link);
  publishOnChange(Flow, app_->settings().pumpFlowLpm(), lastFlow_, baseline, link);
  publishOnChange(Volume, app_->settings().chamberVolumeL(), lastVolume_, baseline, link);
  publishOnChange(PumpMax, app_->settings().pumpMaxPercent(), lastPumpMax_, baseline, link);
  publishOnChange(Deadband, app_->settings().deadbandKpa(), lastDeadband_, baseline, link);

  const char* fault = vacuum.faultCode();
  if (baseline || lastFault_ != fault) {
    lastFault_ = fault;
    describe(Fault);
    link.notify(characteristics_[Fault]);
  }

  if (nowMs - lastFastMs_ >= kFastRateMs) {
    lastFastMs_ = nowMs;
    describe(Kpa);
    link.notify(characteristics_[Kpa]);
    describe(Duty);
    link.notify(characteristics_[Duty]);
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
