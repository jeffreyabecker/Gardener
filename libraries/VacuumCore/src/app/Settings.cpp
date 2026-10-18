#include "app/Settings.h"

namespace gardener {

DisplayUnit Settings::unit() const { return unit_; }

// No store yet, so a write is accepted and dropped. The reply echoes the field afterwards, so it
// answers the unit the device still prints rather than the one the host asked for.
void Settings::setUnit(DisplayUnit unit) { (void)unit; }

}  // namespace gardener
