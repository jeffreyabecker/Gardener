#pragma once

struct bvm;
class FS;

namespace gardener {
namespace berry {

// Each native module a routine sees, installed from its own file in this folder: the module's
// callbacks and its table are file-local, and only the installer is exported.
void installVacuumModule(bvm* vm);
void installScreenModule(bvm* vm);
void installDeviceModule(bvm* vm);
void installButtonsModule(bvm* vm);
void installBuzzerModule(bvm* vm);
// The jingle library is read once, when the module is installed, from the filesystem the panel was
// given.
void installJinglesModule(bvm* vm, FS& fs);
void installRoutinesModule(bvm* vm);
void installStoreModule(bvm* vm);

}  // namespace berry
}  // namespace gardener
