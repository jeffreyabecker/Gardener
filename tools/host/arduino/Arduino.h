#pragma once

// The host's Arduino.h. It is the core's entry header, not a copy of a class: everything the
// firmware names — String, Print, Stream, Printable, millis and delay, the pgmspace reads Gfx's font
// tables go through — comes from Arduino's own published API, vendored beside this file. What the
// host still has to answer for itself is the clock and a port a case types into, and both live here
// beside it.

// The API's headers use the C library before they include it, so the core's entry header puts it
// first, as a board's does.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <ArduinoAPI.h>
