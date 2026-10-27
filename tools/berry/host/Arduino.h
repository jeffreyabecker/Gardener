#pragma once

// The little of Arduino.h the compiled panel and machine sources include, for the host. The parts
// that reach real hardware are not here, because nothing that uses them is compiled: what is here is
// the text type, the clock and the integer names.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Clock.h"
#include "WString.h"

#ifndef PROGMEM
#define PROGMEM
#endif

#define pgm_read_byte(addr) (*reinterpret_cast<const uint8_t*>(addr))
#define pgm_read_word(addr) (*reinterpret_cast<const uint16_t*>(addr))

typedef uint8_t byte;
typedef bool boolean;
