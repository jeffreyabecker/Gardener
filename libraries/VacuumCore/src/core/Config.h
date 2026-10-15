#pragma once

#include <stdint.h>

// Policy tunables: what the machine does, not how it is wired (see hal/Pins.h).
//
// The device is a BLE peripheral: every decision comes from the central, so what lives
// here is only what the firmware must own to be fast or safe.

namespace eggplan {
namespace cfg {

constexpr const char* FIRMWARE_VERSION = "0.1.0";

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
constexpr float PUMP_DUTY_MAX_PERCENT = 90.0f;
constexpr float PUMP_RAMP_PERCENT_PER_SEC = 150.0f;
constexpr uint32_t PUMP_MAX_RUN_MS = 15UL * 60UL * 1000UL;
// Measured capability: about 60 kPa of suction below ambient, so the reachable
// absolute floor is ambient dependent, not a fixed number.
constexpr float PUMP_MAX_SUCTION_KPA = 60.0f;

// --- Pressure --------------------------------------------------------------
constexpr float DEFAULT_AMBIENT_KPA = 101.325f;  // one standard atmosphere

// --- Bluetooth -------------------------------------------------------------
constexpr const char* BLE_NAME_PREFIX = "EggplanVac";
constexpr uint16_t BLE_MTU = 517;

// --- Telemetry -------------------------------------------------------------
constexpr uint32_t TELEMETRY_DEFAULT_MS = 250;
constexpr uint32_t TELEMETRY_MIN_MS = 50;

}  // namespace cfg
}  // namespace eggplan
