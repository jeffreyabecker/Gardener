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
// The settings module's declarations, the rows a settings list declares, each one consumed: a row
// belongs to the one record that takes it. Installs the record a declaration is read back through.
void installSettingsModule(bvm* vm);
// The rows a settings list declares, in order, each declaration consumed: a row belongs to the one
// record that takes it. Raises, leaving every declaration as it was, for a list holding anything
// else.
std::vector<std::unique_ptr<gfx::ui::SettingsRow>> takeSettingsRows(bvm* vm, int index);
// The declaration a row was built from, as the record a page reads it back through: the row, held by
// pointer, with the key, label, kind and the step, bounds and entries that kind was declared with.
// Left on the stack for the caller to push onto a list.
void pushSettingDeclaration(bvm* vm, gfx::ui::SettingsRow* row);
// The module a routine file registers itself through: register(key, title, icon, settings,
// scene_class) hands the registry a record with the page it names, push(key, callback, activation)
// builds that page from the activation and puts it on the stack, load_settings and save_settings
// read and write the routine's set, rows and title answer the browser's list, and settings_cfgs the
// declarations its settings list is built from.
void installRoutinesModule(bvm* vm);

}  // namespace berry
}  // namespace gardener
