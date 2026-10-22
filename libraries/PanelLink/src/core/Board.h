#pragma once

#include <stdint.h>

namespace gardener {
namespace cfg {

// --- The panel's half of the rig --------------------------------------------
// The display bus, the piezo, the gauge and the four keys. The machine's pins are in the core's
// core/Board.h; this file is the panel library's, because only a build with a panel has them.

// The panel is the only bus part: it is on `Wire1` (bus 1) at two of the free pins, clear of the
// S3 strapping pins (0, 3, 45, 46) and of the USB pins (19/20).
constexpr int PANEL_I2C_SDA = 17;
constexpr int PANEL_I2C_SCL = 18;
// The bus speed belongs to the harness rather than to the part on it.
constexpr uint32_t PANEL_I2C_FREQ_HZ = 400000;

// The piezo: a passive element on a spare GPIO, driven by one LEDC channel. GPIO12 is the fifth
// key's pin on the POC 5-way module; the firmware reads only four keys, and the buzzer takes the
// pin. A board that wants that key back has to move the buzzer.
constexpr int BUZZER_PWM = 12;

// Battery sense: a 1S Li-ion pack through a 100k/100k divider, so the top of the pack lands inside
// the ADC range. GPIO4 is ADC1 channel 3, and it is the ADC1 pin that is neither a button nor the
// sensor: ADC1 covers GPIO1..GPIO10, GPIO1/2/10 are button pins, GPIO7 is the sensor, and a pressed
// button would yank the sense line to ground. The S3 strapping pins (0, 3, 45, 46) stay clear too.
constexpr int BATTERY_ADC = 4;
// The board's divider, not the pack's property: two equal resistors, so the ratio is 2.0 and the
// gauge trims the pair against a meter for the rest of the line.
constexpr float BATTERY_DIVIDER_RATIO = 2.0f;

// Four button UI, active low to GND with pull ups on the module. The button driver arms them on
// change interrupts and settles the debounce on each read; the panel owns the keys and their
// gestures.
// They sit away from the S3 strapping pins (0, 3, 45, 46) and from the USB pins (19/20). GPIO10/11
// are the variant's default SS/MOSI, which costs nothing here: this firmware never starts SPI.
constexpr int BTN_UP = 1;
constexpr int BTN_DOWN = 2;
constexpr int BTN_OK = 10;
constexpr int BTN_BACK = 11;

}  // namespace cfg
}  // namespace gardener
