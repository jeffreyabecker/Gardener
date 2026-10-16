#pragma once

#include <stdint.h>

#include "app/serial/Items.h"
#include "core/Snapshot.h"
#include "hal/Display.h"

namespace gardener {

class SerialLink;

// The two numbers on the wire that are not free: the panel turns through four orientations, and the
// built-in font scales 1 to 8. Here rather than in core/Config.h because they belong to the panel
// and its font rather than to the machine's policy, and here rather than in the two files that
// enforce them because the table prints one of them and the error reply the other.
constexpr float ROTATION_MIN = 0.0f;
constexpr float ROTATION_MAX = 3.0f;
constexpr float DRAW_SIZE_MIN = 1.0f;
constexpr float DRAW_SIZE_MAX = 8.0f;

// How many fields one write command may carry. A limit rather than a growing buffer, because this
// path allocates nothing; it is four because no resource has more writable fields than that, so
// nothing a host has a reason to send is refused for it.
constexpr uint8_t kMaxFieldWrites = 4;

// What a reply may depend on: the machine as it is now, the machine as of the last control tick,
// the panel, and the interface's own settings. A field reader is a plain function of this, which is
// what lets the field tables be built at file scope instead of naming a member per field.
struct WireView {
  // The plant is copied in rather than referenced. A reply has to describe the machine as the
  // command just left it, and the snapshot below is only rebuilt once per loop iteration, so
  // `plant start` reporting the state it started *from* is exactly the bug this avoids. It is one
  // small copy per reply, and the readers are then plain functions of their inputs.
  PlantState plant;
  const SystemSnapshot& snapshot;
  const Display& display;
  bool logEnabled;
  // A marker is stored decoded and printed escaped, so what is here is the marker itself rather
  // than the token a host sent.
  const String& terminator;
  bool customTerminator;
  // Two flags rather than one, because "nobody is there" and "this transport cannot tell" are
  // different answers and the field reports them differently.
  bool hostKnown;
  bool hostAttached;
};

// One drawing operator: its name, how many coordinates it takes, whether its last argument is a
// string that runs to the end of the line, and the call it makes.
//
// The dispatch, the `op=` item in the reply and the rows of `help panel draw` all come from here,
// so an operator cannot be drawn without being listed or listed without being drawn.
struct DrawOp {
  const char* name;
  uint8_t args;
  bool string;
  void (*call)(Display& display, const int16_t* args, const char* text);
};

// The operators, in the order `help panel draw` lists them. Defined in app/serial/SerialDraw.cpp beside
// the calls themselves.
const DrawOp* drawOpTable(uint8_t& count);

// One field of one resource.
//
// `type`, `unit` and `choices` are for the help page. `min` and `max` point at the constants the
// validators use, so a bound is printed from the same place it is enforced and cannot be typed
// twice; both are null when the field has no bound.
struct FieldSpec {
  const char* name;
  bool writable;
  const char* type;     // "float", "int", "bool", "choice" or "text"
  const char* unit;     // null when the value has no unit
  const char* choices;  // null unless the values are a closed set, comma separated
  const float* min;
  const float* max;
  void (*read)(const WireView& view, Items& out);
};

// One action. `takesRest` says the action's argument is the rest of the line rather than a fixed
// count of tokens, which is how `panel draw` receives an op and its coordinates at all.
struct ActionSpec {
  const char* name;
  bool takesRest;
  void (SerialLink::*run)(char* rest);
};

// One field of one write, after the generic half of the dispatch has established that the field
// exists and that a host may write it. Nothing is applied until every one of them has been parsed
// and validated, which is what makes a multi-field write all or nothing.
struct FieldWrite {
  const FieldSpec* field;
  const char* value;
};

// One resource: the fields a host can read, the actions it can ask for, and the writer for its
// writable fields, null when it has none.
//
// Every reply is derived from these tables. `get` walks a resource's fields, a single-field read
// finds one by name, a write's echo reads the field it just set, and `help` prints the same names
// with the same bounds — so a field cannot be readable without being listed, listed without
// something to read it, or listed with a bound the validator does not use.
struct ResourceSpec {
  const char* name;
  const FieldSpec* fields;
  uint8_t fieldCount;
  const ActionSpec* actions;
  uint8_t actionCount;
  bool (SerialLink::*write)(const FieldWrite* writes, uint8_t count);
};

}  // namespace gardener
