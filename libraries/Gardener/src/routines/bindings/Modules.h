#pragma once

struct bvm;

namespace gardener {

namespace berry {

// Each native module a routine sees, installed from its own file in this folder: the module's
// callbacks and its table are file-local, and only the installer is exported.
void installVacuumModule(bvm* vm);
void installDeviceModule(bvm* vm);
// The native Config class a routine can instantiate for its own INI document.
void installConfigClass(bvm* vm);
// Answers an instance of that class over `document`, read once: the document and the name it was
// opened with belong to the instance. The device's `config` call answers this.
void pushConfigDocument(bvm* vm, const char* document);

}  // namespace berry
}  // namespace gardener
