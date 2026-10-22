#include "routines/Bindings.h"

#include <berry.h>

#include <cstring>

#include "Logger.h"
#include "PanelContext.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"

namespace gardener {
namespace berry {

// The pointers `bindings/Module.h` declares. install() is the one assignment.
PanelContext* gContext = nullptr;
Routines* gRoutines = nullptr;
Logger* gLogger = nullptr;

namespace {

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
    "class bounds\n"
    "  var w, h\n"
    "end\n"
    "class event\n"
    "  var btn, press\n"
    "end\n"
    // What a routine's describe() says the browser may edit. `values` is the cycle order for a
    // `choice`; the other kinds pass nil. `nullable` lets the value be nothing: the editor shows
    // `--` for it and steps to it below the kind's lowest value. The class is constructed by the
    // routine, so its fields stay plain data.
    "class setting\n"
    "  var key, label, kind, step, min, max, values, nullable\n"
    "  def init(key, label, kind, step, min, max, values, nullable)\n"
    "    self.key = key\n"
    "    self.label = label\n"
    "    self.kind = kind\n"
    "    self.step = step\n"
    "    self.min = min\n"
    "    self.max = max\n"
    "    self.values = values\n"
    "    self.nullable = nullable\n"
    "  end\n"
    "end\n";

}  // namespace

void install(bvm* vm, PanelContext& context, Routines& routines, Logger& logger) {
  gContext = &context;
  gRoutines = &routines;
  gLogger = &logger;

  installVacuumModule(vm);
  installScreenModule(vm);
  installDeviceModule(vm);
  installButtonsModule(vm);
  installBuzzerModule(vm);
  installJinglesModule(vm, context.library());
  installRoutinesModule(vm);
  installStoreModule(vm);

  // The records are classes, not native functions, so they are compiled rather than installed from
  // a table. Before any routine file runs, so every call that answers a record has one to answer.
  const int base = be_top(vm);
  int rc = be_loadbuffer(vm, "records", kRecords, strlen(kRecords));
  if (rc == BE_OK) rc = be_pcall(vm, 0);
  if (rc != BE_OK) {
    // Without the parser's own line a failure here is unfindable: every record class is gone and the
    // files that name one fail after it, with a message about the name rather than about this.
    String message = "vm: records did not compile";
    if (be_isstring(vm, -1)) {
      message += ": ";
      message += be_tostring(vm, -1);
    } else if (be_isstring(vm, -2)) {
      message += ": ";
      message += be_tostring(vm, -2);
    }
    logger.error(message.c_str());
  }
  be_pop(vm, be_top(vm) - base);
}

}  // namespace berry
}  // namespace gardener
