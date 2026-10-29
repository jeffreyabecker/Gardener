#include "Build.h"

#if GARDENER_PANEL

#include <stdio.h>

#include <any>
#include <memory>
#include <utility>
#include <vector>

#include <Arduino.h>
#include <berry.h>

#include <Gfx.h>

#include "BerryScene.h"
#include "core/Logger.h"
#include "panel/PanelContext.h"
#include "routines/Registration.h"
#include "routines/RoutineRegistry.h"
#include "routines/Routines.h"
#include "Module.h"
#include "Modules.h"

namespace gardener {
namespace berry {
namespace {

// The list the registrations' classes are held in, installed empty and appended to as they register.
constexpr const char* PageClasses = "__routineClasses";

// The record a launch hands a page's init, installed by this module.
constexpr const char* ActivationRecord = "RoutineActivation";

// The record one of the browser's rows is, installed here too: the glyph, the title and the key.
constexpr const char* RowRecord = "RoutineRow";

// `RoutineActivation(routine_key, settings_values)`: the record `routines.activate` builds for the
// page it answers, and the one an image builds for a page it launches itself.
int activationInit(bvm* vm) {
  if (be_top(vm) >= 2) setValue(vm, 1, "routine_key", 2);
  if (be_top(vm) >= 3) setValue(vm, 1, "settings_values", 3);
  be_return_nil(vm);
}

// The two fields a launch carries: the key the routine was launched under, which the page takes as its
// own, and the clean settings map its init_from_settings() reads. A page's init reads those two and
// keeps no record of its own.
const bnfuncinfo ActivationFields[] = {
    {"init", activationInit},
    {"routine_key", nullptr},
    {"settings_values", nullptr},
    {nullptr, nullptr},
};

// The three fields one row of the browser's list carries.
const bnfuncinfo RowFields[] = {
    {"init", recordInit},
    {"icon", nullptr},
    {"title", nullptr},
    {"value", nullptr},
    {nullptr, nullptr},
};

// The messages below are stack buffers because be_raise longjmps past any C++ destructor a String
// would need, and every check runs before the first String is built.

void requireText(bvm* vm, int index, const char* what) {
  if (be_top(vm) < index || !be_isstring(vm, index)) {
    char message[80];
    snprintf(message, sizeof(message), "routines.register: %s expected", what);
    be_raise(vm, "type_error", message);
  }
}

// The hex digits that name the routine's packed bitmap, which is the value `screen.icon` takes. Nil is
// a record that names no glyph and is let through; a value that is not a glyph is refused here rather
// than drawn as nothing when a browser row is drawn.
void requireGlyph(bvm* vm, int index) {
  if (be_top(vm) < index || be_isnil(vm, index)) return;
  if (!be_isstring(vm, index)) {
    be_raise(vm, "type_error", "routines.register: the icon is a string of hex digits");
  }
  if (!gfx::shapes::isMonoBitmap(argText(vm, index))) {
    be_raise(vm, "value_error", "routines.register: the icon is the width, the height and the rows");
  }
}

// A routine's page is a class the file declares, and a launch builds an instance of it: anything else
// is a registration that would answer no page.
void requireClass(bvm* vm, int index) {
  if (be_top(vm) < index || !be_isclass(vm, index)) {
    be_raise(vm, "type_error", "routines.register: the page is a class");
  }
}

// The class's slot in the list above, appended so a later registration never moves another's.
int holdClass(bvm* vm, int index) {
  const int klass = be_absindex(vm, index);
  if (!be_getglobal(vm, PageClasses) || !be_islist(vm, -1)) {
    be_pop(vm, 1);
    return -1;
  }
  const int list = be_absindex(vm, -1);
  be_pushvalue(vm, klass);
  be_data_push(vm, list);
  const int slot = be_data_size(vm, list) - 1;
  // be_data_push copies into the list and leaves the value where it was, so the class and the list
  // are undone here.
  be_pop(vm, 2);
  return slot;
}

// Whether the value at `index` is a map a file can build: an instance of the map class, which is what
// `{...}` makes, or the interpreter's own map. The map itself is left on the stack.
bool pushMap(bvm* vm, int index) {
  if (be_ismap(vm, index)) {
    be_pushvalue(vm, index);
    return true;
  }
  if (!be_ismapinstance(vm, index)) return false;
  be_getmember(vm, index, ".p");
  if (be_ismap(vm, -1)) return true;
  be_pop(vm, 1);
  return false;
}

// The value at `index` as a set carries it: a flag, a number, a key, or nothing. False for a value no
// setting can hold, which the caller refuses by name rather than casting.
bool settingValue(bvm* vm, int index, std::any& value) {
  value = std::any();
  if (be_isnil(vm, index)) return true;
  if (be_isbool(vm, index)) {
    value = std::any(be_tobool(vm, index) != 0);
    return true;
  }
  if (be_isstring(vm, index)) {
    value = std::any(String(argText(vm, index)));
    return true;
  }
  if (be_isint(vm, index)) {
    value = std::any((int32_t)be_toint(vm, index));
    return true;
  }
  if (be_isreal(vm, index)) {
    value = std::any((float)be_toreal(vm, index));
    return true;
  }
  return false;
}

}  // namespace

// A setting's value as a map answers it: the type its row holds it in, and nil for nothing.
void pushSettingValue(bvm* vm, const std::any& value) {
  if (!value.has_value()) {
    be_pushnil(vm);
    return;
  }
  if (const bool* flag = std::any_cast<bool>(&value)) {
    be_pushbool(vm, *flag);
    return;
  }
  if (const String* text = std::any_cast<String>(&value)) {
    be_pushstring(vm, text->c_str());
    return;
  }
  if (const int32_t* whole = std::any_cast<int32_t>(&value)) {
    be_pushint(vm, *whole);
    return;
  }
  if (const float* real = std::any_cast<float>(&value)) {
    be_pushreal(vm, *real);
    return;
  }
  be_pushnil(vm);
}

// A set as a map the page reads: one entry per key, in the order the store answered them. The map is an
// instance of the map class — the braced literal a routine file writes, which indexes through the
// class's item — rather than the interpreter's raw container, which no page can subscript.
void pushSettingsMap(bvm* vm, const panel::page::SettingValues& values) {
  // The map class's instance with the raw container it wraps on top of it, as be_newobject leaves an
  // instance: the entries go into the container and the pop below leaves the instance as the answer.
  be_newobject(vm, "map");
  const int raw = be_absindex(vm, -1);
  for (const std::pair<const String, std::any>& held : values) {
    be_pushstring(vm, held.first.c_str());
    pushSettingValue(vm, held.second);
    // be_data_insert takes the pair this call left on the stack and leaves both where they are, so
    // the push is undone here.
    be_data_insert(vm, raw);
    be_pop(vm, 2);
  }
  be_pop(vm, 1);
}

// A map as a set: every key that holds a value a setting can hold, as the type it was written as. A
// value no setting holds is left out, which is how a refused value falls back to the row's default.
bool takeSettingMap(bvm* vm, int index, panel::page::SettingValues& values) {
  const int at = be_absindex(vm, index);
  const int base = be_top(vm);
  const bool map = pushMap(vm, at);
  if (map) {
    // The map the entries are read from; the iterator the API walks it with is the value on top of the
    // stack, which is why it is the map, not that value, that is handed to the two calls below.
    const int source = be_absindex(vm, -1);
    be_pushiter(vm, source);
    while (be_iter_hasnext(vm, source)) {
      be_iter_next(vm, source);
      std::any value;
      if (be_isstring(vm, -2) && settingValue(vm, -1, value)) {
        values[String(argText(vm, -2))] = std::move(value);
      }
      be_pop(vm, 2);  // the key and the value, now that the entry is taken
    }
  }
  be_pop(vm, be_top(vm) - base);
  return map;
}

namespace {

// One row of the browser's list, left on the stack as the record itself so the caller can push it
// onto a list: the glyph the routine's file gave or nothing when it gave none, the title the row
// shows and the key a launch is made from.
void pushRow(bvm* vm, const RoutineRow& row) {
  const int record = beginRecord(vm, RowRecord);
  if (row.glyph.isEmpty()) {
    be_pushnil(vm);
    be_setmember(vm, record, "icon");
    be_pop(vm, 1);
  } else {
    setText(vm, record, "icon", row.glyph.c_str());
  }
  setText(vm, record, "title", row.title.c_str());
  setText(vm, record, "value", row.routineKey.c_str());
}

// The activation a page's init reads, built from the record this module installed. Its init assigns the
// two fields and cannot raise, so nothing here needs a protected call.
void pushActivation(bvm* vm, const RoutineActivation& activation) {
  be_getglobal(vm, ActivationRecord);
  be_pushstring(vm, activation.routineKey.c_str());
  pushSettingsMap(vm, activation.values);
  be_call(vm, 2);
  settleClassCall(vm, 2);
}

// The page a registration names: an instance of the class the file gave, built from the activation and
// wrapped as the scene the stack drives. Nothing, with one line, for a page whose init raised — the
// class is the file's and may refuse the set it is handed.
std::unique_ptr<panel::page::Scene> buildPage(bvm* vm, int slot,
                                              const RoutineActivation& activation) {
  if (vm == nullptr || context == nullptr || routines == nullptr || slot < 0) return nullptr;

  const int base = be_top(vm);
  std::unique_ptr<panel::page::Scene> page;
  if (be_getglobal(vm, PageClasses) && be_islist(vm, -1)) {
    const int list = be_absindex(vm, -1);
    be_pushint(vm, slot);
    be_getindex(vm, list);
    be_remove(vm, -2);  // the list the read left under the class

    if (be_isclass(vm, -1)) {
      pushActivation(vm, activation);
      // The class is the callable and the activation its argument; a page that raised leaves the
      // exception and its message where the pop below clears them.
      if (be_pcall(vm, 1) != BE_OK) {
        if (logger != nullptr) {
          String message("vm: routine ");
          message += activation.routineKey;
          message += ": ";
          message += be_isstring(vm, -2) ? be_tostring(vm, -2) : "error";
          message += ": ";
          const char* detail = be_tostring(vm, -1);
          message += detail != nullptr ? detail : "error";
          logger->error(message.c_str());
        }
      } else if (be_isinstance(vm, settleClassCall(vm, 1))) {
        page = std::make_unique<BerryScene>(routines->presses(), vm, be_absindex(vm, -1), *context);
      }
    }
  }
  be_pop(vm, be_top(vm) - base);
  return page;
}

// routines.register(routine_key, title, icon, settings, scene_class): the record a routine file
// declares, handed to the registry with the page it names. The settings list is the module's
// declarations, in the order the settings list shows, and each is consumed here, because a row belongs
// to the one record that took it; nil declares nothing.
int routinesRegister(bvm* vm) {
  if (context == nullptr || routines == nullptr) {
    be_raise(vm, "runtime_error", "routines.register: no panel");
  }
  if (be_top(vm) < 5) {
    be_raise(vm, "type_error",
             "routines.register: register(routine_key, title, icon, settings, scene_class)");
  }
  requireText(vm, 1, "the routine key");
  requireText(vm, 2, "the title");
  requireGlyph(vm, 3);
  requireClass(vm, 5);
  // Every check runs before a row is taken, so a refused registration changes nothing.
  std::vector<std::unique_ptr<panel::page::SettingsRow>> rows;
  if (!be_isnil(vm, 4)) rows = takeSettingsRows(vm, 4);

  const String routineKey(argText(vm, 1));
  const String title(argText(vm, 2));
  const String glyph = be_isnil(vm, 3) ? String() : String(argText(vm, 3));
  const int slot = holdClass(vm, 5);

  Registration record(routineKey, title, std::move(rows),
                      [vm, slot](const RoutineActivation& activation) {
                        return buildPage(vm, slot, activation);
                      },
                      glyph);
  be_pushbool(vm, routines->registry().add(std::move(record)));
  be_return(vm);
}

// routines.load_settings(routine_key): the set a routine begins with, as a map the page reads. A key
// that is not registered answers an empty map rather than raising: a page has an answer either way.
int routinesLoadSettings(bvm* vm) {
  if (routines == nullptr) {
    be_raise(vm, "runtime_error", "routines.load_settings: no panel");
  }
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "routines.load_settings: load_settings(routine_key)");
  }
  pushSettingsMap(vm, routines->registry().loadSettings(String(argText(vm, 1))));
  be_return(vm);
}

// routines.save_settings(routine_key, values): a settled set written back under the keys the record
// declares, once, and a key the map leaves out written as nothing.
int routinesSaveSettings(bvm* vm) {
  if (routines == nullptr) {
    be_raise(vm, "runtime_error", "routines.save_settings: no panel");
  }
  if (be_top(vm) < 2 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "routines.save_settings: save_settings(routine_key, values)");
  }
  const String routineKey(argText(vm, 1));
  if (!pushMap(vm, 2)) {
    be_raise(vm, "type_error", "routines.save_settings: values is a map");
  }
  const int map = be_absindex(vm, -1);

