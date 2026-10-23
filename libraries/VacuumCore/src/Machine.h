#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "Ownership.h"
#include "VacuumSystem.h"

namespace gardener {

// --- Closed loop vacuum control ---------------------------------------------
constexpr uint32_t CONTROL_PERIOD_MS = 50;
// Shallowest non-zero target the loop will accept, in kPa below the ambient reference, and
// inclusive: the wire publishes it as the `min` of `vacuum target`, so a pull of exactly this much
// is accepted and anything gentler is refused rather than regulated. Zero is not below this bound:
// it is the ambient target, which releases the chamber.
constexpr float CONTROL_MIN_TARGET_KPA = 5.0f;
constexpr float CONTROL_DEADBAND_KPA = 0.2f;

// The bounds a loop input may hold, one definition for the loader, the setters and both links'
// refusals, so a stored value and a written one are judged by the same numbers. `pump_max` is
// bounded above by PUMP_DUTY_MAX_PERCENT, which is also the pump driver's ceiling.
constexpr float PUMP_FLOW_MIN_LPM = 0.05f;
constexpr float PUMP_FLOW_MAX_LPM = 10.0f;
constexpr float CHAMBER_VOLUME_MIN_L = 0.005f;
constexpr float CHAMBER_VOLUME_MAX_L = 5.0f;
constexpr float CONTROL_DEADBAND_MIN_KPA = 0.05f;
constexpr float CONTROL_DEADBAND_MAX_KPA = 2.0f;
constexpr float CONTROL_FLOOR_MAX_KPA = 100.0f;

// --- The deadline on a run --------------------------------------------------
// The longest hold the device will grant, and the most a start may ask for. Nothing else ends a
// run on the controller's behalf: a link that keeps its port open but stops talking is released.
constexpr uint32_t CONTROL_MAX_RUN_MS = 900000;

// --- PID configuration from the pump and the chamber ------------------------
// The loop's gains are derived from two settings — the pump's projected free-flow rate and the
// chamber volume — so a different pump or chamber retunes the loop by changing two numbers, not
// three gains. The defaults are the fitted rig's, set on the bench; the wire overrides them with
// `device set flow` and `device set volume`.
constexpr float PUMP_FLOW_LPM = 1.0f;     // the fitted pump's free flow, in L/min
constexpr float CHAMBER_VOLUME_L = 0.05f;  // the fitted chamber's volume, in litres

// The loop's shape, rig-independent: the proportional correction time, the derivative's damping
// fraction at full slew, the integrator time and the integral clamp. These are tuned once, not per
// rig; the values reproduce the loop as it was hand-tuned for the reference pump and chamber.
constexpr float PID_RESPONSE_S = 0.1f;        // seconds to correct an error proportionally
constexpr float PID_DAMP_FRACTION = 0.17f;    // duty the derivative takes off at full slew
constexpr float PID_INTEGRAL_TIME_S = 150.0f; // integrator time constant; slow, so the saturated
                                              // pull does not wind the integral
constexpr float PID_INTEGRAL_LIMIT_PERCENT = 150.0f;

// The four gains the loop runs with, in the units `VacuumSystem::pidUpdate` consumes.
struct PidGains {
  float kp;
  float ki;
  float kd;
  float integralLimit;
};

// The one function of the two settings: the rate the depth rises at full duty near ambient, in
// kPa/s, is the pump's free flow times ambient pressure over the volume, which is the same slew
// the derivative term measures. Each gain scales against it, so the loop keeps its shape
// whatever pump and chamber are fitted.
inline PidGains pidGainsFor(float flowLpm, float volumeL) {
  const float slewKpaPerSec = 101.325f * flowLpm / (60.0f * volumeL);
  PidGains gains;
  gains.kp = 100.0f / (PID_RESPONSE_S * slewKpaPerSec);
  gains.ki = gains.kp / PID_INTEGRAL_TIME_S;
  gains.kd = PID_DAMP_FRACTION * 100.0f / slewKpaPerSec;
  gains.integralLimit = PID_INTEGRAL_LIMIT_PERCENT;
  return gains;
}

// --- Pump limits the control loop must respect -------------------------------
// The duty cap keeps the pump inside its own rating; a run is ended by its deadline or by a fault,
// and both are the vacuum system's.
constexpr float PUMP_DUTY_MAX_PERCENT = 90.0f;
constexpr float PUMP_RAMP_PERCENT_PER_SEC = 150.0f;
// The floor `status` reports: the deepest target worth offering. About 60 kPa of suction below
// ambient is the pump's measured reach, and targets are relative to the same ambient, so this is
// also the deepest one it can hold. A constant because both bounds are.
constexpr float CONTROL_FLOOR_KPA = 60.0f;

// --- Vent valve --------------------------------------------------------------
// Timing only; which part is fitted, and therefore which coil level means "open", is the
// composition root's call in Machine.cpp — normally open is the fail-safe default.
constexpr float VENT_MARGIN_KPA = 2.0f;      // close enough to the vent floor to be released
// The depth this rig settles at with the valve open, measured on the bench. A chamber with the
// valve open does not reach ambient: the tube, the orifice and the sensor's offset leave it a
// little above zero, so the release criterion is this floor rather than 0.
constexpr float VENT_FLOOR_KPA = 1.0f;
constexpr uint32_t VENT_TIMEOUT_MS = 30000;  // a release that misses the floor inside this faults
// A target set shallower than the chamber while pulling or holding is reached by opening the
// valve, not by waiting for the leak. This chooses where the valve closes. True closes it a lead
// deeper than the target, so the travel after the coil drops covers the rest; false leaves it
// open until the reading passes the target and lets the pump pull back. Try both on the bench.
constexpr bool CONTROL_VENT_TO_TARGET = true;
constexpr float VENT_TARGET_LEAD_KPA = 1.0f;  // how far deeper than the target it closes
// A reset releases the chamber first, but it does not wait out the whole vent: the restart is safe
// once the pump is off and the valve is open, and this is the margin a near-vented chamber gets.
constexpr uint32_t REBOOT_VENT_WAIT_MS = 2000;

// --- The loop's own faults ---------------------------------------------------
// A pull driven hard against a blocked hose or a stuck sensor moves nothing, and a depth that runs
// past its target is the loop not holding what it was asked to. Both are the machine's to notice,
// because a controller that has stopped talking is exactly when they matter.
constexpr float CONTROL_PROGRESS_KPA = 1.0f;      // headway that counts as a pull making progress
constexpr uint32_t CONTROL_PROGRESS_TIMEOUT_MS = 10000;
constexpr float CONTROL_PROGRESS_DUTY_FRACTION = 0.5f;  // of the duty cap
constexpr float CONTROL_OVERSHOOT_KPA = 5.0f;     // deeper than the target, and so past it
constexpr uint8_t CONTROL_OVERSHOOT_READINGS = 5;

// The pins the machine's parts are wired to. The sketch names them, so this library carries no
// board: a different rig is a different set of numbers in the sketch.
struct MachinePins {
  uint8_t sensorAdc;
  uint8_t pumpPwm;
  uint8_t ventValve;
};

// The optional assemblies a composition root fitted, one byte on both links so a host gets the same
// answer whichever transport it uses. The sketch ORs the bits it built and the machine holds the
// byte. There is no bit for stopping when the owner leaves: every build does that.
enum Capability : uint8_t {
  PanelFitted = 0x01,    // a panel (and with it keys, pages and the VM) is fitted
  BatteryFitted = 0x02,  // a battery gauge is fitted; a wired-only device has none
  // The fitted valve fails safe: de-energised means vented, so the chamber cannot hold pressure
  // through a crash, a cut wire or a dead coil. A normally-closed part inverts that.
  FailSafeValve = 0x04,
};

class Logger;
class MachineSettings;

// The machine: the pressure loop and the policy around it, with no panel part named. It owns the
// parts, the ownership stack, the capability byte and the device settings, and it reports through
// the logger it was built with. boot() brings the parts up; the links and the tasks stay the
// composition's.
//
// It is not copyable: there is one machine, and everything that needs it holds a reference.
class Machine {
 public:
  Machine(Logger& logger, const MachinePins& pins, MachineSettings& settings);

  Machine(const Machine&) = delete;
  Machine& operator=(const Machine&) = delete;

  // The parts come up and say whether they answered. The links and the tasks are the
  // composition's, so a machine-alone build calls this alone.
  void boot();

  VacuumSystem& vacuum() { return vacuum_; }
  Ownership& ownership() { return ownership_; }
  uint8_t capabilities() const { return capabilities_; }
  void setCapabilities(uint8_t capabilities) { capabilities_ = capabilities; }
  Logger& logger() { return logger_; }
  MachineSettings& settings() { return settings_; }
  uint32_t uptimeMs() const { return millis(); }

 private:
  Logger& logger_;
  MachineSettings& settings_;
  VacuumSystem vacuum_;
  Ownership ownership_;
  uint8_t capabilities_ = 0;
};

}  // namespace gardener
