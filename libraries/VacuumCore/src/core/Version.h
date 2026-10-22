#pragma once

#include <stdint.h>

namespace gardener {
namespace cfg {

// What this build is. Both links serve it, so a controller can tell one revision from another
// without opening the sketch.
constexpr const char* FIRMWARE_VERSION = "0.7.0";
// The wire revision, reported by `device get`. The wire is still unsettled, so the number stays 1
// and does not move per revision; a controller must not treat it as a compatibility guarantee.
constexpr uint32_t PROTOCOL_VERSION = 1;

}  // namespace cfg
}  // namespace gardener
