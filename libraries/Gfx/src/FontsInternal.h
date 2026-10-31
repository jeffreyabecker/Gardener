#pragma once

#include "Fonts.h"

namespace gardener::gfx {

// The design a leaf or a measurement that names no family draws in: what setDefaultFamily set, or
// the Adafruit family when nothing has, and never null. It is the module's plumbing — the resolver
// reads it to fill a box and nothing else asks — so this header is included inside this library and
// by nothing above it. A caller that wants to know what a leaf will draw in draws one.
const Family* defaultFamily();

}  // namespace gardener::gfx
