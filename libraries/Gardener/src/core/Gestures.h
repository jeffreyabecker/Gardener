#pragma once

#include <stddef.h>
#include <string.h>

#include <initializer_list>
#include <vector>

namespace gardener {

// The standard presses the classifier reports, the words the panel's own holds are named with, and
// the comparisons a page makes against them. A gesture is the whole string the panel answered - one
// key's press, or its parts joined by '+' in panel order - so a page names a constant here rather
// than repeating the words the wire log and the Berry pages also use. The two system: words are not
// presses: ButtonsTick::begin() registers them as what a safety hold answers for.
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
inline constexpr const char* SystemReboot = "system:reboot";
inline constexpr const char* SystemQuit = "system:quit";

// Every word the panel answers under a name of its own. The intern table holds them from boot, so a
// press of one of them allocates nothing and only a chord or a hold mark is ever a new word.
inline constexpr const char* Known[] = {
    UpSingle,   UpDouble, DownSingle, DownDouble,   ForwardSingle, ForwardDouble, BackSingle,
    BackDouble, OkSingle, OkDouble,   CancelSingle, CancelDouble,  SystemReboot,  SystemQuit,
};

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

// One copy of each word the panel answers, made on first sight. A gesture is one of a handful of
// words rather than something a caller writes, so the table keeps the copy and a caller keeps the
// copy's address, which stays valid for as long as the panel runs.
class InternTable {
  // The one table; null until the first instance() makes it or a build sets its own.
  inline static InternTable* instance_ = nullptr;

 public:
  // The table every caller reaches: the panel's own, made on first use unless a build installed
  // one, whose insert takes the lock the panel's callers do not all share.
  static InternTable& instance() {
    if (instance_ == nullptr) {
      instance_ = new InternTable();
      instance_->seed();
    }
    return *instance_;
  }

  // Replaces the table, freeing the one it held, so a build installs its own before anything posts.
  static void setInstance(InternTable* table) {
    if (instance_ != nullptr) delete instance_;
    instance_ = table;
    if (instance_ != nullptr) instance_->seed();
  }
  // Frees the copies it made, and is virtual so a table a build installs is destroyed through this.
  virtual ~InternTable() {
    for (const char* entry : table_)
      delete[] entry;
  }

  // Two tables would free the same word twice, so neither is made from another.
  InternTable(const InternTable&) = delete;
  InternTable& operator=(const InternTable&) = delete;
  InternTable() = default;

  // The one copy of `gesture`, made when the table has not seen the word before. A build whose
  // callers are not all one task overrides this to hold a lock across the insert.
  virtual const char* intern(const char* gesture) {
    for (const char* entry : table_) {
      if (strcmp(entry, gesture) == 0) return entry;
    }

    const size_t length = strlen(gesture);
    char* copy = new char[length + 1];
    memcpy(copy, gesture, length + 1);

    table_.push_back(copy);
    return copy;
  }

 private:
  // The words `Gestures` names, held before anything is pressed. It goes through the virtual, so a
  // build's table seeds under its own lock.
  void seed() {
    for (const char* word : Known)
      intern(word);
  }

  std::vector<const char*> table_;
};

}  // namespace Gestures

}  // namespace gardener