  const RoutineRegistry& registry = routines->registry();
  const std::vector<panel::page::SettingsRow*>* rows = registry.settingsRows(routineKey);
  panel::page::SettingValues values;
  if (rows != nullptr) {
    for (const panel::page::SettingsRow* row : *rows) {
      be_pushstring(vm, row->key().c_str());
      be_getindex(vm, map);
      be_remove(vm, -2);  // the key the read left under the value
      std::any value;
      if (!settingValue(vm, -1, value) || !row->accept(value)) {
        char message[96];
        snprintf(message, sizeof(message), "routines.save_settings: %s is not a value %s holds",
                 row->key().c_str(), row->label().c_str());
        be_raise(vm, "value_error", message);
      }
      be_pop(vm, 1);
      values[row->key()] = std::move(value);
    }
  }
  be_pushbool(vm, registry.saveSettings(routineKey, values));
  be_return(vm);
}

// routines.rows(): the browser's list, one row per record and per alternate in the order they
// registered.
int routinesRows(bvm* vm) {
  if (routines == nullptr) {
    be_raise(vm, "runtime_error", "routines.rows: no panel");
  }
  // The list class's instance with the raw list it appends to on top of it: the pop below leaves the
  // instance as the answer.
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (const RoutineRow& row : routines->registry().rows()) {
    pushRow(vm, row);
    be_data_push(vm, list);
    be_pop(vm, 1);
  }
  be_pop(vm, 1);
  be_return(vm);
}

