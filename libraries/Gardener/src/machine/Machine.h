#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "core/Knob.h"
#include "machine/VacuumSystem.h"

namespace gardener {

class Logger;

// --- Closed loop vacuum control ---------------------------------------------
GARDENER_KNOB_VALID(uint32_t, ControlPeriodMs, GARDENER_CONTROL_PERIOD_MS, 50,
                    GARDENER_CONTROL_PERIOD_MS >= 10 && GARDENER_CONTROL_PERIOD_MS <= 1000,
                    "the control tick is the machine's beat, in milliseconds");
// Shallowest non-zero target the loop will accept, in kPa below the ambient reference, and
// inclusive: the wire publishes it as the `min` of `vacuum target`, so a pull of exactly this much
// is accepted and anything gentler is refused rather than regulated. Zero is not below this bound:
// it is the ambient target, which releases the chamber.
GARDENER_KNOB_VALID(float, ControlMinTargetKpa, GARDENER_CONTROL_MIN_TARGET_KPA, 5.0f,
                    GARDENER_CONTROL_MIN_TARGET_KPA > 0.0f &&
                        GARDENER_CONTROL_MIN_TARGET_KPA <= 100.0f,
                    "the sensor reads to 100 kPa below ambient");

// --- The rig's own numbers ---------------------------------------------------
// What a different pump, chamber or valve changes. These are facts about the build rather than
// preferences a running device holds, so each is a knob: the value here is the fitted rig's,
// measured on the bench, and a build for another rig sets its own by generating the sketch's
// `rig_options.h` (see scripts/arduino.py's `--define`). Neither link carries them, because nothing
// changes one at run time.
//
// The header rather than a define on the command line, because a define there is part of
// arduino-cli's build options: changing one throws away every object in the build path, the
// precompiled archives included. The sketch folder is on the include path of every translation
// unit, so a header in it reaches the knobs that read these numbers and no others.
GARDENER_KNOB_VALID(float, PumpFlowLpm, GARDENER_PUMP_FLOW_LPM, 1.0f, GARDENER_PUMP_FLOW_LPM > 0.0f,
                    "PumpFlowLpm must be positive, in L/min");
GARDENER_KNOB_VALID(float, ChamberVolumeL, GARDENER_CHAMBER_VOLUME_L, 0.05f,
                    GARDENER_CHAMBER_VOLUME_L > 0.0f, "ChamberVolumeL must be positive, in litres");
GARDENER_KNOB_VALID(float, ControlFloorKpa, GARDENER_CONTROL_FLOOR_KPA, 60.0f,
                    GARDENER_CONTROL_FLOOR_KPA >= GARDENER_CONTROL_MIN_TARGET_KPA &&
                        GARDENER_CONTROL_FLOOR_KPA <= 100.0f,
                    "the floor must admit a target and stay inside the sensor's range");
GARDENER_KNOB_VALID(float, PumpDutyMaxPercent, GARDENER_PUMP_DUTY_MAX_PERCENT, 90.0f,
                    GARDENER_PUMP_DUTY_MAX_PERCENT > 0.0f &&
                        GARDENER_PUMP_DUTY_MAX_PERCENT <= 100.0f,
                    "PumpDutyMaxPercent is a percentage of full duty");
GARDENER_KNOB_VALID(float, ControlDeadbandKpa, GARDENER_CONTROL_DEADBAND_KPA, 0.2f,
                    GARDENER_CONTROL_DEADBAND_KPA > 0.0f,
                    "ControlDeadbandKpa must be positive, in kPa");
GARDENER_KNOB_VALID(bool, ValveNormallyOpen, GARDENER_VALVE_NORMALLY_OPEN, true, true,
                    "the fitted solenoid is normally open: de-energised vents");

// --- The deadline on a run --------------------------------------------------
// The longest hold the machine will grant, and the most a start may ask for: the same two hours the
// device's own maximum can be set to. Nothing else ends a run on the controller's behalf: a link
// that keeps its port open but stops talking is released.
GARDENER_KNOB_VALID(uint32_t, ControlMaxRunMs, GARDENER_CONTROL_MAX_RUN_MS, 7200000,
                    GARDENER_CONTROL_MAX_RUN_MS >= 1000 && GARDENER_CONTROL_MAX_RUN_MS <= 86400000,
                    "a run is bounded by a day");

// The hold a start is granted: the device's stored maximum, held to the machine's ceiling, or the
// ceiling itself when the document names none. Every path that starts a run asks this, so the
// document bounds a start from the wire and from a page alike, and a document edited past the
// ceiling cannot promise a run the machine would cut short.
inline uint32_t maxRunSeconds(int32_t storedSeconds) {
  const uint32_t ceiling = ControlMaxRunMs / 1000;
  if (storedSeconds <= 0) return ceiling;
  const uint32_t stored = static_cast<uint32_t>(storedSeconds);
  return stored < ceiling ? stored : ceiling;
}

// --- PID configuration from the pump and the chamber ------------------------
// The loop's shape, rig-independent: the proportional correction time, the derivative's damping
// fraction at full slew, the integrator time and the integral clamp. These are tuned once, not per
// rig; the values reproduce the loop as it was hand-tuned for the reference pump and chamber.
GARDENER_KNOB_VALID(float, PidResponseS, GARDENER_PID_RESPONSE_S, 0.1f,
                    GARDENER_PID_RESPONSE_S > 0.0f,
                    "a response time of zero divides the proportional gain");
GARDENER_KNOB_VALID(float, PidDampFraction, GARDENER_PID_DAMP_FRACTION, 0.17f,
                    GARDENER_PID_DAMP_FRACTION >= 0.0f && GARDENER_PID_DAMP_FRACTION <= 1.0f,
                    "the damping fraction is of full slew");
GARDENER_KNOB_VALID(float, PidIntegralTimeS, GARDENER_PID_INTEGRAL_TIME_S, 150.0f,
                    GARDENER_PID_INTEGRAL_TIME_S > 0.0f,
                    "an integrator time of zero divides the integral gain");
GARDENER_KNOB_VALID(float, PidIntegralLimitPercent, GARDENER_PID_INTEGRAL_LIMIT_PERCENT, 150.0f,
                    GARDENER_PID_INTEGRAL_LIMIT_PERCENT > 0.0f,
                    "the integral clamp is a percentage of duty");

// The four gains the loop runs with, in the units `VacuumSystem::pidUpdate` consumes.
struct PidGains {
  float kp;
  float ki;
  float kd;
  float integralLimit;
};

// The one function of the two numbers: the rate the depth rises at full duty near ambient, in
// kPa/s, is the pump's free flow times ambient pressure over the volume, which is the same slew
// the derivative term measures. Each gain scales against it, so the loop keeps its shape
// whatever pump and chamber are fitted.
inline PidGains pidGainsFor(float flowLpm, float volumeL) {
  const float slewKpaPerSec = 101.325f * flowLpm / (60.0f * volumeL);
  PidGains gains;
  gains.kp = 100.0f / (PidResponseS * slewKpaPerSec);
  gains.ki = gains.kp / PidIntegralTimeS;
  gains.kd = PidDampFraction * 100.0f / slewKpaPerSec;
  gains.integralLimit = PidIntegralLimitPercent;
  return gains;
}

// --- Pump limits the control loop must respect -------------------------------
// The duty cap (`PumpDutyMaxPercent`, a rig number) keeps the pump inside its own rating; a run
// is ended by its deadline or by a fault, and both are the vacuum system's.
GARDENER_KNOB_VALID(float, PumpRampPercentPerSec, GARDENER_PUMP_RAMP_PERCENT_PER_SEC, 150.0f,
                    GARDENER_PUMP_RAMP_PERCENT_PER_SEC > 0.0f,
                    "the ramp is a percentage of duty per second");

// --- Vent valve --------------------------------------------------------------
// Timing only; which part is fitted, and therefore which coil level means "open", is the
// composition root's call in Machine.cpp from the ValveNormallyOpen knob — normally open
// is the fail-safe default.
GARDENER_KNOB_VALID(float, VentMarginKpa, GARDENER_VENT_MARGIN_KPA, 2.0f,
                    GARDENER_VENT_MARGIN_KPA >= 0.0f,
                    "close enough to the vent floor to be released, in kPa");
// The depth this rig settles at with the valve open, measured on the bench. A chamber with the
// valve open does not reach ambient: the tube, the orifice and the sensor's offset leave it a
// little above zero, so the release criterion is this floor rather than 0.
GARDENER_KNOB_VALID(float, VentFloorKpa, GARDENER_VENT_FLOOR_KPA, 1.0f,
                    GARDENER_VENT_FLOOR_KPA >= 0.0f && GARDENER_VENT_FLOOR_KPA < 100.0f,
                    "the vent floor is a depth below ambient, in kPa");
GARDENER_KNOB_VALID(uint32_t, VentTimeoutMs, GARDENER_VENT_TIMEOUT_MS, 30000,
                    GARDENER_VENT_TIMEOUT_MS > 0,
                    "a release that misses the floor inside this faults");
// A target set shallower than the chamber while pulling or holding is reached by opening the
// valve, not by waiting for the leak. This chooses where the valve closes. True closes it a lead
// deeper than the target, so the travel after the coil drops covers the rest; false leaves it
// open until the reading passes the target and lets the pump pull back. Try both on the bench.
GARDENER_KNOB_VALID(bool, ControlVentToTarget, GARDENER_CONTROL_VENT_TO_TARGET, true, true,
                    "reach a shallower target by venting, or let the pump pull back");
GARDENER_KNOB_VALID(float, VentTargetLeadKpa, GARDENER_VENT_TARGET_LEAD_KPA, 1.0f,
                    GARDENER_VENT_TARGET_LEAD_KPA >= 0.0f,
                    "how far deeper than the target the valve closes, in kPa");
// A reset releases the chamber first, but it does not wait out the whole vent: the restart is safe
// once the pump is off and the valve is open, and this is the margin a near-vented chamber gets.
GARDENER_KNOB_VALID(uint32_t, RebootVentWaitMs, GARDENER_REBOOT_VENT_WAIT_MS, 2000,
                    GARDENER_REBOOT_VENT_WAIT_MS >= 0,
                    "the wait before a reset restarts the machine, in milliseconds");

// --- The loop's own faults ---------------------------------------------------
// A pull driven hard against a blocked hose or a stuck sensor moves nothing, and a depth that runs
// past its target is the loop not holding what it was asked to. Both are the machine's to notice,
// because a controller that has stopped talking is exactly when they matter.
GARDENER_KNOB_VALID(float, ControlProgressKpa, GARDENER_CONTROL_PROGRESS_KPA, 1.0f,
                    GARDENER_CONTROL_PROGRESS_KPA > 0.0f,
                    "headway that counts as a pull making progress, in kPa");
GARDENER_KNOB_VALID(uint32_t, ControlProgressTimeoutMs, GARDENER_CONTROL_PROGRESS_TIMEOUT_MS, 10000,
                    GARDENER_CONTROL_PROGRESS_TIMEOUT_MS > 0,
                    "how long a pull may make no progress before it faults");
GARDENER_KNOB_VALID(float, ControlProgressDutyFraction, GARDENER_CONTROL_PROGRESS_DUTY_FRACTION,
                    0.5f,
                    GARDENER_CONTROL_PROGRESS_DUTY_FRACTION > 0.0f &&
                        GARDENER_CONTROL_PROGRESS_DUTY_FRACTION <= 1.0f,
                    "the fraction of the duty cap that counts as driving a pull");
GARDENER_KNOB_VALID(float, ControlOvershootKpa, GARDENER_CONTROL_OVERSHOOT_KPA, 5.0f,
                    GARDENER_CONTROL_OVERSHOOT_KPA > 0.0f,
                    "deeper than the target is past it, in kPa");
GARDENER_KNOB_VALID(uint8_t, ControlOvershootReadings, GARDENER_CONTROL_OVERSHOOT_READINGS, 5,
                    GARDENER_CONTROL_OVERSHOOT_READINGS > 0,
                    "how many readings past the target count as an overshoot");

// The pins the machine's parts are wired to. The sketch names them and the composition builds the
// parts from them, so this library carries no board: a different rig is a different set of numbers.
struct MachinePins {
  uint8_t sensorAdc;
  uint8_t pumpPwm;
  uint8_t ventValve;
};

// The optional assemblies a composition root fitted, one byte on both links so a host gets the same
// answer whichever transport it uses. The sketch ORs the bits it built and the machine holds the
// byte. There is no bit for stopping when a link leaves: no build does that.
enum Capability : uint8_t {
  PanelFitted = 0x01,    // a panel (and with it keys, pages and the VM) is fitted
  BatteryFitted = 0x02,  // a battery gauge is fitted; a wired-only device has none
  // The fitted valve fails safe: de-energised means vented, so the chamber cannot hold pressure
  // through a crash, a cut wire or a dead coil. A normally-closed part inverts that.
  FailSafeValve = 0x04,
};

// The machine: the pressure loop and the policy around it, with no panel part named. It commands
// the three parts it is handed and holds the capability byte, and it reports through the logger it
// was built with. boot() brings the parts up; the parts themselves, the links and the tasks stay the
// composition's.
//
// It is not copyable: there is one machine, and everything that needs it holds a reference.
class Machine {
 public:
  Machine(Logger& logger, Pump* pump, PressureSensor* sensor, VentValve* valve);

  Machine(const Machine&) = delete;
  Machine& operator=(const Machine&) = delete;

  // The parts come up and say whether they answered. The links and the tasks are the
  // composition's, so this is the whole of what the machine starts.
  void boot();

  VacuumSystem& vacuum() { return vacuum_; }
  const VacuumSystem& vacuum() const { return vacuum_; }
  uint8_t capabilities() const { return capabilities_; }
  void setCapabilities(uint8_t capabilities) { capabilities_ = capabilities; }
  Logger& logger() { return logger_; }
  uint32_t uptimeMs() const { return millis(); }

 private:
  Logger& logger_;
  VacuumSystem vacuum_;
  uint8_t capabilities_ = 0;
};

}  // namespace gardener