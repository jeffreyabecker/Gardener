#pragma once

#include <stddef.h>
#include <string.h>

#include <initializer_list>

namespace gardener {

// The standard presses the classifier reports, and the comparisons a page makes against them. A
// gesture is the whole string the panel answered - one key's press, or its parts joined by '+' in
// panel order - so a page names a constant here rather than repeating the words the wire log and the
// Berry pages also use.
namespace Gestures {

inline constexpr const char* UpSingle = "up:single";
inline constexpr const char* UpDouble = "up:double";
inline constexpr const char* DownSingle = "down:single";
inline constexpr const char* DownDouble = "down:double";
inline constexpr const char* ForwardSingle = "forward:single";
inline constexpr const char* ForwardDouble = "forward:double";
inline constexpr const char* BackSingle = "back:single";
inline constexpr const char* BackDouble = "back:double";
inline constexpr const char* OkSingle = "ok:single";
inline constexpr const char* OkDouble = "ok:double";
inline constexpr const char* CancelSingle = "cancel:single";
inline constexpr const char* CancelDouble = "cancel:double";

// True when the whole gesture is `expected`. A chord is not the same gesture as either of its
// parts, so a page that binds one names the whole word.
inline bool is(const char* gesture, const char* expected) {
  return gesture != nullptr && expected != nullptr && strcmp(gesture, expected) == 0;
}

// True when the gesture is one of `expected`: the two names a page treats alike, such as Back and
// Cancel both leaving a list.
inline bool isAny(const char* gesture, std::initializer_list<const char*> expected) {
  for (const char* candidate : expected) {
    if (is(gesture, candidate)) return true;
  }
  return false;
}

// True when one part of the gesture is `part`: `ok:single` is a part of `ok:single+cancel:5` and
// not of `ok:double`, and a partial word such as `ok` is no part at all.
inline bool hasPart(const char* gesture, const char* part) {
  if (gesture == nullptr || part == nullptr || part[0] == '\0') return false;
  const size_t length = strlen(part);
  for (const char* at = gesture; at != nullptr;) {
    if (strncmp(at, part, length) == 0 && (at[length] == '\0' || at[length] == '+')) return true;
    const char* plus = strchr(at, '+');
    at = plus != nullptr ? plus + 1 : nullptr;
  }
  return false;
}

}  // namespace Gestures

}  // namespace gardener
