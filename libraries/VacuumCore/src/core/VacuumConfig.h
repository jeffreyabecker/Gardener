#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Eggplan Vacuum - board configuration
//
// Target: ESP32-C3 (layout assumes an ESP32-C3 Super Mini).
// Everything here is a compile time default; override per build with -D flags.
// ---------------------------------------------------------------------------

#ifndef EGGPLAN_WITH_BERRY
#define EGGPLAN_WITH_BERRY 0
#endif

namespace eggplan {
namespace cfg {

// --- I2C bus, shared by the OLED and the MPRLS sensor ----------------------
// GPIO8/9 are the ESP32-C3 Wire defaults. GPIO8 also drives the on board
// NeoPixel on some C3 Super Mini clones; move the bus to 4/5 if that matters.
constexpr int PIN_I2C_SDA = 8;
constexpr int PIN_I2C_SCL = 9;
constexpr uint32_t I2C_FREQ_HZ = 400000;

// --- Adafruit 1.3" 128x64 monochrome OLED, STEMMA QT (SH1106, 0x3C) --------
constexpr uint16_t OLED_WIDTH = 128;
constexpr uint16_t OLED_HEIGHT = 64;
constexpr uint8_t OLED_I2C_ADDR = 0x3C;

// --- Adafruit MPRLS ported pressure sensor breakout (0..25 PSI absolute) ---
// kPa is the primary unit everywhere: the application lives well below 60 kPa
// absolute, so hPa style values would only add digits without adding meaning.
constexpr uint8_t MPRLS_I2C_ADDR = 0x18;
constexpr uint32_t MPRLS_POLL_MS = 20;
constexpr float DEFAULT_AMBIENT_KPA = 101.325f;    // one standard atmosphere
constexpr float MPRLS_MIN_KPA = 0.0f;
constexpr float MPRLS_MAX_KPA = 172.369f;          // 25 PSI full scale
constexpr float WORKING_RANGE_MAX_KPA = 60.0f;     // sensors and pump are happy below this

// --- JQB2438 DC vacuum pump, driven through a low side MOSFET --------------
// PWM  -> gate of a logic level N-MOSFET (flyback diode across the pump)
// TACH -> optional open collector tach/hall output, needs a pull up
constexpr int PIN_PUMP_PWM = 6;
constexpr int PIN_PUMP_TACH = 7;
constexpr bool PUMP_TACH_ENABLED = false;
constexpr uint8_t PUMP_TACH_PULSES_PER_REV = 2;
constexpr uint32_t PUMP_PWM_FREQ_HZ = 20000;
constexpr uint8_t PUMP_PWM_RESOLUTION = 10;
constexpr float PUMP_DUTY_MAX_PERCENT = 90.0f;
constexpr float PUMP_RAMP_PERCENT_PER_SEC = 150.0f;
constexpr uint32_t PUMP_MAX_RUN_MS = 15UL * 60UL * 1000UL;

// --- Four button UI, active low to GND, internal pull ups ------------------
constexpr int PIN_BTN_UP = 0;
constexpr int PIN_BTN_DOWN = 1;
constexpr int PIN_BTN_OK = 3;
constexpr int PIN_BTN_BACK = 10;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 25;
constexpr uint32_t BUTTON_LONGPRESS_MS = 800;
constexpr uint32_t BUTTON_REPEAT_MS = 180;

// --- Closed loop vacuum control -------------------------------------------
constexpr uint32_t CONTROL_PERIOD_MS = 50;
// Floor for the loop target: below the pump's ultimate vacuum the duty would
// simply sit at the cap, so the value mostly guards against nonsense targets.
constexpr float CONTROL_MIN_TARGET_KPA = 1.0f;
constexpr float PID_KP = 3.0f;    // % duty per kPa of error
constexpr float PID_KI = 0.2f;    // % duty per kPa second
constexpr float PID_KD = 0.5f;    // % duty per kPa/s of pressure slew
constexpr float PID_INTEGRAL_LIMIT = 150.0f;
constexpr float CONTROL_DEADBAND_KPA = 0.2f;

// --- Bluetooth ------------------------------------------------------------
constexpr const char* BLE_NAME_PREFIX = "EggplanVac";
constexpr uint16_t BLE_MTU = 517;
constexpr uint32_t TELEMETRY_DEFAULT_MS = 250;
constexpr size_t BLE_LINE_MAX = 512;

// --- Script store ---------------------------------------------------------
constexpr const char* PROGRAM_DIR = "/programs";
constexpr const char* PROGRAM_TMP = "/programs/.upload.tmp";
constexpr const char* AUTOSTART_FILE = "/programs/.autostart";
constexpr size_t PROGRAM_NAME_MAX = 31;
constexpr size_t PROGRAM_MAX_BYTES = 64 * 1024;
constexpr uint8_t PROGRAM_MAX_COUNT = 24;

// --- Firmware -------------------------------------------------------------
constexpr const char* FIRMWARE_VERSION = "0.1.0";

}  // namespace cfg
}  // namespace eggplan
