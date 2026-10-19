#include "app/disconnected-ui/Bindings.h"

#include <berry.h>

#include "app/AppContext.h"
#include "app/StreamLogger.h"
#include "app/VacuumSystem.h"
#include "app/disconnected-ui/Routines.h"
#include "core/Config.h"
#include "core/Units.h"

namespace gardener {
namespace berry {
namespace {

AppContext* gContext = nullptr;
Routines* gRoutines = nullptr;
StreamLogger* gLogger = nullptr;

// --- building a module -----------------------------------------------------

void installModule(bvm* vm, const char* name, const bnfuncinfo* library) {
  be_newmodule(vm);
  const int module = be_absindex(vm, -1);
  for (const bnfuncinfo* entry = library; entry->name != nullptr; entry++) {
    be_pushntvfunction(vm, entry->function);
    // be_setmember copies into the module and leaves the value where it was, so the push is
    // undone here. Passing the absolute index keeps it valid across the push.
    be_setmember(vm, module, entry->name);
    be_pop(vm, 1);
  }
  be_setglobal(vm, name);
  // be_setglobal leaves the value it stored on the stack; the module is done with here.
  be_pop(vm, 1);
}

// --- the records -----------------------------------------------------------
//
// A routine reads a record with dots -- `s.kpa`, `ev.btn` -- and in this interpreter that works only
// on an instance whose class declares the field. Only the parser makes an instance variable, and
// upstream's own class tables come from a code generator this build does not run, so the record
// classes are declared once, here, in the language a routine is written in, and the firmware fills
// the instances it answers with. `setting` is the same idea for a routine's own describe(): the
// browser reads the fields the routine builds.
//
// A record is the state at the moment of the call, which is what the map it replaced was: writing
// one of its fields changes the routine's copy and nothing else.

constexpr const char* kRecords =
    "class reading\n"
    "  var kpa, target, duty, floor, min, state, fault, batt, sensor_ok\n"
    "end\n"
    "class unit\n"
    "  var name, suffix, per_kpa, whole_step, decimals\n"
    "end\n"
    "class event\n"
    "  var btn, press\n"
    "end\n"
    // What a routine's describe() says the browser may edit. `values` is the cycle order for a
    // `choice`; the other kinds pass nil. The class is constructed by the routine, so its fields
    // stay plain data.
    "class setting\n"
    "  var key, label, kind, step, min, max, values\n"
    "  def init(key, label, kind, step, min, max, values)\n"
    "    self.key = key\n"
    "    self.label = label\n"
    "    self.kind = kind\n"
    "    self.step = step\n"
    "    self.min = min\n"
    "    self.max = max\n"
    "    self.values = values\n"
    "  end\n"
    "end\n";

// Constructs one: the class value stays under the instance, so endRecord() removes the class and
// leaves the finished record for be_return().
int beginRecord(bvm* vm, const char* className) {
  if (!be_getglobal(vm, className)) return -1;
  be_call(vm, 0);
  return be_absindex(vm, -1);
}

void endRecord(bvm* vm) { be_remove(vm, -2); }

void setReal(bvm* vm, int record, const char* field, breal value) {
  be_pushreal(vm, value);
  be_setmember(vm, record, field);
  be_pop(vm, 1);
}

void setInt(bvm* vm, int record, const char* field, bint value) {
  be_pushint(vm, value);
  be_setmember(vm, record, field);
  be_pop(vm, 1);
}

void setText(bvm* vm, int record, const char* field, const char* value) {
  be_pushstring(vm, value);
  be_setmember(vm, record, field);
  be_pop(vm, 1);
}

void setFlag(bvm* vm, int record, const char* field, bool value) {
  be_pushbool(vm, value);
  be_setmember(vm, record, field);
  be_pop(vm, 1);
}

// --- argument helpers ------------------------------------------------------

breal argReal(bvm* vm, int index) { return be_toreal(vm, index); }
bint argInt(bvm* vm, int index) { return be_toint(vm, index); }
const char* argText(bvm* vm, int index) { return be_tostring(vm, index); }

// --- vacuum ----------------------------------------------------------------

int vacuumTarget(bvm* vm) {
  gContext->vacuum().setTarget((float)argReal(vm, 1));
  be_return_nil(vm);
}

int vacuumTargetOff(bvm* vm) {
  gContext->vacuum().clearTarget();
  be_return_nil(vm);
}

// The machine's own bounds, as two constants `state()` publishes: shallower than `min` is not a
// target and `floor` is the deepest worth asking for. Kept here so a page steps and restores a
// target without carrying its own copy of the bounds — and so one bound serves the editor, a
// stored value and anything else that asks.
int vacuumClamp(bvm* vm) {
  float kpa = (float)argReal(vm, 1);
  if (kpa < cfg::CONTROL_MIN_TARGET_KPA) {
    kpa = cfg::CONTROL_MIN_TARGET_KPA;
  } else if (kpa > cfg::CONTROL_FLOOR_KPA) {
    kpa = cfg::CONTROL_FLOOR_KPA;
  }
  be_pushreal(vm, kpa);
  be_return(vm);
}

int vacuumStart(bvm* vm) {
  be_pushbool(vm, gContext->vacuum().start());
  be_return(vm);
}

int vacuumStop(bvm* vm) {
  gContext->vacuum().stop();
  be_return_nil(vm);
}

int vacuumReset(bvm* vm) {
  gContext->vacuum().resetFault();
  be_return_nil(vm);
}

int vacuumState(bvm* vm) {
  VacuumSystem& system = gContext->vacuum();
  const Battery& battery = gContext->battery();
  const int record = beginRecord(vm, "reading");
  if (record < 0) be_return_nil(vm);
  setReal(vm, record, "kpa", system.currentKpa());
  setReal(vm, record, "target", system.targetKpa());
  setReal(vm, record, "duty", system.pump().dutyPercent());
  setReal(vm, record, "floor", cfg::CONTROL_FLOOR_KPA);
  setReal(vm, record, "min", cfg::CONTROL_MIN_TARGET_KPA);
  setText(vm, record, "state", runStateName(system.runState()));
  setText(vm, record, "fault", system.faultCode());
  // -1 rather than 0 when no gauge is fitted, so "no battery" is not "flat battery".
  setInt(vm, record, "batt", battery.present() ? (bint)battery.percent() : (bint)-1);
  setFlag(vm, record, "sensor_ok",
          system.sensor().present() && system.sensor().readingValid());
  endRecord(vm);
  be_return(vm);
}

int vacuumNowMs(bvm* vm) {
  be_pushint(vm, (bint)millis());
  be_return(vm);
}

int vacuumLog(bvm* vm) {
  if (be_top(vm) >= 1 && be_isstring(vm, 1) && gLogger != nullptr) {
    gLogger->info(argText(vm, 1));
  }
  be_return_nil(vm);
}

const bnfuncinfo kVacuumLibrary[] = {
    {"target", vacuumTarget},   {"target_off", vacuumTargetOff}, {"clamp", vacuumClamp},
    {"start", vacuumStart},     {"stop", vacuumStop},            {"reset", vacuumReset},
    {"state", vacuumState},     {"now_ms", vacuumNowMs},         {"log", vacuumLog},
    {nullptr, nullptr},
};

// --- screen ----------------------------------------------------------------

Display& panel() { return gContext->display(); }

int screenClear(bvm* vm) {
  panel().clear();
  be_return_nil(vm);
}

int screenFlush(bvm* vm) {
  panel().display();
  be_return_nil(vm);
}

int screenPixel(bvm* vm) {
  panel().pixel(argInt(vm, 1), argInt(vm, 2));
  be_return_nil(vm);
}

int screenLine(bvm* vm) {
  panel().line(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4));
  be_return_nil(vm);
}

int screenHLine(bvm* vm) {
  panel().hLine(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3));
  be_return_nil(vm);
}

int screenVLine(bvm* vm) {
  panel().vLine(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3));
  be_return_nil(vm);
}

