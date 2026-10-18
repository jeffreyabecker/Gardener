#pragma once

#include "app/AppContext.h"
#include "app/Overlay.h"
#include "app/VacuumSystem.h"
#include "hal/impl/AdcBattery.h"
#include "hal/impl/AdafruitDisplay.h"
#include "hal/impl/Buttons.h"
#include "hal/impl/MprlsSensor.h"
#include "hal/impl/PwmPump.h"
#include "hal/impl/SolenoidValve.h"

namespace gardener {

// Builds and owns the whole hardware graph, and exposes it as interfaces. This is the only place
// outside the drivers that names a fitted part or includes a vendor library.
class ContextFactory {
 public:
  ContextFactory();

  AppContext& context() { return context_; }

 private:
  MprlsSensor sensor_;
  PwmPump pump_;
  NormallyOpenValve vent_;
  AdcBattery battery_;
  Buttons buttons_;
  AdafruitDisplay display_;
  Overlay overlay_;
  VacuumSystem system_;
  AppContext context_;
};

}  // namespace gardener
