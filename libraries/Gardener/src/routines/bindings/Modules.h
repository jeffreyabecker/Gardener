#pragma once

#include <any>
#include <memory>
#include <vector>

#include <berry.h>

#include "panel/page/SettingsScene.h"

namespace gardener {

namespace panel::page {
class SettingsRow;
}  // namespace panel::page

namespace berry {

// Each native module a routine sees, installed from its own file in this folder: the module's
// callbacks and its table are file-local, and only the installer is exported.
void installVacuumModule(bvm* vm);
void installScreenModule(bvm* vm);
void installDeviceModule(bvm* vm);
// The stack a page moves through, from Berry: push() a page over the one showing, pop() the top one
// and pop_to_root() every page above the root. Installs the record a native factory carries a scene
// back to the image in.
void installUxModule(bvm* vm);
// The settings module's declarations, the rows a settings list declares, each one consumed: a row
// belongs to the one record that takes it. Installs the record a declaration is read back through,
// and the SettingsEditScene the image opens a declaration list with.
void installSettingsModule(bvm* vm);
// The rows a settings list declares, in order, each declaration consumed: a row belongs to the one
// record that takes it. Raises, leaving every declaration as it was, for a list holding anything
// else.
std::vector<std::unique_ptr<panel::page::SettingsRow>> takeSettingsRows(bvm* vm, int index);
// The declaration a row was built from, as the record a page reads it back through: the row, held by
// pointer, with the key, label, kind and the step, bounds and entries that kind was declared with.
// Left on the stack for the caller to push onto a list.
void pushSettingDeclaration(bvm* vm, panel::page::SettingsRow* row);
// One value in the type a setting holds it in: nil for nothing, a flag, a whole number, a real or a
// key. What a scene answers when it pops is read the same way.
void pushSettingValue(bvm* vm, const std::any& value);
// A set as a map the image reads: one entry per key, in the order the map answered them.
void pushSettingsMap(bvm* vm, const panel::page::SettingValues& values);
// A map as a set: every key that holds a value a setting can hold. False for a value that is not a
// map, which the caller refuses.
bool takeSettingMap(bvm* vm, int index, panel::page::SettingValues& values);
// A scene a native factory built, carried into the image as an instance the one field holds. The
// stack owns the scene once it is pushed; a handle that is never pushed frees it with the
// collector, and one pushed twice has nothing to give the second time.
void pushSceneHandle(bvm* vm, std::unique_ptr<panel::page::Scene> scene);
// The module a routine file registers itself through: register(key, title, icon, settings,
// scene_class) hands the registry a record with the class its page is built from, activate(key,
// values) answers the page a launch builds, load_settings and save_settings read and write the
// routine's set, rows and title answer the browser's list, and settings_cfgs the declarations its
// settings list is built from.
void installRoutinesModule(bvm* vm);

}  // namespace berry
}  // namespace gardener
