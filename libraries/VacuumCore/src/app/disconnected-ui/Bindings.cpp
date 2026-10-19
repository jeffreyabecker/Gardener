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

// --- filling a map ---------------------------------------------------------
//
// be_setmember does not accept a map, so the entries go in through the index path: key at -2,
// value at -1, then both are popped.

void setReal(bvm* vm, int map, const char* key, breal value) {
  be_pushstring(vm, key);
  be_pushreal(vm, value);
  be_setindex(vm, map);
  be_pop(vm, 2);
}

void setInt(bvm* vm, int map, const char* key, bint value) {
  be_pushstring(vm, key);
  be_pushint(vm, value);
  be_setindex(vm, map);
  be_pop(vm, 2);
}

void setText(bvm* vm, int map, const char* key, const char* value) {
  be_pushstring(vm, key);
  be_pushstring(vm, value);
  be_setindex(vm, map);
  be_pop(vm, 2);
}

void setFlag(bvm* vm, int map, const char* key, bool value) {
  be_pushstring(vm, key);
  be_pushbool(vm, value);
  be_setindex(vm, map);
  be_pop(vm, 2);
}

int beginMap(bvm* vm) {
  be_newmap(vm);
  return be_absindex(vm, -1);
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
  const int map = beginMap(vm);
  setReal(vm, map, "kpa", system.currentKpa());
  setReal(vm, map, "target", system.targetKpa());
  setReal(vm, map, "duty", system.pump().dutyPercent());
  setReal(vm, map, "floor", cfg::CONTROL_FLOOR_KPA);
  setReal(vm, map, "min", cfg::CONTROL_MIN_TARGET_KPA);
  setText(vm, map, "state", runStateName(system.runState()));
  setText(vm, map, "fault", system.faultCode());
  // -1 rather than 0 when no gauge is fitted, so "no battery" is not "flat battery".
  setInt(vm, map, "batt", battery.present() ? (bint)battery.percent() : (bint)-1);
  setFlag(vm, map, "sensor_ok", system.sensor().present() && system.sensor().readingValid());
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
    {"target", vacuumTarget},   {"target_off", vacuumTargetOff}, {"start", vacuumStart},
    {"stop", vacuumStop},       {"reset", vacuumReset},          {"state", vacuumState},
    {"now_ms", vacuumNowMs},    {"log", vacuumLog},              {nullptr, nullptr},
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
  const int map = beginMap(vm);
  setText(vm, map, "name", info.name);
  setText(vm, map, "suffix", info.suffix);
  setReal(vm, map, "per_kpa", info.perKpa);
  setReal(vm, map, "whole_step", info.wholeStep);
  setInt(vm, map, "decimals", (bint)displayDecimals(unit));
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

  const int map = beginMap(vm);
  setText(vm, map, "btn", buttonName(button));
  setText(vm, map, "press", pressName(press));
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
  be_newlist(vm);
  const int list = be_absindex(vm, -1);
  for (uint8_t i = 0; i < gRoutines->count(); i++) {
    be_pushstring(vm, gRoutines->nameAt(i));
    be_data_push(vm, list);
    be_pop(vm, 1);
  }
  be_return(vm);
}

const bnfuncinfo kRoutinesLibrary[] = {
    {"register", routinesRegister},
    {"set_active", routinesSetActive},
    {"names", routinesNames},
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
}

}  // namespace berry
}  // namespace gardener
