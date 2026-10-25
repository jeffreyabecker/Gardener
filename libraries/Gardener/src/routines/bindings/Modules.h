#pragma once

struct bvm;

namespace gardener {

class Library;

namespace berry {

// Each native module a routine sees, installed from its own file in this folder: the module's
// callbacks and its table are file-local, and only the installer is exported.
void installVacuumModule(bvm* vm);
void installScreenModule(bvm* vm);
void installDeviceModule(bvm* vm);
void installButtonsModule(bvm* vm);
void installBuzzerModule(bvm* vm);
// The jingle library is read once, when the module is installed, from the interpreter's library.
void installJinglesModule(bvm* vm, Library& library);
void installStoreModule(bvm* vm);

}  // namespace berry
}  // namespace gardener
