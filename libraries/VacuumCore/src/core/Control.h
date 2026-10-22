#pragma once

#include <stdint.h>

namespace gardener {
namespace cfg {

// --- Closed loop vacuum control ---------------------------------------------
constexpr uint32_t CONTROL_PERIOD_MS = 50;
// Shallowest target the loop will accept, in kPa below the ambient reference, and inclusive: the
// wire publishes it as the `min` of `vacuum target`, so a pull of exactly this much is accepted and
// anything gentler is refused rather than regulated.
constexpr float CONTROL_MIN_TARGET_KPA = 5.0f;
constexpr float PID_KP = 3.0f;    // % duty per kPa of error
constexpr float PID_KI = 0.2f;    // % duty per kPa second
constexpr float PID_KD = 0.5f;    // % duty per kPa/s of pressure slew
constexpr float PID_INTEGRAL_LIMIT = 150.0f;
constexpr float CONTROL_DEADBAND_KPA = 0.2f;

// --- Pump limits the control loop must respect -------------------------------
// The only protections the pump has: the controller owns when a run ends, so nothing here
// stops one.
constexpr float PUMP_DUTY_MAX_PERCENT = 90.0f;
constexpr float PUMP_RAMP_PERCENT_PER_SEC = 150.0f;
// The floor `status` reports: the deepest target worth offering. About 60 kPa of suction below
// ambient is the pump's measured reach, and targets are relative to the same ambient, so this is
// also the deepest one it can hold. A constant because both bounds are.
constexpr float CONTROL_FLOOR_KPA = 60.0f;

// --- Vent valve --------------------------------------------------------------
// Timing only; which part is fitted, and therefore which coil level means "open", is the
// composition root's call in Machine.cpp — normally open is the fail-safe default.
constexpr float VENT_MARGIN_KPA = 2.0f;      // close enough to ambient to be released
constexpr uint32_t VENT_TIMEOUT_MS = 30000;  // stop waiting on a leaky rig
// A target set shallower than the chamber while pulling or holding is reached by opening the
// valve, not by waiting for the leak. This chooses where the valve closes. True stops it a lead
// above the target, so the travel after the coil drops covers the rest; false leaves it open
// until the reading passes the target and lets the pump pull back. Try both on the bench.
constexpr bool CONTROL_VENT_TO_TARGET = true;
constexpr float VENT_TARGET_LEAD_KPA = 1.0f;  // how far above the target the valve closes
// A reset releases the chamber first, but it does not wait out the whole vent: the restart is safe
// once the pump is off and the valve is open, and this is the margin a near-vented chamber gets.
constexpr uint32_t REBOOT_VENT_WAIT_MS = 2000;

}  // namespace cfg
}  // namespace gardener
