#pragma once

#include <stdint.h>

#include "core/Config.h"
#include "core/Snapshot.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/VentValve.h"

namespace eggplan {

// The plant: the pressure loop, the release path and the run state, between the drivers and
// the wire.
//
// It is told what to do — regulate to this target, start, stop, rest — and it reports what is
// true: the pressure, the duty, the state. The state is the plant's own, because both of the
// things that advance it are facts about the chamber rather than orders: the target has been
// reached, and the release is over. So each command sets the state it causes and service()
// moves the other two, which keeps one machine with one owner.
//
// The run's duration is not a plant fact and is not here: the device holds until it is told to
// stop, and nothing times a run.
//
// The loop is a plain PID on absolute pressure with the pump as the only actuator, so it can
// pull down but never vent; the valve is the only way back up, which is why every release
// path here stops the pump before it opens the valve. Gains live in core/Config.h.
class VacuumSystem {
 public:
  VacuumSystem(Pump& pump, PressureSensor& sensor, VentValve& vent)
      : pump_(pump), sensor_(sensor), vent_(vent) {}

  void begin();                  // pump off, valve open: the safe resting state
  void service(uint32_t nowMs);  // the state machine, the PID step and the fault detection

  // --- the target ---------------------------------------------------------
  void setTarget(float kpa);
  // A run without a target is not a run, so clearing one releases the chamber.
  void clearTarget();
  bool targetActive() const { return targetActive_; }
  float targetKpa() const { return targetKpa_; }

  // --- the loop and the release path --------------------------------------
  bool start();  // false when there is no target, no reading, or a latched fault
  void stop();   // stop and release
  void vent();   // stop, then open: the one release path
  void rest();   // the resting state without releasing: pump off, valve open

  // --- the run state ------------------------------------------------------
  // What the device is doing. Each value is set by the command that caused it, or by
  // service() when the chamber settles into it.
  RunState state() const { return state_; }

  // --- what the chamber is doing ------------------------------------------
  float kpa() const { return sensor_.kpa(); }
  float vacuumKpa() const { return sensor_.vacuumKpa(); }
  float vacuumPercent() const { return sensor_.vacuumPercent(); }
  float ambientKpa() const { return sensor_.ambientKpa(); }
  bool sensorOk() const { return sensor_.present(); }
  bool readingValid() const { return sensor_.readingValid(); }
  float duty() const { return pump_.dutyPercent(); }

  // Deepest target the pump can actually reach: ambient minus its suction rating,
  // floored by the sanity limit.
  float minReachableKpa() const;

  // The reference "vacuum" is measured against, so the definition sits with the loop.
  bool setAmbientKpa(float kpa) { return sensor_.setAmbientKpa(kpa); }
  void captureAmbient() { sensor_.captureAmbient(); }

  // --- what went wrong ----------------------------------------------------
  // True once the plant has failed and its outputs are safe. The latch is here and only
  // resetFault() clears it, so a fault is permanent until someone asks; state() says the same
  // thing as Fault while it is held.
  bool faulted() const { return faulted_; }
  const char* faultCode() const { return pump_.faultCode(); }
  void resetFault();

 private:
  void setState(RunState state);

  // The chamber's own answers, rather than queries for anyone outside: these are the two
  // transitions that are facts about the chamber instead of orders, and service() is where
  // they are asked.
  bool atTarget() const;  // close enough to the target, deadband included
  bool vented() const;    // settled within VENT_MARGIN_KPA, or VENT_TIMEOUT_MS elapsed

  void regulate(uint32_t nowMs);
  void fail(const char* code);

  void pidReset() { pidIntegral_ = 0.0f; }
  float pidUpdate(float dt, float error, float measurementSlew);

  Pump& pump_;
  PressureSensor& sensor_;
  VentValve& vent_;

  RunState state_ = RunState::Idle;
  bool targetActive_ = false;
  float targetKpa_ = 0.0f;
  bool faulted_ = false;
  uint32_t ventStartMs_ = 0;

  float pidIntegral_ = 0.0f;
  float lastMeasured_ = 0.0f;
  float output_ = 0.0f;
  uint32_t lastControlMs_ = 0;
  uint32_t lastDerivativeMs_ = 0;
};

}  // namespace eggplan
