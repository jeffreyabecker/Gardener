#include "Clock.h"

namespace {
unsigned long nowMs = 0;
}  // namespace

namespace host {

unsigned long millis() { return nowMs; }
void advance(unsigned long ms) { nowMs += ms; }
void reset() { nowMs = 0; }

}  // namespace host
