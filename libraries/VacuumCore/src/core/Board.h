#pragma once

#include <stdint.h>

namespace gardener {
namespace cfg {

// --- The machine's rig ------------------------------------------------------
// The sensor, the pump, the vent valve and the port the controller talks to it on. A board variant
// is a different set of numbers here.
//
// A pin is one shared truth — the composition root that hands it to the driver, the driver that
// reads it, and the wiring docs all name the same rig — so it is a constant here rather than a
// number in Machine.cpp.
//
// The panel is the only bus part, and it is the panel library's: its bus, its keys and its gauge
// are in PanelLink's core/Board.h.

// The pressure sensor: a CFSensor XGZP6847A100KPGN33, whose output is 0.2..2.7 V over -100..0 kPa.
// GPIO7 is ADC1 channel 6, clear of the buttons, the battery sense and the strapping pins, so it
// is the one ADC1 pin left for the part.
constexpr int SENSOR_ADC = 7;

// The pump: a JQB2438 through a low side MOSFET.
constexpr int PUMP_PWM = 6;

// The vent valve: a solenoid through a logic level MOSFET with a flyback diode across the coil,
// wired like the pump. Which coil level means "open" is chosen in Machine.cpp by the valve class it
// constructs; normally open is the fail-safe default.
constexpr int VENT_VALVE = 5;

// The controller link's speed. Only meaningful on a real UART; USB CDC ignores it.
constexpr uint32_t SERIAL_BAUD = 115200;

}  // namespace cfg
}  // namespace gardener
