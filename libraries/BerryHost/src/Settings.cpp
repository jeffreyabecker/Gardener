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
#include "panel/glass/PressureRow.h"
#include "panel/glass/Units.h"
#include "panel/page/SettingsRows.h"
#include "routines/Routines.h"
#include "storage/Config.h"
#include "storage/DeviceConfig.h"
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

// --- the declaration read back ---------------------------------------------

// The record a routine's declarations are read back through: a page reads one off
// `routines.settings_cfgs` and `SettingsEditScene` takes the row it holds. The row is borrowed
// rather than owned, because reading a record's declarations leaves them where they were — the same
// list opened twice edits the same declarations.
constexpr const char* SettingRecord = "RoutineSetting";

const bnfuncinfo SettingFields[] = {
    {"init", recordInit},
    {".p", nullptr},
    {"key", nullptr},
    {"label", nullptr},
    {"kind", nullptr},
    {"step", nullptr},
    {"min", nullptr},
    {"max", nullptr},
    {"entries", nullptr},
    {nullptr, nullptr},
};

// The row a record holds, borrowed: the comobj is what carries a C++ object into the image, and the
// box is what the interpreter frees, never the row in it.
struct RowRef {
  gfx::ui::SettingsRow* row;
};

int rowRefRelease(bvm* vm) {
  delete static_cast<RowRef*>(be_tocomptr(vm, 1));
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

// The unit the panel prints pressure in, out of the device's own document: a declaration is made while
// the panel runs, so a row is built in the unit showing then.
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

// A declaration's number, and nil for the one its kind does not have: a switch has no step, a choice
// no bound.
void setNumberOrNil(bvm* vm, int record, const char* field, float value) {
  if (isnan(value)) {
    be_pushnil(vm);
  } else {
    be_pushreal(vm, value);
  }
  be_setmember(vm, record, field);
  be_pop(vm, 1);
}

// The keys a choice offers, as the list class's instance the image iterates over, and an empty one
// for a kind that offers none. Left on the stack for the caller to set on a record.
void pushEntries(bvm* vm, const std::vector<gfx::ui::ChoiceItem>& entries) {
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (const gfx::ui::ChoiceItem& entry : entries) {
    be_pushstring(vm, entry.key.c_str());
    be_data_push(vm, list);
    be_pop(vm, 1);
  }
  be_pop(vm, 1);  // the raw list, leaving the instance as the answer
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
    be_raise(vm, "type_error", "routines.register: settings is a list of declarations");
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
      be_raise(vm, "type_error", "routines.register: settings is a list of declarations");
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

// The declaration a record holds, as the record a page reads it back through: the row it names,
// borrowed, with the key, label, kind and the step, bounds and entries that kind was declared with.
// Left on the stack for the caller to push onto a list.
void pushSettingDeclaration(bvm* vm, gfx::ui::SettingsRow* row) {
  const gfx::ui::SettingDeclaration declaration = row->declaration();
  const int record = beginRecord(vm, SettingRecord);

  be_newcomobj(vm, new RowRef{row}, rowRefRelease);
  be_setmember(vm, record, ".p");
  be_pop(vm, 1);

  setText(vm, record, "key", row->key().c_str());
  setText(vm, record, "label", row->label().c_str());
  setText(vm, record, "kind", declaration.kind);
  setNumberOrNil(vm, record, "step", declaration.step);
  setNumberOrNil(vm, record, "min", declaration.min);
  setNumberOrNil(vm, record, "max", declaration.max);
  pushEntries(vm, declaration.entries);
  setValue(vm, record, "entries", -1);
  be_pop(vm, 1);
}

namespace {

// Whether the value at `index` is one of the records `routines.settings_cfgs` answers: the class is
// what tells a declaration a routine's record made from any other object a file could hand the list.
bool isDeclarationRecord(bvm* vm, int index) {
  if (!be_isinstance(vm, index)) return false;
  const char* name = be_classname(vm, index);
  return name != nullptr && strcmp(name, SettingRecord) == 0;
}

// The row a declaration record borrows: it is in the comobj the record's `.p` field carries, and the
// list is read rather than consumed, so the same declarations open a list again.
gfx::ui::SettingsRow* borrowedRow(bvm* vm, int index) {
  const int base = be_top(vm);
  gfx::ui::SettingsRow* row = nullptr;
  if (be_getmember(vm, index, ".p") && be_iscomobj(vm, -1)) {
    RowRef* held = static_cast<RowRef*>(be_tocomptr(vm, -1));
    if (held != nullptr) row = held->row;
  }
  be_pop(vm, be_top(vm) - base);
  return row;
}

}  // namespace

// SettingsEditScene(cfgs, values, title): the list a routine's declaration list opens, built from
// the records `routines.settings_cfgs` answers — the rows they borrow, so nothing is read back from
// a document — the map the list starts from, and the heading over it; a nil title draws none, and
// nil declarations draw an empty list. The list answers the set it settled on, which is what a page
// pushed with it is handed back.
int settingsEditScene(bvm* vm) {
  if (context == nullptr || routines == nullptr) {
    be_raise(vm, "runtime_error", "SettingsEditScene: no panel");
  }
  if (be_top(vm) < 2) {
    be_raise(vm, "type_error", "SettingsEditScene: SettingsEditScene(cfgs, values, title)");
  }
  const bool values = !be_isnil(vm, 2);
  if (values && !be_ismap(vm, 2) && !be_ismapinstance(vm, 2)) {
    be_raise(vm, "type_error", "SettingsEditScene: values is a map");
  }
  const bool titled = be_top(vm) >= 3 && !be_isnil(vm, 3);
  if (titled && !be_isstring(vm, 3)) {
    be_raise(vm, "type_error", "SettingsEditScene: the title is a string");
  }
  // Every check runs before the first row is read, so a refused call changes nothing.
  const int base = be_top(vm);
  const bool listed = !be_isnil(vm, 1);
  if (listed && !pushItems(vm, 1)) {
    be_raise(vm, "type_error", "SettingsEditScene: cfgs is a list of declarations");
  }
  const int list = listed ? be_absindex(vm, -1) : -1;
  const int count = listed ? be_data_size(vm, list) : 0;
  for (int i = 0; i < count; i++) {
    be_pushint(vm, i);
    be_getindex(vm, list);
    const bool declaration = isDeclarationRecord(vm, -1);
    be_pop(vm, 2);  // the entry and the index the read left under it
    if (!declaration) {
      be_raise(vm, "type_error", "SettingsEditScene: cfgs is a list of declarations");
    }
  }

  std::vector<gfx::ui::SettingsRow*> rows;
  rows.reserve((size_t)count);
  for (int i = 0; i < count; i++) {
    be_pushint(vm, i);
    be_getindex(vm, list);
    rows.push_back(borrowedRow(vm, -1));
    be_pop(vm, 2);
  }
  gfx::ui::SettingValues held;
  if (values) takeSettingMap(vm, 2, held);
  const String title = titled ? String(argText(vm, 3)) : String();
  std::unique_ptr<gfx::ui::SettingsScene> scene = std::make_unique<gfx::ui::SettingsScene>(
      routines->presses(), routines->scenes(), title, rows, held);
  be_pop(vm, be_top(vm) - base);
  pushSceneHandle(vm, std::move(scene));
  be_return(vm);
}

// The `settings` module: the four declaration classes, installed as members so a file reaches a
// kind through the module rather than through a global of its own, the record a routine's
// declarations are read back through, and the list a declaration list opens.
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

  installRecord(vm, SettingRecord, SettingFields);

  // The list a declaration list opens, a global because a page names it as one.
  be_pushntvfunction(vm, settingsEditScene);
  be_setglobal(vm, "SettingsEditScene");
  // be_setglobal leaves the value it stored on the stack; the function is done with here.
  be_pop(vm, 1);
}

}  // namespace berry
}  // namespace gardener
#endif
