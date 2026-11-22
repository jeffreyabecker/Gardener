#pragma once

#include <stdint.h>

#include "core/Knob.h"
#include "machine/VacuumSystem.h"

namespace gardener {

class Logger;

// --- Closed loop vacuum control ---------------------------------------------
GARDENER_KNOB_VALID(uint32_t, ControlPeriodMs, GARDENER_CONTROL_PERIOD_MS, 50,
                    GARDENER_CONTROL_PERIOD_MS >= 10 && GARDENER_CONTROL_PERIOD_MS <= 1000,
                    "the control tick is the machine's beat, in milliseconds");

// --- The hold band -----------------------------------------------------------
// The pump is a switch, so the loop holds a band rather than a setpoint: the pump runs while the
// chamber is shallower than the target by the lower number, stops once the reading is back inside,
// and the valve opens when the chamber is deeper than the target by the upper one. Both are
// percentages of the target, so 5 % either side of 20 kPa holds between 19 and 21.
GARDENER_KNOB_VALID(float, ControlBelowPercent, GARDENER_CONTROL_BELOW_PERCENT, 5.0f,
                    GARDENER_CONTROL_BELOW_PERCENT > 0.0f &&
                        GARDENER_CONTROL_BELOW_PERCENT < 100.0f,
                    "how far under the target the pump switches on, in percent of it");
GARDENER_KNOB_VALID(float, ControlAbovePercent, GARDENER_CONTROL_ABOVE_PERCENT, 5.0f,
                    GARDENER_CONTROL_ABOVE_PERCENT > 0.0f,
                    "how far past the target the valve opens, in percent of it");

// --- The build's options -----------------------------------------------------
// What a different pump, chamber or valve changes. These are facts about the build rather than
// preferences a running device holds, so each is a knob: the value here is the fitted build's,
// measured on the bench, and another build sets its own by generating the sketch's `options.h`
// (see scripts/arduino.py's `--define`). Neither link carries them, because nothing changes one at
// run time.
//
// The header rather than a define on the command line, because a define there is part of
// arduino-cli's build options: changing one throws away every object in the build path, the
// precompiled archives included. The sketch folder is on the include path of every translation
// unit, so a header in it reaches the knobs that read these numbers and no others.
GARDENER_KNOB_VALID(float, ControlFloorKpa, GARDENER_CONTROL_FLOOR_KPA, 60.0f,
                    GARDENER_CONTROL_FLOOR_KPA > 0.0f && GARDENER_CONTROL_FLOOR_KPA <= 100.0f,
                    "the floor stays inside the sensor's range");
// The part fitted to this build is normally closed: the coil has to be energised to vent, and a
// power loss seals the chamber instead of releasing it. Building with this false is what inverts
// that, and it is the one place the fitted valve is named.
GARDENER_KNOB_VALID(bool, ValveNormallyOpen, GARDENER_VALVE_NORMALLY_OPEN, false, true,
                    "true only for a normally open solenoid: de-energised vents");

// --- The deadline on a run --------------------------------------------------
// The longest hold the machine will grant, and the most a start may ask for: the same two hours the
// device's own maximum can be set to. Nothing else ends a run on the controller's behalf: a link
// that keeps its port open but stops talking is released.
GARDENER_KNOB_VALID(uint32_t, ControlMaxRunMs, GARDENER_CONTROL_MAX_RUN_MS, 7200000,
                    GARDENER_CONTROL_MAX_RUN_MS >= 1000 && GARDENER_CONTROL_MAX_RUN_MS <= 86400000,
                    "a run is bounded by a day");

// --- Vent valve --------------------------------------------------------------
// Timing only; which part is fitted, and therefore which coil level means "open", is the
// composition root's call in Machine.cpp from the ValveNormallyOpen knob.
GARDENER_KNOB_VALID(float, VentMarginKpa, GARDENER_VENT_MARGIN_KPA, 2.0f,
                    GARDENER_VENT_MARGIN_KPA >= 0.0f,
                    "close enough to the vent floor to be released, in kPa");
// The depth this build settles at with the valve open, measured on the bench. A chamber with the
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
// The loop's own faults ---------------------------------------------------
// A pull driven hard against a blocked hose or a stuck sensor moves nothing, which is the pump's to
// notice. A depth past its target the loop vents back from, so the overshoot is a fault only for a
// run that keeps needing that: a reading that will not come back is the one thing venting cannot
// fix. Both are the machine's to notice, because a controller that has stopped talking is exactly
// when they matter.
GARDENER_KNOB_VALID(float, ControlProgressKpa, GARDENER_CONTROL_PROGRESS_KPA, 1.0f,
                    GARDENER_CONTROL_PROGRESS_KPA > 0.0f,
                    "headway that counts as a pull making progress, in kPa");
GARDENER_KNOB_VALID(uint32_t, ControlProgressTimeoutMs, GARDENER_CONTROL_PROGRESS_TIMEOUT_MS, 10000,
                    GARDENER_CONTROL_PROGRESS_TIMEOUT_MS > 0,
                    "how long a pull may make no progress before it faults");
GARDENER_KNOB_VALID(uint8_t, ControlOvershootReadings, GARDENER_CONTROL_OVERSHOOT_READINGS, 5,
                    GARDENER_CONTROL_OVERSHOOT_READINGS > 0,
                    "how many readings past the band count as an overshoot");
GARDENER_KNOB_VALID(uint8_t, ControlOvershootRecoveries, GARDENER_CONTROL_OVERSHOOT_RECOVERIES, 3,
                    GARDENER_CONTROL_OVERSHOOT_RECOVERIES > 0,
                    "how many vent-back recoveries a run allows before an overshoot faults");

// The pins the machine's parts are wired to. The sketch names them and the composition builds the
// parts from them, so this library carries no board: another wiring is another set of pins.
struct MachinePins {
  uint8_t sensorAdc;
  uint8_t pump;
  uint8_t ventValve;
};

// The machine: the pressure loop and the policy around it. It commands the three parts it is
// handed and reports through the logger it was built with. boot() brings the parts up; the parts
// themselves, the link and the tasks stay the composition's.
//
// It is not copyable: there is one machine, and everything that needs it holds a reference.
class Machine {
 public:
  Machine(Logger& logger, Pump* pump, PressureSensor* sensor, VentValve* valve);

  Machine(const Machine&) = delete;
  Machine& operator=(const Machine&) = delete;

  // The parts come up and say whether they answered. The link and the tasks are the composition's,
  // so this is the whole of what the machine starts.
  void boot();

  VacuumSystem& vacuum() { return vacuum_; }
  Logger& logger() { return logger_; }

 private:
  Logger& logger_;
  VacuumSystem vacuum_;
};

}  // namespace gardener