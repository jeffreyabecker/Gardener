#pragma once

namespace gardener {

// The machine's build-time knobs, handed to it by the composition root. The
// full device's root reads them from settings.ini; a machine-alone build hands
// constants. The machine reads these, never the settings file, so storage can
// move to the panel without the machine following it.
struct MachineConfig {
  bool bluetooth = true;    // whether the radio may advertise
  bool diagnostics = true;  // whether the diagnostics stream runs
  bool battery = true;      // whether a battery gauge is fitted
};

}  // namespace gardener
