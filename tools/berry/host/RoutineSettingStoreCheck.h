#pragma once

namespace gardener {

// RoutineSettingStore's own cases, over an in-memory image loaded from `dataDir`: printed as a suite's
// cases are, and answering whether every one passed.
bool runRoutineSettingStoreCases(const char* dataDir);

}  // namespace gardener
