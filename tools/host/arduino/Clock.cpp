#include "Clock.h"

#include <Arduino.h>

namespace gardener::host {
namespace {

unsigned long clockMs = 0;

}  // namespace

unsigned long nowMs() { return clockMs; }

void advance(unsigned long ms) { clockMs += ms; }

void reset() { clockMs = 0; }

}  // namespace gardener::host

// The API declares both inside extern "C" in Common.h, so these definitions keep that linkage and
// become the calls every firmware source already makes.
unsigned long millis() { return gardener::host::nowMs(); }

void delay(unsigned long ms) { gardener::host::advance(ms); }
