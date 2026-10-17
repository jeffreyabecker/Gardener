#include "app/serial/SerialLink.h"

#include <Arduino.h>

#include "app/serial/Items.h"
#include "app/serial/SerialSurface.h"

namespace gardener {

// `help` answers with a page: `ok`, then one row per item, then `end`. That is the shape every list
// in the protocol has, and it is what lets a value hold any printable character — a separator that
// can appear inside a value is a separator that has to be escaped.
//
// Every row is derived from the tables in app/serial/SerialPayload.cpp and app/serial/SerialDraw.cpp, so a host
// that enumerates `help` and replays what it finds can never be told `unknown_command`, and a bound
// printed here is the same constant the validator refuses with.

void SerialLink::help(Tokens& tokens) {
  char* resource = tokens.next();
  if (!resource) {
    emitResourceList();
    return;
  }
  lower(resource);

  uint8_t count = 0;
  const ResourceSpec* resources = SerialLink::resources(count);
  for (uint8_t i = 0; i < count; i++) {
    if (!same(resources[i].name, resource)) continue;

    char* op = tokens.next();
    if (!op) {
      emitResourcePage(resources[i]);
      return;
    }
    lower(op);

    // An action with a list behind it expands into that list, and `panel draw` is the only one: its
    // list is the drawing operators. Anything else after a resource is as unknown as a field that
    // does not exist, which is what stops `help vacuum draw` inventing a page of operators that
    // `vacuum` cannot draw.
    for (uint8_t a = 0; a < resources[i].actionCount; a++) {
      if (!same(resources[i].actions[a].name, op)) continue;
      if (!resources[i].actions[a].takesRest) break;
      if (tokens.rest() != nullptr) {
        errBadArgument(tokens.rest());
        return;
      }
      emitDrawPage(resources[i], op);
      return;
    }
    errUnknown(op);
    return;
  }
  errUnknown(resource);
}

// Every resource and the size of each: the map of the whole interface in one reply.
void SerialLink::emitResourceList() {
  uint8_t count = 0;
  const ResourceSpec* resources = SerialLink::resources(count);

  Items header;
  header.add("result", "help");
  header.addU32("rows", count);
  ok(header.text());

  for (uint8_t i = 0; i < count; i++) {
    Items items;
    items.add("resource", resources[i].name);
    items.addU32("fields", resources[i].fieldCount);
    items.addU32("actions", resources[i].actionCount);
    row(items.text());
  }
  endPage(header.text());
}

// One resource: its fields with the bounds a write is held to, then its actions.
void SerialLink::emitResourcePage(const ResourceSpec& resource) {
  Items header;
  header.add("result", "help");
  header.add("resource", resource.name);
  header.addU32("rows", (uint32_t)resource.fieldCount + resource.actionCount);
  ok(header.text());

  for (uint8_t i = 0; i < resource.fieldCount; i++) {
    const FieldSpec& field = resource.fields[i];
    Items items;
    items.add("field", field.name);
    items.add("type", field.type);
    items.add("access", field.writable ? "rw" : "r");
    if (field.unit) items.add("unit", field.unit);
    if (field.choices) items.add("choices", field.choices);
    if (field.min) items.addFloat("min", *field.min, 1);
    if (field.max) items.addFloat("max", *field.max, 1);
    row(items.text());
  }

  for (uint8_t i = 0; i < resource.actionCount; i++) {
    Items items;
    items.add("action", resource.actions[i].name);
    // An action whose argument is the rest of the line says so, and its own page says what to put
    // there.
    if (resource.actions[i].takesRest) items.add("rest", "1");
    row(items.text());
  }
  endPage(header.text());
}

// One action with a list behind it: the drawing operators, with the arguments each one takes.
void SerialLink::emitDrawPage(const ResourceSpec& resource, const char* action) {
  uint8_t count = 0;
  const DrawOp* ops = drawOpTable(count);

  Items header;
  header.add("result", "help");
  header.add("resource", resource.name);
  header.add("op", action);
  header.addU32("rows", count);
  ok(header.text());

  for (uint8_t i = 0; i < count; i++) {
    Items items;
    items.add("op", ops[i].name);
    items.addU32("args", ops[i].args);
    if (ops[i].string) items.add("string", "1");
    row(items.text());
  }
  endPage(header.text());
}

void SerialLink::row(const String& items) { writeLine("", items.c_str()); }

// The line that closes a page repeats the page's own items after `end`, so a controller that joins a
// page to its `ok` line has one description of what it just received.
void SerialLink::endPage(const String& headerItems) {
  String line = "end ";
  line += headerItems;
  writeLine("", line.c_str());
}

}  // namespace gardener