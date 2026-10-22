#pragma once

#include <stdint.h>

#include "core/Config.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/VentValve.h"

namespace gardener {

// What the device is doing, which is also the state a controller watches. There is no DONE:
// after a release it is IDLE, and the controller knows when the run is over because it is the one
// that ended it. HOLD is the target-reached edge, and it is the cue a controller times its hold
// from. VENT is the valve open to raise pressure: a release to ambient, or a correction to a
// shallower target, which returns to PULL/HOLD. There is no MANUAL (raw duty is not part of the
// interface, Q15).
enum class RunState : uint8_t { Idle, Pulling, Holding, Venting, Fault };

inline const char* runStateName(RunState state) {
  switch (state) {
    case RunState::Idle: return "IDLE";
    case RunState::Pulling: return "PULL";
    case RunState::Holding: return "HOLD";
    case RunState::Venting: return "VENT";
    case RunState::Fault: return "FAULT";
    default: return "?";
  }
}

// The vacuum system: the pressure loop, the release path and the run state, between the devices and
// the wire. The devices are not its own — it is handed the three it commands, so the pins and the
// bus stay in hal/ and this file stays the control law.
//
// It is told what to do — regulate to this target, start, stop, rest — and it reports what is
// true: the pressure, the duty, the state. The state is the vacuum system's own, because both of the
// things that advance it are facts about the chamber rather than orders: the target has been
// reached, and the release is over. So each command sets the state it causes and service()
// moves the other two, which keeps one machine with one owner.
//
// The run's duration is not a fact about the vacuum system and is not here: the device holds until it is told to
// stop, and nothing times a run.
//
// Pressures here are depths below ambient, in kPa: a target is a depth to pull to, and
// currentKpa() is how deep the chamber is now. The reference is the sensor's own — the part takes
// it when it comes up — so it cancels out of every comparison against a target.
//
// The loop is a plain PID on that relative pressure with the pump as the only actuator, so it
// can pull down but never vent; the valve is the only way back up, which is why every release
// path here stops the pump before it opens the valve. A target set shallower than the chamber
// while a run is active uses the same actuator: the valve opens until the new target is reached,
// then the run carries on. Gains live in core/Config.h.
class VacuumSystem {
 public:
  VacuumSystem(Pump* pump, PressureSensor* sensor, VentValve* vent)
      : pump_(pump), sensor_(sensor), vent_(vent) {}

  void begin();                  // pump off, valve open: the safe resting state
  void service(uint32_t nowMs);  // the state machine, the PID step and the fault detection

  // --- commands -----------------------------------------------------------
  void setTarget(float kpa);  // kPa below ambient
  // A run without a target is not a run, so clearing one releases the chamber.
  void clearTarget();
  bool start();                   // false: no target/reading, or faulted
  // Which of the three refusals start() would answer with, or null when it would start. The wire
  // reports this, so the reply and the refusal come from one place.
  const char* startRefusal() const;
  void stop();                    // the one release path: stop + vent
  void resetFault();

  // --- the reads ----------------------------------------------------------
  // Depth below ambient as the sensor reports it: the frame the target is stated in, so it is the
  // same number `vacuum get` reports as `kpa`. Zero while a reading is invalid.
  float currentKpa() const;
  RunState runState() const { return state_; }
  float targetKpa() const { return targetKpa_; }
  const char* faultCode() const { return faultCode_; }

  // The devices, so the serial layer can read what the loop commands without a second copy.
  Pump& pump() { return *pump_; }
  VentValve& vent() { return *vent_; }
  PressureSensor& sensor() { return *sensor_; }

 private:
  void setState(RunState state);

  // The chamber's own answers, rather than queries for anyone outside: these are the two
  // transitions that are facts about the chamber instead of orders, and service() is where
  // they are asked.
  bool atTarget() const;  // close enough to the target, deadband included
  bool released() const;  // settled within VENT_MARGIN_KPA, or VENT_TIMEOUT_MS elapsed
  void release();         // stop, then open: the one release path
  void rest();            // the resting state without releasing: pump off, valve open
  void startTargetVent(); // stop, open, and land on the current target
  void endTargetVent();   // close the valve and hand the run back to the PID

  void regulate(uint32_t nowMs);
  void fail(const char* code);
  // The latch is the fault code: empty means nothing is latched, so there is no second flag to
  // keep in step with it, and nothing but resetFault() clears it.
  bool faulted() const { return faultCode_[0] != '\0'; }

  void pidReset() { pidIntegral_ = 0.0f; }
  float pidUpdate(float dt, float error, float measurementSlew);

  Pump* pump_;
  PressureSensor* sensor_;
  VentValve* vent_;

  RunState state_ = RunState::Idle;
  float targetKpa_ = 0.0f;  // 0 while no target is set, which is what `vacuum get` reports
  const char* faultCode_ = "";
  uint32_t ventStartMs_ = 0;
  // VENT is either a release to ambient or a controlled vent to the current target; this is what
  // tells them apart when the end is decided.
  bool targetVent_ = false;

  float pidIntegral_ = 0.0f;
  float lastMeasured_ = 0.0f;
  float output_ = 0.0f;
  uint32_t lastControlMs_ = 0;
  uint32_t lastDerivativeMs_ = 0;
};

}  // namespace gardener
