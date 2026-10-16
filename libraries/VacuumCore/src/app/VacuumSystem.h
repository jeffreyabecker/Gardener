#pragma once

#include <Adafruit_MPRLS.h>
#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>

#include "core/Config.h"
#include "core/Snapshot.h"
#include "hal/Pins.h"

namespace gardener {

// The plant: the whole rig between the wire and the hardware — the sensor, the pump and the
// valve, and the pressure loop, the release path and the run state on top of them.
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
//
// The sensor, the pump and the valve are the plant's own rather than injected: each has exactly
// one user, this class, so folding them in keeps the pins, the bus traffic and the loop that
// depends on them in one file. The panel and the battery stay in hal/, where more than one
// app/ file needs them.
class VacuumSystem {
 public:
  // Brings up the sensor, the pump and the valve, then captures the ambient reference. False
  // when the MPRLS did not answer: the plant still comes up, and service() will not regulate on
  // a reading nobody trusts.
  bool begin(TwoWire& wire);
  bool pumpReady() const { return pumpReady_; }

  // The sensor poll, the pump ramp, the control step and the state machine.
  void service(uint32_t nowMs);

  // --- commands -----------------------------------------------------------
  void setTarget(float kpa);  // kPa below the captured ambient
  // A run without a target is not a run, so clearing one releases the chamber.
  void clearTarget();
  bool start();                   // false: no target/reading, or faulted
  void stop();                    // the one release path: stop + vent
  void resetFault();
  void captureAmbient();          // the only way a reference is ever set

  // --- the reads ----------------------------------------------------------
  // Depth below the captured ambient: the frame the target is stated in, and the sensor's own
  // deficit, so it is the same number `status` reports as `vac`. Zero while a reading is invalid.
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

  // --- the sensor ---------------------------------------------------------
  bool readNow(float& outKpa);
  void pollSensor(uint32_t nowMs);
  // False when the value is outside what the part can measure; the reference is then left
  // alone, so a capture taken while the chamber was already pumped down is refused rather than
  // adopted. Only captureAmbient() calls it, so the reference can be taken but never dialled in.
  bool setAmbientKpa(float kpa);
  float vacuumPercent() const;

  // --- the pump -----------------------------------------------------------
  // The duty cap and the slew limit live here rather than in the loop: a diaphragm pump dislikes
  // a step from 0 to full, and the cap is the only protection it has.
  void setDuty(float percent);
  void updatePump(uint32_t nowMs);
  void writeDuty(float percent);

  // --- the valve ----------------------------------------------------------
  // Normally open, so de-energised is released, and the drive is active high because the MOSFET
  // gate is. Vent *timing* belongs to the run model, not to the pin.
  void openValve();
  void closeValve();
  void writeValve(bool energise);

  // --- the devices --------------------------------------------------------
  Adafruit_MPRLS sensor_;
  bool sensorPresent_ = false;
  bool readingValid_ = false;
  uint32_t lastPollMs_ = 0;
  float ambientKpa_ = 0.0f;
  float filteredKpa_ = 0.0f;
  float window_[5] = {0, 0, 0, 0, 0};
  uint8_t windowCount_ = 0;
  uint8_t windowIndex_ = 0;

  bool pumpReady_ = false;
  float pumpDuty_ = 0.0f;
  float pumpTarget_ = 0.0f;
  uint32_t lastPumpMs_ = 0;

  bool valveOpen_ = true;  // de-energised is released

  // --- the run ------------------------------------------------------------
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
