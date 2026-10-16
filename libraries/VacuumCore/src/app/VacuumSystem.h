#pragma once

#include <stdint.h>

#include "core/Config.h"
#include "core/Snapshot.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/VentValve.h"

namespace gardener {

// The plant: the pressure loop, the release path and the run state, between the devices and the
// wire. The devices are not its own — it is handed the three it commands, so the pins and the bus
// stay in hal/ and this file stays the control law.
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
// Pressures here are relative to the ambient the sensor captured, in kPa below it: a target is
// a depth to pull to, and currentKpa() is how deep the chamber is now. Nothing dials that
// reference in — capturing is the only way to take one — so it is whatever the rig was sitting
// at when it was taken, and it cancels out of every comparison against a target.
//
// The loop is a plain PID on that relative pressure with the pump as the only actuator, so it
// can pull down but never vent; the valve is the only way back up, which is why every release
// path here stops the pump before it opens the valve. Gains live in core/Config.h.
class VacuumSystem {
 public:
  VacuumSystem(Pump& pump, PressureSensor& sensor, VentValve& vent)
      : pump_(pump), sensor_(sensor), vent_(vent) {}

  void begin();                  // pump off, valve open: the safe resting state
  void service(uint32_t nowMs);  // the state machine, the PID step and the fault detection

  // --- commands -----------------------------------------------------------
  void setTarget(float kpa);  // kPa below the captured ambient
  // A run without a target is not a run, so clearing one releases the chamber.
  void clearTarget();
  bool start();                   // false: no target/reading, or faulted
  // Which of the three refusals start() would answer with, or null when it would start. The wire
  // reports this, so the reply and the refusal come from one place.
  const char* startRefusal() const;
  void stop();                    // the one release path: stop + vent
  void resetFault();
  void captureAmbient();          // the only way a reference is ever set

  // --- the reads ----------------------------------------------------------
  // Depth below the captured ambient: the frame the target is stated in, and the sensor's own
  // deficit, so it is the same number `plant get` reports as `vac`. Zero while a reading is invalid.
  float currentKpa() const;
  PlantState state() const;

 private:
  void setState(RunState state);

  // The chamber's own answers, rather than queries for anyone outside: these are the two
  // transitions that are facts about the chamber instead of orders, and service() is where
  // they are asked.
  bool atTarget() const;  // close enough to the target, deadband included
  bool vented() const;    // settled within VENT_MARGIN_KPA, or VENT_TIMEOUT_MS elapsed
  void vent();            // stop, then open: the one release path
  void rest();            // the resting state without releasing: pump off, valve open

  void regulate(uint32_t nowMs);
  void fail(const char* code);
  // The latch is the fault code: empty means nothing is latched, so there is no second flag to
  // keep in step with it, and nothing but resetFault() clears it.
  bool faulted() const { return faultCode_[0] != '\0'; }

  void pidReset() { pidIntegral_ = 0.0f; }
  float pidUpdate(float dt, float error, float measurementSlew);

  Pump& pump_;
  PressureSensor& sensor_;
  VentValve& vent_;

  RunState state_ = RunState::Idle;
  float targetKpa_ = 0.0f;  // 0 while no target is set, which is what state() reports
  const char* faultCode_ = "";
  uint32_t ventStartMs_ = 0;

  float pidIntegral_ = 0.0f;
  float lastMeasured_ = 0.0f;
  float output_ = 0.0f;
  uint32_t lastControlMs_ = 0;
  uint32_t lastDerivativeMs_ = 0;
};

}  // namespace gardener
