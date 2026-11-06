#pragma once

// The one way a constant a build may retune is declared:
//
//   GARDENER_KNOB_VALID(uint8_t, GestureQueueDepth, GARDENER_GESTURE_QUEUE_DEPTH, 64,
//                       GARDENER_GESTURE_QUEUE_DEPTH > 0, "a queue of nothing refuses every gesture");
//
// The value is the token. core/knob_defaults.h gives every token its default inside an #ifndef, so a
// build replaces one by defining it first — the sketch's rig_options.h, written from `--define`.
// The default is also an argument, read by tools/knob.py to write that header: a macro cannot apply
// it, because the test for "already defined" is a preprocessing directive and a replacement list
// cannot contain one.
//
// inline so a header's knob is one object across the build. Two translation units that disagree
// about a knob are a wrong answer the linker cannot see, so a knob belongs to the whole build and is
// set in one place.
#define GARDENER_KNOB(type, name, token) inline constexpr type name = token

#define GARDENER_KNOB_VALID(type, name, token, default, assertion, message)                        \
  GARDENER_KNOB(type, name, token);                                                                \
  static_assert(assertion, message)

// The rig's own numbers come first: rig_options.h holds the define for each knob this rig retunes,
// and the generated defaults fill in the ones it does not.
#if __has_include("rig_options.h")
#include "rig_options.h"
#endif

#include "core/knob_defaults.h"