// routines.title(routine_key): the label a page draws for a record or an alternate, and nil for a
// key nobody registered, which is what a page tests before it draws a heading.
int routinesTitle(bvm* vm) {
  if (routines == nullptr) {
    be_raise(vm, "runtime_error", "routines.title: no panel");
  }
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "routines.title: title(routine_key)");
  }
  const RoutineRegistry& registry = routines->registry();
  const String routineKey(argText(vm, 1));
  if (registry.entry(routineKey) == nullptr) be_return_nil(vm);
  be_pushstring(vm, registry.title(routineKey).c_str());
  be_return(vm);
}

// routines.settings_cfgs(routine_key): the declarations a record holds, in the order its settings
// list shows them, each as the record a page reads it back through. Nil for a key nobody registered
// and for a record that declares nothing, which is what a page tests before it opens a list; reading
// leaves the record's rows where they were, so a list opened twice edits the same declarations.
int routinesSettingsCfgs(bvm* vm) {
  if (routines == nullptr) {
    be_raise(vm, "runtime_error", "routines.settings_cfgs: no panel");
  }
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "routines.settings_cfgs: settings_cfgs(routine_key)");
  }
  const std::vector<panel::page::SettingsRow*>* rows =
      routines->registry().settingsRows(String(argText(vm, 1)));
  if (rows == nullptr || rows->empty()) be_return_nil(vm);

  // The list class's instance with the raw list it appends to on top of it: the pop below leaves the
  // instance as the answer.
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (panel::page::SettingsRow* row : *rows) {
    pushSettingDeclaration(vm, row);
    be_data_push(vm, list);
    be_pop(vm, 1);
  }
  be_pop(vm, 1);
  be_return(vm);
}

