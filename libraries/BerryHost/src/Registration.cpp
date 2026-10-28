#include "Build.h"

#if GARDENER_PANEL

#include <stdio.h>

#include <memory>
#include <vector>

#include <Arduino.h>
#include <berry.h>

#include <Gfx.h>

#include "routines/Registration.h"
#include "Module.h"
#include "Modules.h"

namespace gardener {
namespace berry {
namespace {

// The record a routine's file declares, held by the instance the file made: the comobj owns it and
// the interpreter frees it with the instance. Nothing reads it yet — `register` is what moves it
// into the registry.
using RecordOwner = std::unique_ptr<gardener::Registration>;

int recordRelease(bvm* vm) {
  delete static_cast<RecordOwner*>(be_tocomptr(vm, 1));
  be_return_nil(vm);
}

// The messages below are stack buffers because be_raise longjmps past any C++ destructor a String
// would need.

// The name the registry keeps for a routine and the label the browser draws. A title a file leaves
// out is the registry's to fall back on, so nothing is substituted here.
void requireText(bvm* vm, int index, const char* what) {
  if (!be_isstring(vm, index)) {
    char message[64];
    snprintf(message, sizeof(message), "Registration: %s expected", what);
    be_raise(vm, "type_error", message);
  }
}

// The packed bitmap a record carries: the width, the height, then one row per byte-run, most
// significant bit first and each row padded to a whole byte, which is the value screen.icon draws.
// Nil is a record that names no glyph; a bitmap that cannot supply the rows it declares is refused
// here rather than read past its end when a browser row is drawn. Nothing is copied yet, so a
// refused record leaves nothing behind.
const uint8_t* checkGlyph(bvm* vm, int index, size_t& length) {
  length = 0;
  if (be_isnil(vm, index)) return nullptr;
  if (!be_isbytes(vm, index)) {
    be_raise(vm, "type_error", "Registration: the glyph is the width, the height and the rows");
  }
  const uint8_t* bits = (const uint8_t*)be_tobytes(vm, index, &length);
  if (bits == nullptr || length < 2 || bits[0] == 0 || bits[1] == 0 ||
      length < 2 + (size_t)((bits[0] + 7) / 8) * bits[1]) {
    be_raise(vm, "value_error", "Registration: the glyph is the width, the height and the rows");
  }
  return bits;
}

// The glyph a record keeps, from a bitmap checkGlyph has passed. A glyph outlives the call that
// declared it and the interpreter frees the file's bytes with its values, so the bitmap is copied
// out and the paint closes over the copy.
gfx::ui::Glyph copyGlyph(const uint8_t* bits, size_t length) {
  if (bits == nullptr) return nullptr;
  auto kept = std::make_shared<std::vector<uint8_t>>(bits, bits + length);
  return [kept](gfx::Rect box) {
    return gfx::ui::icon(box, [kept](gfx::Canvas& canvas) {
      canvas.draw(gfx::shapes::MonoBitmap(0, 0, kept->data(), gfx::ui::Ink));
    });
  };
}

// Registration(routine_key, title, glyph, settings): the record a routine file declares, built from
// the values it hands up. The settings list is the module's declarations, in the order the settings
// list shows, and each is consumed here, because a row belongs to the one record that took it. A
// list the file leaves out declares nothing. A second init hands over a fresh record and drops the
// first to the interpreter.
int registrationInit(bvm* vm) {
  if (be_top(vm) < 4) {
    be_raise(vm, "type_error", "Registration: init(routine_key, title, glyph, settings)");
  }
  requireText(vm, 2, "the routine key");
  requireText(vm, 3, "the title");
  // Both are checked before a row is taken or a byte copied, so a refused record changes nothing.
  size_t length = 0;
  const uint8_t* bits = checkGlyph(vm, 4, length);
  std::vector<std::unique_ptr<gfx::ui::SettingsRow>> rows;
  if (be_top(vm) >= 5) rows = takeSettingsRows(vm, 5);

  be_newcomobj(vm, new RecordOwner(std::make_unique<gardener::Registration>(
                            String(argText(vm, 2)), String(argText(vm, 3)), std::move(rows),
                            nullptr, copyGlyph(bits, length))),
               recordRelease);
  be_setmember(vm, 1, ".p");
  be_pop(vm, 1);
  be_return_nil(vm);
}

const bnfuncinfo RegistrationMembers[] = {
    {".p", nullptr},
    {"init", registrationInit},
    {nullptr, nullptr},
};

}  // namespace

// The class a routine file subclasses, installed beside the settings module: the record lives in
// the instance, so a file declares a routine without reaching the C++ object the registry will take.
void installRegistrationClass(bvm* vm) {
  be_pushclass(vm, "Registration", RegistrationMembers);
  be_setglobal(vm, "Registration");
  be_pop(vm, 1);
}

}  // namespace berry
}  // namespace gardener
#endif
