#pragma once

#include <stdint.h>

// Board wiring: which pin is what, plus the two facts about the harness itself — how fast the
// shared bus runs and how the pack's divider is built. The characteristics of the *parts* — the
// panel's geometry and address, the sensor's range and poll interval, the PWM channel's setup —
// live with the driver that reads them, in hal/impl/, because they change with the part and not
// with the board.
//
// Only hal/ and app/ should need this.

namespace gardener {
namespace pins {

// --- I2C bus, shared by the OLED and the MPRLS sensor ----------------------
// GPIO8/9 are the ESP32-S3 Wire defaults (`SDA`/`SCL` in the board variant). The bus
// stays clear of the S3 strapping pins (0, 3, 45, 46) and of the USB pins (19/20).
constexpr int I2C_SDA = 8;
constexpr int I2C_SCL = 9;
// The harness rather than either device on it: two parts share the bus, so the root owns its speed.
constexpr uint32_t I2C_FREQ_HZ = 400000;

// --- JQB2438 DC vacuum pump through a low side MOSFET ----------------------
constexpr int PUMP_PWM = 6;

// --- Vent valve ------------------------------------------------------------
// A solenoid through a logic level MOSFET with a flyback diode across the coil, wired like the
// pump. Which coil level means "open" is chosen in app/App.h by the valve class it constructs;
// normally open is the fail-safe default.
constexpr int VENT_VALVE = 5;

// --- Battery sense ---------------------------------------------------------
// A 1S Li-ion pack through a 100k/100k divider, so the top of the pack lands inside the
// ADC range. GPIO4 is ADC1 channel 3, and it is the one ADC1 pin that is neither a button
// nor a strapping pin: ADC1 covers GPIO1..GPIO10, GPIO1/2/10 are button pins, and a pressed
// button would yank the sense line to ground. The S3 strapping pins (0, 3, 45, 46) stay
// clear too.
constexpr int BATTERY_ADC = 4;
// The board's divider, not the pack's property: two equal resistors, so the ratio is 2.0 and
// BATTERY_CALIBRATION trims the pair against a meter.
constexpr float BATTERY_DIVIDER_RATIO = 2.0f;

// --- Four button UI, active low to GND, pull ups on the module -------------
// Nothing reads these, and nothing times them: they are kept wired and named so a future
// build can use them without a board revision (Q16). They sit away from the S3 strapping
// pins (0, 3, 45, 46) and from the USB pins (19/20). GPIO10/11 are the variant's default
// SS/MOSI, which costs nothing here: this firmware never starts SPI.
constexpr int BTN_UP = 1;
constexpr int BTN_DOWN = 2;
constexpr int BTN_OK = 10;
constexpr int BTN_BACK = 11;

}  // namespace pins
}  // namespace gardener