int screenRect(bvm* vm) {
  panel().rect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4));
  be_return_nil(vm);
}

int screenFillRect(bvm* vm) {
  panel().fillRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4));
  be_return_nil(vm);
}

int screenRoundRect(bvm* vm) {
  panel().roundRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4), argInt(vm, 5));
  be_return_nil(vm);
}

int screenFillRoundRect(bvm* vm) {
  panel().fillRoundRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
                        argInt(vm, 5));
  be_return_nil(vm);
}

int screenCircle(bvm* vm) {
  panel().circle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3));
  be_return_nil(vm);
}

int screenFillCircle(bvm* vm) {
  panel().fillCircle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3));
  be_return_nil(vm);
}

int screenEllipse(bvm* vm) {
  panel().ellipse(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4));
  be_return_nil(vm);
}

int screenFillEllipse(bvm* vm) {
  panel().fillEllipse(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4));
  be_return_nil(vm);
}

int screenTriangle(bvm* vm) {
  panel().triangle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4), argInt(vm, 5),
                   argInt(vm, 6));
  be_return_nil(vm);
}

int screenFillTriangle(bvm* vm) {
  panel().fillTriangle(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4), argInt(vm, 5),
                       argInt(vm, 6));
  be_return_nil(vm);
}

int screenRotatedRect(bvm* vm) {
  panel().rotatedRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4), argInt(vm, 5));
  be_return_nil(vm);
}

