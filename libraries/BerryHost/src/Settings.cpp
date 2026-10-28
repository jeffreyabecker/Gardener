#include "Build.h"

#if GARDENER_PANEL

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <memory>
#include <utility>
#include <vector>

#include <Arduino.h>
#include <berry.h>

#include "panel/PanelContext.h"
#include "panel/Units.h"
#include "routines/PressureRow.h"
#include "routines/Routines.h"
#include "storage/Config.h"
#include "storage/DeviceConfig.h"
#include "ui/SettingsRows.h"
#include "Module.h"
#include "Modules.h"

namespace gardener {
namespace berry {
namespace {

// The four declarations a routine's settings list is made of, one class per kind. A call builds the
// row the declaration describes and the instance holds it, so the row exists from the line that
// declares it to the record that takes it. The classes sit in the module rather than the globals,
// so a page writes `settings.number(...)` and the kind names stay out of the image's namespace.
constexpr const char* NumberName = "number";
constexpr const char* PressureName = "pressure";
constexpr const char* DurationName = "duration";
constexpr const char* ChoiceName = "choice";

// The row belongs to the instance: the comobj owns it and the interpreter frees it with the
// instance.
using RowOwner = std::unique_ptr<gfx::ui::SettingsRow>;

int rowRelease(bvm* vm) {
  delete static_cast<RowOwner*>(be_tocomptr(vm, 1));
  be_return_nil(vm);
}

// The messages below are stack buffers because be_raise longjmps past any C++ destructor a String
// would need. Every check runs before the first String is built, so a refused declaration leaves
// nothing behind.

void requireArguments(bvm* vm, int count, const char* kind, const char* signature) {
  if (be_top(vm) < count) {
    char message[96];
    snprintf(message, sizeof(message), "settings.%s: %s expected", kind, signature);
    be_raise(vm, "type_error", message);
  }
}

void requireText(bvm* vm, int index, const char* kind, const char* what) {
  if (!be_isstring(vm, index)) {
    char message[64];
    snprintf(message, sizeof(message), "settings.%s: %s expected", kind, what);
    be_raise(vm, "type_error", message);
  }
}

void requireNumber(bvm* vm, int index, const char* kind, const char* what) {
  if (!be_isnumber(vm, index)) {
    char message[64];
    snprintf(message, sizeof(message), "settings.%s: %s expected", kind, what);
    be_raise(vm, "type_error", message);
  }
}

// A number the declaration gives, or nil for the nothing a nullable setting holds.
void requireNumberOrNil(bvm* vm, int index, const char* kind, const char* what) {
  if (be_isnil(vm, index)) return;
  requireNumber(vm, index, kind, what);
}

void requireBool(bvm* vm, int index, const char* kind, const char* what) {
  if (!be_isbool(vm, index)) {
    char message[64];
    snprintf(message, sizeof(message), "settings.%s: %s expected", kind, what);
    be_raise(vm, "type_error", message);
  }
}

// The formatter a declaration gives. Every routine file passes nil, and a row prints through its
// own number formatter; a scheme of its own arrives with the page that reads one.
void requireFormatter(bvm* vm, int index, const char* kind) {
  if (!be_isnil(vm, index)) {
    char message[64];
    snprintf(message, sizeof(message), "settings.%s: the formatter is nil", kind);
    be_raise(vm, "type_error", message);
  }
}

// Every call needs the panel's parts: a row takes the queue its editor reads and the stack its
// editor is pushed on, and neither exists before the panel runs.
void requirePanel(bvm* vm) {
  if (context == nullptr || routines == nullptr) {
    be_raise(vm, "runtime_error", "settings: no panel");
  }
}

// Whether the value at `index` is a list a routine can build: an instance of the list class, which
// is what `[...]` makes, or the interpreter's own list.
bool pushItems(bvm* vm, int index) {
  if (be_islist(vm, index)) {
    be_pushvalue(vm, index);
    return true;
  }
  if (!be_islistinstance(vm, index)) return false;
  be_getmember(vm, index, ".p");
  if (be_islist(vm, -1)) return true;
  be_pop(vm, 1);
  return false;
}

// Whether the value at `index` is one of the declarations this module makes: an instance of one of
// the four classes, carrying the comobj its row lives in. The class name is what tells a
// declaration apart from any other object a file could hand a record.
bool isDeclaration(bvm* vm, int index) {
  if (!be_isinstance(vm, index)) return false;
  const char* name = be_classname(vm, index);
  if (name == nullptr) return false;
  if (strcmp(name, NumberName) != 0 && strcmp(name, PressureName) != 0 &&
      strcmp(name, DurationName) != 0 && strcmp(name, ChoiceName) != 0) {
    return false;
  }
  if (!be_getmember(vm, index, ".p")) return false;
  const bool row = be_iscomobj(vm, -1);
  be_pop(vm, 1);
  return row;
}

// The row a declaration holds, moved out of it: a declaration that has been taken holds nothing, so
// one setting is one row on one record.
std::unique_ptr<gfx::ui::SettingsRow> takeRow(bvm* vm, int index) {
  be_getmember(vm, index, ".p");
  RowOwner* owner = static_cast<RowOwner*>(be_tocomptr(vm, -1));
  be_pop(vm, 1);
  return std::move(*owner);
}

// The unit the panel prints pressure in, out of the device's own document, as `device.unit()` reads
// it: a declaration is made while the panel runs, so a row is built in the unit showing then.
DisplayUnit displayUnit() {
  DisplayUnit unit = DisplayUnit::Kpa;
  parseDisplayUnit(context->config().get<String>(DeviceSection, UnitKey, "").c_str(), unit);
  return unit;
}

void holdRow(bvm* vm, std::unique_ptr<gfx::ui::SettingsRow> row) {
  be_newcomobj(vm, new RowOwner(std::move(row)), rowRelease);
  be_setmember(vm, 1, ".p");
  be_pop(vm, 1);
}

// --- the kinds -------------------------------------------------------------

// settings.number(key, default, label, format, step, min, max, nullable, whole). A whole
// declaration is held, printed and stored with no decimals; a real one keeps two.
int numberInit(bvm* vm) {
  requirePanel(vm);
  requireArguments(vm, 10, NumberName,
                   "init(key, default, label, format, step, min, max, nullable, whole)");
  requireText(vm, 2, NumberName, "the key");
  requireNumberOrNil(vm, 3, NumberName, "a default");
  requireText(vm, 4, NumberName, "the label");
  requireFormatter(vm, 5, NumberName);
  requireNumber(vm, 6, NumberName, "the step");
  requireNumber(vm, 7, NumberName, "the lower bound");
  requireNumber(vm, 8, NumberName, "the upper bound");
  requireBool(vm, 9, NumberName, "the nullable flag");
  requireBool(vm, 10, NumberName, "the whole flag");

  const uint8_t decimals = be_tobool(vm, 10) ? 0 : 2;
  holdRow(vm, std::make_unique<gfx::ui::NumberRow>(
                  routines->presses(), routines->scenes(), String(argText(vm, 2)),
                  String(argText(vm, 4)), be_isnil(vm, 3) ? NAN : (float)be_toreal(vm, 3),
                  (float)be_toreal(vm, 6), (float)be_toreal(vm, 7), (float)be_toreal(vm, 8),
                  be_tobool(vm, 9), nullptr, decimals));
  be_return_nil(vm);
}

// settings.pressure(key, default, label, format, step, nullable). The step is whole display steps
// and the bounds are the machine's, so a keypress cannot leave the editor on a target the loop
// refuses.
int pressureInit(bvm* vm) {
  requirePanel(vm);
  requireArguments(vm, 7, PressureName, "init(key, default, label, format, step, nullable)");
  requireText(vm, 2, PressureName, "the key");
  requireNumberOrNil(vm, 3, PressureName, "a default");
  requireText(vm, 4, PressureName, "the label");
  requireFormatter(vm, 5, PressureName);
  requireNumber(vm, 6, PressureName, "the step");
  requireBool(vm, 7, PressureName, "the nullable flag");

  holdRow(vm, std::make_unique<PressureRow>(
                  routines->presses(), routines->scenes(), String(argText(vm, 2)),
                  String(argText(vm, 4)), be_isnil(vm, 3) ? NAN : (float)be_toreal(vm, 3),
                  displayUnit(), (float)be_toreal(vm, 6), be_tobool(vm, 7)));
  be_return_nil(vm);
}

// settings.duration(key, default, label, format, step, min, max, nullable), in whole seconds.
int durationInit(bvm* vm) {
  requirePanel(vm);
  requireArguments(vm, 9, DurationName,
                   "init(key, default, label, format, step, min, max, nullable)");
  requireText(vm, 2, DurationName, "the key");
  requireNumber(vm, 3, DurationName, "a default");
  requireText(vm, 4, DurationName, "the label");
  requireFormatter(vm, 5, DurationName);
  requireNumber(vm, 6, DurationName, "the step");
  requireNumber(vm, 7, DurationName, "the lower bound");
  requireNumber(vm, 8, DurationName, "the upper bound");
  requireBool(vm, 9, DurationName, "the nullable flag");

  holdRow(vm, std::make_unique<gfx::ui::DurationRow>(
                  routines->presses(), routines->scenes(), String(argText(vm, 2)),
                  String(argText(vm, 4)), (int32_t)be_toint(vm, 3), (int32_t)be_toint(vm, 6),
                  (int32_t)be_toint(vm, 7), (int32_t)be_toint(vm, 8), be_tobool(vm, 9)));
  be_return_nil(vm);
}

// settings.choice(key, default, label, format, entries, nullable). An entry is one string, carried
// as the key the document keeps and the title the list prints.
int choiceInit(bvm* vm) {
  requirePanel(vm);
  requireArguments(vm, 7, ChoiceName, "init(key, default, label, format, entries, nullable)");
  requireText(vm, 2, ChoiceName, "the key");
  requireText(vm, 3, ChoiceName, "a default");
  requireText(vm, 4, ChoiceName, "the label");
  requireFormatter(vm, 5, ChoiceName);
  requireBool(vm, 7, ChoiceName, "the nullable flag");
  if (!pushItems(vm, 6)) {
    be_raise(vm, "type_error", "settings.choice: entries is a list");
  }

  const int entries = be_absindex(vm, -1);
  const int count = be_data_size(vm, entries);
  for (int i = 0; i < count; i++) {
    be_pushint(vm, i);
    be_getindex(vm, entries);
    be_remove(vm, -2);  // the index the read left under the entry
    const bool text = be_isstring(vm, -1);
    be_pop(vm, 1);
    if (!text) {
      be_raise(vm, "type_error", "settings.choice: every entry is a string");
    }
  }

  std::vector<gfx::ui::ChoiceItem> items;
  items.reserve((size_t)count);
  for (int i = 0; i < count; i++) {
    be_pushint(vm, i);
    be_getindex(vm, entries);
    be_remove(vm, -2);
    gfx::ui::ChoiceItem item;
    item.key = argText(vm, -1);
    item.title = item.key;
    be_pop(vm, 1);
    items.push_back(std::move(item));
  }
  be_pop(vm, 1);  // the entries

  holdRow(vm, std::make_unique<gfx::ui::ChoiceRow>(
                  routines->presses(), routines->scenes(), String(argText(vm, 2)),
                  String(argText(vm, 4)), String(argText(vm, 3)), items, be_tobool(vm, 7)));
  be_return_nil(vm);
}

const bnfuncinfo NumberMembers[] = {
    {".p", nullptr},
    {"init", numberInit},
    {nullptr, nullptr},
};

const bnfuncinfo PressureMembers[] = {
    {".p", nullptr},
    {"init", pressureInit},
    {nullptr, nullptr},
};

const bnfuncinfo DurationMembers[] = {
    {".p", nullptr},
    {"init", durationInit},
    {nullptr, nullptr},
};

const bnfuncinfo ChoiceMembers[] = {
    {".p", nullptr},
    {"init", choiceInit},
    {nullptr, nullptr},
};

}  // namespace

// The rows a settings list declares, in the order the list gave them: every entry is a declaration
// of one of this module's kinds, and each is consumed, since a row belongs to the record that takes
// it. The checks run before any row is taken, so a list holding anything else raises and leaves
// every declaration as it was.
std::vector<std::unique_ptr<gfx::ui::SettingsRow>> takeSettingsRows(bvm* vm, int index) {
  if (!pushItems(vm, index)) {
    be_raise(vm, "type_error", "Registration: settings is a list of declarations");
  }
  const int list = be_absindex(vm, -1);
  const int count = be_data_size(vm, list);

  for (int i = 0; i < count; i++) {
    be_pushint(vm, i);
    be_getindex(vm, list);
    be_remove(vm, -2);  // the index the read left under the entry
    const bool declaration = isDeclaration(vm, -1);
    be_pop(vm, 1);
    if (!declaration) {
      be_raise(vm, "type_error", "Registration: settings is a list of declarations");
    }
  }

  std::vector<std::unique_ptr<gfx::ui::SettingsRow>> rows;
  rows.reserve((size_t)count);
  for (int i = 0; i < count; i++) {
    be_pushint(vm, i);
    be_getindex(vm, list);
    be_remove(vm, -2);
    rows.push_back(takeRow(vm, -1));
    be_pop(vm, 1);
  }
  be_pop(vm, 1);  // the list
  return rows;
}

// The `settings` module: the four declaration classes, installed as members so a file reaches a
// kind through the module rather than through a global of its own.
void installSettingsModule(bvm* vm) {
  struct Kind {
    const char* name;
    const bnfuncinfo* members;
  };
  const Kind kinds[] = {
      {NumberName, NumberMembers},
      {PressureName, PressureMembers},
      {DurationName, DurationMembers},
      {ChoiceName, ChoiceMembers},
  };

  be_newmodule(vm);
  const int module = be_absindex(vm, -1);
  for (const Kind& kind : kinds) {
    be_pushclass(vm, kind.name, kind.members);
    // be_setmember copies into the module and leaves the class where it was, so the push is undone
    // here. The absolute index keeps the module valid across the push.
    be_setmember(vm, module, kind.name);
    be_pop(vm, 1);
  }
  be_setglobal(vm, "settings");
  // be_setglobal leaves the value it stored on the stack; the module is done with here.
  be_pop(vm, 1);
}

}  // namespace berry
}  // namespace gardener
#endif