// routines.activate(routine_key, settings_values): the page a launch builds, an instance of the
// class its file registered, held in the handle a factory answers so ux.push can take it. The set
// the caller states is held to the record's declaration, so a setting it leaves out or a row
// refuses begins at the default. Nil, with the stack untouched, for a key nobody registered, a
// record that names no page and a page whose init raised.
int routinesActivate(bvm* vm) {
  if (routines == nullptr) {
    be_raise(vm, "runtime_error", "routines.activate: no panel");
  }
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "routines.activate: activate(routine_key, settings_values)");
  }
  if (be_top(vm) >= 2 && !be_isnil(vm, 2) && !be_ismap(vm, 2) && !be_ismapinstance(vm, 2)) {
    be_raise(vm, "type_error", "routines.activate: settings_values is a map");
  }
  // The checks are above: a raise below would longjmp past the key's String and the set.
  const String routineKey(argText(vm, 1));
  panel::page::SettingValues values;
  if (be_top(vm) >= 2 && !be_isnil(vm, 2)) takeSettingMap(vm, 2, values);
  std::unique_ptr<panel::page::Scene> page = routines->registry().activate(routineKey, values);
  if (!page) be_return_nil(vm);
  pushSceneHandle(vm, std::move(page));
  be_return(vm);
}

