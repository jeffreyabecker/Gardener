// Adafruit_GFX.h — the little of the library the font headers need, for the host.
//
// The faces under Fonts/ begin with #include <Adafruit_GFX.h>, which on a device brings in Arduino
// and Print. The check only wants the font structs, so this file is put ahead of the library on the
// include path and supplies them, plus PROGMEM as nothing.

#pragma once

#include <stdint.h>

#ifndef PROGMEM
#define PROGMEM
#endif

#include <gfxfont.h>
