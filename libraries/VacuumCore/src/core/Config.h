#pragma once

#include <stdint.h>

// Policy tunables: what the machine does, not how it is wired (see hal/Pins.h).
//
// The device is a serial peripheral: every decision comes from the controller, so what
// lives here is only what the firmware must own to be fast or safe.

namespace gardener {
namespace cfg {

constexpr const char* FIRMWARE_VERSION = "0.6.0";
// The wire revision, reported by `device get`. It moves when a command's meaning moves, not when
// one is added: proto 2 is the resource-first surface, and a controller that does not know a
// revision should stop rather than guess.
constexpr uint32_t PROTOCOL_VERSION = 2;

// --- Closed loop vacuum control -------------------------------------------
constexpr uint32_t CONTROL_PERIOD_MS = 50;
// Shallowest target the loop will accept, in kPa below the ambient reference, and inclusive: the
// wire publishes it as the `min` of `plant target`, so a pull of exactly this much is accepted and
// anything gentler is refused rather than regulated.
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
// Measured capability: about 60 kPa of suction below ambient. Targets are relative to the
// captured ambient too, so this is also the deepest one the pump can reach — the floor.
constexpr float PUMP_MAX_SUCTION_KPA = 60.0f;
// The floor `status` reports: the deepest target worth offering, which is the pump's reach
// unless the loop refuses targets that deep anyway. A constant, because both bounds are.
constexpr float CONTROL_FLOOR_KPA =
    PUMP_MAX_SUCTION_KPA > CONTROL_MIN_TARGET_KPA ? PUMP_MAX_SUCTION_KPA : CONTROL_MIN_TARGET_KPA;

// --- Vent valve ------------------------------------------------------------
// Timing only; which part is fitted, and therefore which coil level means "open", is the
// composition root's call in app/App.h — normally open is the fail-safe default.
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
// A command line is a resource and an op. Anything longer is refused rather than truncated.
constexpr uint16_t COMMAND_LINE_MAX = 128;
// Where the device looks for the end of a command. A host can point it elsewhere with
// `link term`, which turns off newline framing so a whole payload — a program's source — arrives
// as one command instead of in chunks. That payload is far longer than a command line, so a
// custom terminator raises the ceiling to COMMAND_PAYLOAD_MAX, which is also what bounds the
// receive buffer when a host never sends its terminator.
constexpr const char* LINE_TERMINATOR = "\n";
constexpr uint16_t TERMINATOR_MAX = 16;      // longest a host may set it to
constexpr uint32_t COMMAND_PAYLOAD_MAX = 64u * 1024u;
// With USB CDC the presence of a host is observable and its loss stops the run. On a plain
// UART it is not, so a lost link is simply never reported.

// --- Pressure --------------------------------------------------------------
constexpr float DEFAULT_AMBIENT_KPA = 101.325f;  // one standard atmosphere

}  // namespace cfg
}  // namespace gardener
