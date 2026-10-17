#pragma once

#include <stdint.h>

#include "app/serial/Items.h"
#include "core/Snapshot.h"
#include "hal/Display.h"

namespace gardener {

class SerialLink;

// How many fields one write command may carry. A limit rather than a growing buffer, because this
// path allocates nothing; it is four because no resource has more writable fields than that, so
// nothing a host has a reason to send is refused for it.
constexpr uint8_t kMaxFieldWrites = 4;

// What a reply may depend on: the machine as it is now, the machine as of the last control tick,
// the panel, and the interface's own settings. A field reader is a plain function of this, which is
// what lets the field tables be built at file scope instead of naming a member per field.
struct WireView {
  // The vacuum state is copied in rather than referenced. A reply has to describe the machine as
  // the command just left it, and the snapshot below is only rebuilt once per loop iteration, so
  // `vacuum start` reporting the state it started *from* is exactly the bug this avoids. It is one
  // small copy per reply, and the readers are then plain functions of their inputs.
  VacuumState vacuum;
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

// One action. It takes no arguments: everything a command carries beyond the action is a write.
struct ActionSpec {
  const char* name;
  void (SerialLink::*run)();
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