int screenFillRotatedRect(bvm* vm) {
  panel().fillRotatedRect(argInt(vm, 1), argInt(vm, 2), argInt(vm, 3), argInt(vm, 4),
                          argInt(vm, 5));
  be_return_nil(vm);
}

int screenText(bvm* vm) {
  // Berry's order is the drawing's: x, y, size, inverted, text. The Display takes the string
  // before the size, so the last two arguments swap on the way through.
  panel().text(argInt(vm, 1), argInt(vm, 2), argText(vm, 5), (uint8_t)argInt(vm, 3),
               be_tobool(vm, 4));
  be_return_nil(vm);
}

// The unit the panel prints, as the same fields core/Units.h holds: a page formats and steps a
// target in the unit on the glass without carrying a table of its own.
int screenUnit(bvm* vm) {
  const DisplayUnit unit = gContext->settings().unit();
  const DisplayUnitInfo& info = displayUnitInfo(unit);
  const int record = beginRecord(vm, "unit");
  if (record < 0) be_return_nil(vm);
  setText(vm, record, "name", info.name);
  setText(vm, record, "suffix", info.suffix);
  setReal(vm, record, "per_kpa", info.perKpa);
  setReal(vm, record, "whole_step", info.wholeStep);
  setInt(vm, record, "decimals", (bint)displayDecimals(unit));
  endRecord(vm);
  be_return(vm);
}

const bnfuncinfo kScreenLibrary[] = {
    {"draw_clear", screenClear},
    {"draw_pixel", screenPixel},
    {"draw_line", screenLine},
    {"draw_hline", screenHLine},
    {"draw_vline", screenVLine},
    {"draw_rect", screenRect},
    {"draw_fillrect", screenFillRect},
    {"draw_roundrect", screenRoundRect},
    {"draw_fillroundrect", screenFillRoundRect},
    {"draw_circle", screenCircle},
    {"draw_fillcircle", screenFillCircle},
    {"draw_ellipse", screenEllipse},
    {"draw_fillellipse", screenFillEllipse},
    {"draw_triangle", screenTriangle},
    {"draw_filltriangle", screenFillTriangle},
    {"draw_rotatedrect", screenRotatedRect},
    {"draw_fillrotatedrect", screenFillRotatedRect},
    {"draw_text", screenText},
    {"flush", screenFlush},
    {"unit", screenUnit},
    {nullptr, nullptr},
};

// --- buttons ---------------------------------------------------------------

// A pull, not a callback: the queue is drained by the routine when it is ready, so no Berry code
// runs while a native call is on the stack.
int buttonsTake(bvm* vm) {
  Button button = Button::Up;
  Press press = Press::None;
  if (gRoutines == nullptr || !gRoutines->takePress(button, press)) be_return_nil(vm);

  const int record = beginRecord(vm, "event");
  if (record < 0) be_return_nil(vm);
  setText(vm, record, "btn", buttonName(button));
  setText(vm, record, "press", pressName(press));
  endRecord(vm);
  be_return(vm);
}

const bnfuncinfo kButtonsLibrary[] = {
    {"take", buttonsTake},
    {nullptr, nullptr},
};

// --- routines --------------------------------------------------------------

int routinesRegister(bvm* vm) {
  if (be_top(vm) < 3 || !be_isstring(vm, 1) || !be_isstring(vm, 2) || !be_isinstance(vm, 3)) {
    be_raise(vm, "type_error", "routines.register(name, title, routine)");
  }
  const bool ok = gRoutines->registerRoutine(argText(vm, 1), argText(vm, 2), 3);
  be_pushbool(vm, ok);
  be_return(vm);
}

int routinesSetActive(bvm* vm) {
  const bool ok = be_isstring(vm, 1) && gRoutines->setActive(argText(vm, 1));
  be_pushbool(vm, ok);
  be_return(vm);
}

int routinesNames(bvm* vm) {
  // A list a routine can ask the size of and index, which means an instance of the `list` class: the
  // entries go into the class's own backing list and that is dropped, exactly as for a record.
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (size_t i = 0; i < gRoutines->count(); i++) {
    be_pushstring(vm, gRoutines->nameAt(i));
    be_data_push(vm, list);
    be_pop(vm, 1);
  }
  be_pop(vm, 1);
  be_return(vm);
}

// The registered object itself, so a page can call one of its methods in its own frame. Returning
// the instance rather than calling describe() here is what keeps the runtime from re-entering the
// interpreter inside a native call.
int routinesGet(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1) || gRoutines == nullptr ||
      !gRoutines->pushObject(argText(vm, 1))) {
    be_return_nil(vm);
  }
  be_return(vm);
}

