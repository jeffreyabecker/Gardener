#pragma once

#include <stdint.h>

// Board wiring: which pin is what, and the electrical characteristics of the parts.
// Only hal/ and app/ should need this.

namespace eggplan {
namespace pins {

// --- I2C bus, shared by the OLED and the MPRLS sensor ----------------------
// GPIO8/9 are the ESP32-C3 Wire defaults. GPIO8 also drives the on board NeoPixel on
// some C3 Super Mini clones; move the bus to 4/5 if that matters.
constexpr int I2C_SDA = 8;
constexpr int I2C_SCL = 9;
constexpr uint32_t I2C_FREQ_HZ = 400000;

// --- Adafruit 1.3" 128x64 monochrome OLED, STEMMA QT (SH1106) --------------
constexpr uint16_t OLED_WIDTH = 128;
constexpr uint16_t OLED_HEIGHT = 64;
constexpr uint8_t OLED_I2C_ADDR = 0x3C;

// --- Adafruit MPRLS ported pressure sensor (0..25 PSI absolute) ------------
constexpr uint8_t MPRLS_I2C_ADDR = 0x18;
constexpr uint32_t MPRLS_POLL_MS = 20;
constexpr float MPRLS_MIN_KPA = 0.0f;
constexpr float MPRLS_MAX_KPA = 172.369f;  // 25 PSI full scale

// --- JQB2438 DC vacuum pump through a low side MOSFET ----------------------
constexpr int PUMP_PWM = 6;
constexpr uint32_t PUMP_PWM_FREQ_HZ = 20000;
constexpr uint8_t PUMP_PWM_RESOLUTION = 10;

// --- Vent valve ------------------------------------------------------------
// The valve is normally open (Q10): energise to close. A logic level MOSFET with a
// flyback diode across the coil, wired like the pump.
constexpr int VENT_VALVE = 5;

// --- Battery sense ---------------------------------------------------------
// A 1S Li-ion pack through a 100k/100k divider, so the top of the pack lands inside the
// ADC range. GPIO4 is ADC1 channel 4, and it is the only free ADC1 pin: GPIO0..3 are
// ADC1 as well, but they are the button pins, and a pressed button would yank the sense
// line to ground. GPIO4 also stays clear of the GPIO2 strapping pin.
constexpr int BATTERY_ADC = 4;
constexpr float BATTERY_DIVIDER_RATIO = 2.0f;

// --- Four button UI, active low to GND, internal pull ups ------------------
// Nothing reads these, and nothing times them: they are kept wired and named so a future
// build can use them without a board revision (Q16).
constexpr int BTN_UP = 0;
constexpr int BTN_DOWN = 1;
constexpr int BTN_OK = 3;
constexpr int BTN_BACK = 10;

}  // namespace pins
}  // namespace eggplan
