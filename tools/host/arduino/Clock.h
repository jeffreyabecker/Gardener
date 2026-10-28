#pragma once

// The clock the firmware reads through millis() and waits on through delay(). On the device the core
// answers both; here the suite owns them, so a case that drives a run for an hour costs no real
// time. The definitions are in Clock.cpp, which is the one place that reaches the API's global
// declarations.

namespace gardener::host {

// How far the fake clock has been advanced, in milliseconds.
unsigned long nowMs();

// Moves the clock forward. delay() advances it by the time it is asked to wait.
void advance(unsigned long ms);

// Back to zero, for a case that starts a fresh timeline.
void reset();

}  // namespace gardener::host
