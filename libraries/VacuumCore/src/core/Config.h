#pragma once

#include <stdint.h>

// Policy tunables: what the machine does, not how it is wired (see hal/Pins.h).
//
// The device is a serial peripheral: every decision comes from the controller, so what
// lives here is only what the firmware must own to be fast or safe.

namespace eggplan {
namespace cfg {

constexpr const char* FIRMWARE_VERSION = "0.3.0";

// --- Closed loop vacuum control -------------------------------------------
constexpr uint32_t CONTROL_PERIOD_MS = 50;
// Floor for the loop target. Below the pump's ultimate vacuum the duty would simply
// sit at the cap, so this mostly guards against nonsense targets.
constexpr float CONTROL_MIN_TARGET_KPA = 5.0f;
constexpr float PID_KP = 3.0f;    // % duty per kPa of error
constexpr float PID_KI = 0.2f;    // % duty per kPa second
constexpr float PID_KD = 0.5f;    // % duty per kPa/s of pressure slew
constexpr float PID_INTEGRAL_LIMIT = 150.0f;
constexpr float CONTROL_DEADBAND_KPA = 0.2f;

// --- Pump limits the control loop must respect -----------------------------
// The only protections the pump has: the controller owns when a run ends, so nothing here
// stops one.
constexpr float PUMP_DUTY_MAX_PERCENT = 90.0f;
constexpr float PUMP_RAMP_PERCENT_PER_SEC = 150.0f;
// Measured capability: about 60 kPa of suction below ambient, so the reachable
// absolute floor is ambient dependent, not a fixed number.
constexpr float PUMP_MAX_SUCTION_KPA = 60.0f;

// --- Vent valve ------------------------------------------------------------
// The valve is normally open (Q10): energise to close, so venting is the de-energised
// state and a fault is fail-safe by construction.
constexpr float VENT_MARGIN_KPA = 2.0f;      // close enough to ambient to be released
constexpr uint32_t VENT_TIMEOUT_MS = 30000;  // stop waiting on a leaky rig

// --- Battery ---------------------------------------------------------------
// 1S Li-ion (Q6), a straight line between empty and full. BATTERY_CALIBRATION trims the
// divider against a real meter because the ESP32 ADC is not lab grade.
constexpr float BATTERY_EMPTY_VOLTS = 3.2f;
constexpr float BATTERY_FULL_VOLTS = 4.2f;
constexpr float BATTERY_CALIBRATION = 1.0f;
constexpr uint8_t BATTERY_LOW_PERCENT = 20;

// --- Controller link --------------------------------------------------------
// The device is a serial peripheral: every decision comes from the controller, so what
// lives here is only what the firmware must own to be fast or safe.
constexpr uint32_t SERIAL_BAUD = 115200;  // only meaningful on a real UART; USB CDC ignores it
// A command line is verb, key, value. Anything longer is refused rather than truncated.
constexpr uint16_t COMMAND_LINE_MAX = 128;
// With USB CDC the presence of a host is observable and its loss stops the run. On a plain
// UART it is not, so a lost link is simply never reported.

// --- Pressure --------------------------------------------------------------
constexpr float DEFAULT_AMBIENT_KPA = 101.325f;  // one standard atmosphere

}  // namespace cfg
}  // namespace eggplan
