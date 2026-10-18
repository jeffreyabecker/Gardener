#include "app/Settings.h"

namespace gardener {

DisplayUnit Settings::unit() const { return unit_; }

// No store yet, so a write is applied but not persisted: it holds until the next restart.
void Settings::setUnit(DisplayUnit unit) { unit_ = unit; }

}  // namespace gardener
