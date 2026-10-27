#pragma once

// The little of Arduino.h the drawing sources include, for the host: the program-space reads the
// font tables go through, which are plain memory here.

#include <stdint.h>

#ifndef PROGMEM
#define PROGMEM
#endif

#define pgm_read_byte(addr) (*reinterpret_cast<const uint8_t*>(addr))
#define pgm_read_word(addr) (*reinterpret_cast<const uint16_t*>(addr))