int routinesHas(bvm* vm) {
  if (be_top(vm) < 2 || !be_isstring(vm, 1) || !be_isstring(vm, 2)) {
    be_raise(vm, "type_error", "routines.has(name, method)");
  }
  const bool found = gRoutines != nullptr && gRoutines->hasMethod(argText(vm, 1), argText(vm, 2));
  be_pushbool(vm, found);
  be_return(vm);
}

const bnfuncinfo kRoutinesLibrary[] = {
    {"register", routinesRegister},
    {"set_active", routinesSetActive},
    {"names", routinesNames},
    {"get", routinesGet},
    {"has", routinesHas},
    {nullptr, nullptr},
};

// --- store -----------------------------------------------------------------
//
// The active routine's own values. The fallback is both the answer for a key nothing has written
// and the type the text is read back as, so a routine reads a number as a number and a missing key
// is not an error it has to test for.

int storeGet(bvm* vm) {
  const int top = be_top(vm);
  const bool named = top >= 3;
  const int keyIndex = named ? 2 : 1;
  const int fallbackIndex = named ? 3 : 2;

  if (top < 1 || !be_isstring(vm, 1) || !be_isstring(vm, keyIndex)) {
    be_raise(vm, "type_error", "store.get(key, fallback) or store.get(name, key, fallback)");
  }

  const bool hasFallback = top >= fallbackIndex;
  const bool fallbackInt = hasFallback && be_isint(vm, fallbackIndex);
  const bool fallbackReal = hasFallback && be_isreal(vm, fallbackIndex);
  const bool fallbackBool = hasFallback && be_isbool(vm, fallbackIndex);

  const char* routine = named ? argText(vm, 1) : nullptr;
  const char* key = argText(vm, keyIndex);
  String text;
  const bool stored =
      gRoutines != nullptr &&
      (named ? gRoutines->storeGet(routine, key, text) : gRoutines->storeGet(key, text));

  if (!stored) {
    // Nothing under the key: the fallback the caller passed is the answer, as it stands.
    if (hasFallback) {
      be_pushvalue(vm, fallbackIndex);
    } else {
      be_pushnil(vm);
    }
    be_return(vm);
  }

  if (fallbackInt) {
    be_pushint(vm, (bint)text.toInt());
  } else if (fallbackReal) {
    be_pushreal(vm, (breal)text.toFloat());
  } else if (fallbackBool) {
    be_pushbool(vm, text == "true" || text == "1");
  } else {
    be_pushstring(vm, text.c_str());
  }
  be_return(vm);
}

int storeSet(bvm* vm) {
  const int top = be_top(vm);
  const bool named = top >= 3;
  const int keyIndex = named ? 2 : 1;
  const int valueIndex = named ? 3 : 2;

  if (top < 2 || !be_isstring(vm, 1) || !be_isstring(vm, keyIndex)) {
    be_raise(vm, "type_error", "store.set(key, value) or store.set(name, key, value)");
  }

  const char* routine = named ? argText(vm, 1) : nullptr;
  const char* key = argText(vm, keyIndex);
  // be_tostring converts a number or a bool in place, so every scalar has the text form the file
  // holds. The pointer is into the VM's string table, which is why it is used before anything else
  // is pushed.
  const char* value = be_tostring(vm, valueIndex);
  const bool written =
      value != nullptr && gRoutines != nullptr &&
      (named ? gRoutines->storeSet(routine, key, String(value))
             : gRoutines->storeSet(key, String(value)));
  be_pushbool(vm, written);
  be_return(vm);
}

const bnfuncinfo kStoreLibrary[] = {
    {"get", storeGet},
    {"set", storeSet},
    {nullptr, nullptr},
};

}  // namespace

void install(bvm* vm, AppContext& context, Routines& routines, StreamLogger& logger) {
  gContext = &context;
  gRoutines = &routines;
  gLogger = &logger;

  installModule(vm, "vacuum", kVacuumLibrary);
  installModule(vm, "screen", kScreenLibrary);
  installModule(vm, "buttons", kButtonsLibrary);
  installModule(vm, "routines", kRoutinesLibrary);
  installModule(vm, "store", kStoreLibrary);

  // The records are classes, not native functions, so they are compiled rather than installed from
  // a table. Before any routine file runs, so every call that answers a record has one to answer.
  const int base = be_top(vm);
  if (be_loadbuffer(vm, "records", kRecords, sizeof(kRecords) - 1) != BE_OK ||
      be_pcall(vm, 0) != BE_OK) {
    logger.error("vm: records did not compile");
  }
  be_pop(vm, be_top(vm) - base);
}

}  // namespace berry
}  // namespace gardener
