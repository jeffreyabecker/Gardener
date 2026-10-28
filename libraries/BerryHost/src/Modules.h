#pragma once

#include <memory>
#include <vector>

struct bvm;

namespace gardener {

namespace gfx {
namespace ui {
class SettingsRow;
}  // namespace ui
}  // namespace gfx

namespace berry {

// Each native module a routine sees, installed from its own file in this folder: the module's
// callbacks and its table are file-local, and only the installer is exported.
void installVacuumModule(bvm* vm);
void installScreenModule(bvm* vm);
void installDeviceModule(bvm* vm);
// The declarations a routine's settings list is made of: a class per kind, each call holding the
// row it built in the instance, so nothing about a setting is read back off a field.
void installSettingsModule(bvm* vm);
// The native Registration class a routine subclasses: the instance holds the record it declares,
// and init builds that record from the values the file hands up.
void installRegistrationClass(bvm* vm);
// The rows a settings list declares, in order, each declaration consumed: a row belongs to the one
// record that takes it. Raises, leaving every declaration as it was, for a list holding anything
// else.
std::vector<std::unique_ptr<gfx::ui::SettingsRow>> takeSettingsRows(bvm* vm, int index);

}  // namespace berry
}  // namespace gardener
