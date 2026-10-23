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
// A routine reads a record with dots -- `s.pressure_kpa`, `ev.btn` -- and in this interpreter that works only
// on an instance whose class declares the field. Only the parser makes an instance variable, and
// upstream's own class tables come from a code generator this build does not run, so the record
// classes are declared once, here, in the language a routine is written in, and the firmware fills
// the instances it answers with. `setting` is the same idea for a routine's own settings: the
// settings list a routine registers is built from them.
//
// A record is the state at the moment of the call, which is what the map it replaced was: writing
// one of its fields changes the routine's copy and nothing else.

constexpr const char* kRecords =
    "class reading\n"
    "  var pressure_kpa, target, duty, floor, min, state, fault, batt, sensor_ok\n"
    "end\n"
    "class unit\n"
    "  var name, suffix, per_kpa, whole_step, decimals\n"
    "end\n"
    "class bounds\n"
    "  var w, h\n"
    "end\n"
    // What a routine registers as one of its editable values. The class is constructed by the
    // routine, so its fields stay plain data; `editor` is the kind's editor object, which owns how
    // the value is read, printed and edited; `default` is what it holds when nothing has been
    // stored, and `icon` is what the settings list draws at the left of the row, or nil.
    "class setting\n"
    "  var key, label, icon, editor, default\n"
    "  def init(key, label, editor, default, icon)\n"
    "    self.key = key\n"
    "    self.label = label\n"
    "    self.icon = icon\n"
    "    self.editor = editor\n"
    "    self.default = default\n"
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
  installUxModule(vm);
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
