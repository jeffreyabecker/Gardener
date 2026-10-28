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
// belongs to the one record that takes it.
void installSettingsModule(bvm* vm);
// The rows a settings list declares, in order, each declaration consumed: a row belongs to the one
// record that takes it. Raises, leaving every declaration as it was, for a list holding anything
// else.
std::vector<std::unique_ptr<gfx::ui::SettingsRow>> takeSettingsRows(bvm* vm, int index);
// The module a routine file registers itself through: register(key, title, icon, settings,
// scene_class) hands the registry a record with the page it names, and load_settings and
// save_settings read and write the routine's set.
void installRoutinesModule(bvm* vm);

}  // namespace berry
}  // namespace gardener
