#pragma once

// The firmware's version, as one string: the boot line and `device get` both report it. A build
// sets it from the compiler rather than editing this file, so a release is a build flag rather
// than a commit here. The token is stringized, so the define needs no quoting:
//
//   --build-property "build.defines=-DGARDENER_VERSION=1.2.3"

#ifndef GARDENER_VERSION
#define GARDENER_VERSION 0.7.0
#endif

#define GARDENER_STRINGIFY_(token) #token
#define GARDENER_STRINGIFY(token) GARDENER_STRINGIFY_(token)

namespace gardener {
inline namespace core {

constexpr const char* FirmwareVersion = GARDENER_STRINGIFY(GARDENER_VERSION);

}  // namespace core
}  // namespace gardener
