#pragma once

struct bvm;

namespace gardener {

namespace berry {

// Each native module a routine sees, installed from its own file in this folder: the module's
// callbacks and its table are file-local, and only the installer is exported.
void installVacuumModule(bvm* vm);
void installScreenModule(bvm* vm);
void installDeviceModule(bvm* vm);
void installButtonsModule(bvm* vm);
void installBuzzerModule(bvm* vm);
// The jingle library is taken once, when the module is installed, from the document the panel read.
void installJinglesModule(bvm* vm);
void installStoreModule(bvm* vm);
// The native Config class a routine can instantiate for its own INI document.
void installConfigClass(bvm* vm);

}  // namespace berry
}  // namespace gardener