// routines.keys(): every registered key in registration order, each alternate after its record, as
// the list a driver walks and a summary counts the image by. An empty list, never nil, for an image
// that registered nothing.
int routinesKeys(bvm* vm) {
  if (routines == nullptr) {
    be_raise(vm, "runtime_error", "routines.keys: no panel");
  }
  // The list class's instance with the raw list it appends to on top of it: the pop below leaves the
  // instance as the answer.
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (const String& key : routines->registry().keys()) {
    be_pushstring(vm, key.c_str());
    be_data_push(vm, list);
    be_pop(vm, 1);
  }
  be_pop(vm, 1);
  be_return(vm);
}

const bnfuncinfo RoutinesLibrary[] = {
    {"register", routinesRegister},
    {"keys", routinesKeys},
    {"load_settings", routinesLoadSettings},
    {"save_settings", routinesSaveSettings},
    {"rows", routinesRows},
    {"title", routinesTitle},
    {"settings_cfgs", routinesSettingsCfgs},
    {"activate", routinesActivate},
    {nullptr, nullptr},
};

}  // namespace

// The module the image registers its routines through: one call to declare a record with the page
// it names, one to build that page for a launch, the two the page reads and writes its set through,
// and the three the browser lists them and edits them with.
void installRoutinesModule(bvm* vm) {
  installRecord(vm, ActivationRecord, ActivationFields);
  installRecord(vm, RowRecord, RowFields);
  installModule(vm, "routines", RoutinesLibrary);
  // The classes a registration names, in a list the interpreter owns for the life of the VM: the
  // registrations outlive the call that made them, the pages are built later, and a value a C++ object
  // holds is none of the collector's.
  be_newlist(vm);
  be_setglobal(vm, PageClasses);
  // be_setglobal leaves the value it stored on the stack; the list is done with here.
  be_pop(vm, 1);
}

}  // namespace berry
}  // namespace gardener
#endif
