#pragma once

#include <stdint.h>

// Every constant the firmware owns: the board it is wired to, and the policy it runs by.
//
// The board facts are here rather than in hal/ because a pin is one shared truth — the composition
// root that hands it to the driver, the driver that reads it, and the wiring docs all name the same
// rig — and this is the header every layer already includes. That width is the price of it: because
// core/ is visible everywhere, the serial layer can see a GPIO too.
//
// The device is a serial peripheral: every decision comes from the controller, so what
// lives here is only what the firmware must own to be fast, safe or wired.

namespace gardener {
namespace cfg {

constexpr const char* FIRMWARE_VERSION = "0.7.0";
// The wire revision, reported by `device get`. The wire is still unsettled, so the number stays 1
// and does not move per revision; a controller must not treat it as a compatibility guarantee.
constexpr uint32_t PROTOCOL_VERSION = 1;

// --- Board: the pin map and the harness ------------------------------------
// The rig as it is wired. A board variant is a different set of numbers here.
//
// The panel is the only bus part: it is on `Wire1` (bus 1) at two of the free pins, clear of the
// S3 strapping pins (0, 3, 45, 46) and of the USB pins (19/20). The sensor is not on a bus at all;
// its amplified analog output goes to an ADC pin.
constexpr int PANEL_I2C_SDA = 17;
constexpr int PANEL_I2C_SCL = 18;
// The bus speed belongs to the harness rather than to the part on it.
constexpr uint32_t PANEL_I2C_FREQ_HZ = 400000;

// The pressure sensor: a CFSensor XGZP6847A100KPGN33, whose output is 0.2..2.7 V over -100..0 kPa.
// GPIO7 is ADC1 channel 6 and clear of the buttons (GPIO1/2/10), the battery sense (GPIO4) and the
// strapping pins, so it is the one ADC1 pin left for the part.
constexpr int SENSOR_ADC = 7;

// The pump: a JQB2438 through a low side MOSFET.
constexpr int PUMP_PWM = 6;

// The vent valve: a solenoid through a logic level MOSFET with a flyback diode across the coil,
// wired like the pump. Which coil level means "open" is chosen in app/Build.cpp by the valve class it
// constructs; normally open is the fail-safe default.
constexpr int VENT_VALVE = 5;

// The piezo: a passive element on a spare GPIO, driven by one LEDC channel. GPIO12 is the fifth
// key's pin on the POC 5-way module; the firmware reads only four keys, and the buzzer takes the
// pin. A board that wants that key back has to move the buzzer.
constexpr int BUZZER_PWM = 12;

// Battery sense: a 1S Li-ion pack through a 100k/100k divider, so the top of the pack lands inside
// the ADC range. GPIO4 is ADC1 channel 3, and it is the ADC1 pin that is neither a button nor the
// sensor: ADC1 covers GPIO1..GPIO10, GPIO1/2/10 are button pins, GPIO7 is the sensor, and a pressed
// button would yank the sense line to ground. The S3 strapping pins (0, 3, 45, 46) stay clear too.
constexpr int BATTERY_ADC = 4;
// The board's divider, not the pack's property: two equal resistors, so the ratio is 2.0 and
// BATTERY_CALIBRATION trims the pair against a meter.
constexpr float BATTERY_DIVIDER_RATIO = 2.0f;

// Four button UI, active low to GND with pull ups on the module. hal/impl/Buttons.h arms them on
// change interrupts and settles the debounce on each read; Back held for 3 s is the one key with an
// action, and App owns that rule.
// They sit away from the S3 strapping pins (0, 3, 45, 46) and from the USB pins (19/20). GPIO10/11
// are the variant's default SS/MOSI, which costs nothing here: this firmware never starts SPI.
constexpr int BTN_UP = 1;
constexpr int BTN_DOWN = 2;
constexpr int BTN_OK = 10;
constexpr int BTN_BACK = 11;

// --- Closed loop vacuum control -------------------------------------------
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

// --- Pump limits the control loop must respect -----------------------------
// The only protections the pump has: the controller owns when a run ends, so nothing here
// stops one.
constexpr float PUMP_DUTY_MAX_PERCENT = 90.0f;
constexpr float PUMP_RAMP_PERCENT_PER_SEC = 150.0f;
// Measured capability: about 60 kPa of suction below ambient. Targets are relative to the
// same ambient, so this is also the deepest one the pump can reach — the floor.
constexpr float PUMP_MAX_SUCTION_KPA = 60.0f;
// The floor `status` reports: the deepest target worth offering, which is the pump's reach
// unless the loop refuses targets that deep anyway. A constant, because both bounds are.
constexpr float CONTROL_FLOOR_KPA =
    PUMP_MAX_SUCTION_KPA > CONTROL_MIN_TARGET_KPA ? PUMP_MAX_SUCTION_KPA : CONTROL_MIN_TARGET_KPA;

// --- Vent valve ------------------------------------------------------------
// Timing only; which part is fitted, and therefore which coil level means "open", is the
// composition root's call in app/Build.cpp — normally open is the fail-safe default.
constexpr float VENT_MARGIN_KPA = 2.0f;      // close enough to ambient to be released
constexpr uint32_t VENT_TIMEOUT_MS = 30000;  // stop waiting on a leaky rig

// --- Battery ---------------------------------------------------------------
// 1S Li-ion (Q6), a straight line between empty and full. BATTERY_CALIBRATION trims the
// divider against a real meter because the ESP32 ADC is not lab grade.
constexpr float BATTERY_EMPTY_VOLTS = 3.2f;
constexpr float BATTERY_FULL_VOLTS = 4.2f;
constexpr float BATTERY_CALIBRATION = 1.0f;
constexpr uint8_t BATTERY_LOW_PERCENT = 20;

// --- Buzzer ----------------------------------------------------------------
// A passive piezo reproduces a square wave, so the firmware offers a frequency range rather than a
// volume. The note parser rejects a note outside it, and the serial field publishes it as the
// `tone` bounds.
constexpr float BUZZER_MIN_HZ = 100.0f;
constexpr float BUZZER_MAX_HZ = 10000.0f;

// --- Front panel keys -------------------------------------------------------
// A key released before this is a short press; at or after it, a long press. Both are reported when
// the key comes up. A consumer that wants a longer hold watches heldMs() and picks its own mark, so
// there is no third threshold here.
constexpr uint32_t BUTTON_LONG_MS = 600;
// Two short presses with the second starting within this of the first release are one double
// press. The first is held back for the window so it cannot also act as a single; a long press is
// never part of a double.
constexpr uint32_t BUTTON_DOUBLE_MS = 350;

// --- The device's own screen ------------------------------------------------
// The splash is read before the first live frame, and the panel is then repainted on a fixed
// cadence: often enough that the battery blink is visible, slow enough that a whole frame's I2C
// write does not crowd the control loop.
constexpr uint32_t SPLASH_HOLD_MS = 1500;
constexpr uint32_t UI_REFRESH_MS = 250;

// --- Controller link --------------------------------------------------------
// The device is a serial peripheral: every decision comes from the controller, so what
// lives here is only what the firmware must own to be fast or safe.
constexpr uint32_t SERIAL_BAUD = 115200;  // only meaningful on a real UART; USB CDC ignores it
// A command header is a domain, a verb and its arguments. Anything longer is refused rather than
// truncated. A command body, when a domain defines one, follows the header and is not bounded here.
constexpr uint16_t COMMAND_LINE_MAX = 128;
// The serial `fs` domain's write body. The declared byte count is authoritative and is capped here;
// a body that stops arriving for this long is abandoned and its partial file removed.
constexpr uint32_t FS_WRITE_MAX_BYTES = 1024 * 1024;
constexpr uint32_t FS_WRITE_IDLE_MS = 2000;
// With USB CDC the presence of a host is observable and its loss stops the run. On a plain
// UART it is not, so a lost link is simply never reported.

}  // namespace cfg
}  // namespace gardener
