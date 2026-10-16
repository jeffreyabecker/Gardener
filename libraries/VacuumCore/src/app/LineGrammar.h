#pragma once

#include <stddef.h>
#include <stdint.h>

namespace gardener {

// The line grammar: how a command line becomes tokens, and how a token becomes a number.
//
// Every command shares this half, so it is the one part of the interface with more than one
// reader. It works in place — a token is a pointer into the caller's line, with a NUL written
// where its separator was — so parsing a command allocates nothing.

// True when the line begins with `word` followed by a separator or the end, so `draw` is not
// confused with a verb that merely starts with those letters. Case insensitive, like the
// dispatcher.
bool startsWith(const char* line, const char* word);

// Splits in place: every token is NUL terminated, so the caller's buffer is the token
// storage and nothing is allocated.
size_t splitLine(char* line, char* argv[], size_t maxArgs);

void lower(char* text);

bool same(const char* a, const char* b);

// An argument that does not parse in full is a bad_argument, never a silent zero.
bool parseFloat(const char* text, float& out);

// A coordinate. Out of range is refused rather than wrapped: the library clips anything off the
// panel, but a coordinate that does not fit in an int16_t is a client bug worth telling about.
bool parseCoord(const char* text, int16_t& out);

// Walks a command line one token at a time, NUL terminating as it goes, so a command can take
// exactly as many tokens as it needs and then treat the rest of the line as text. `splitLine`
// cannot do that: it takes a fixed number of tokens up front, which would cut a drawn string at
// its first space.
class Tokens {
 public:
  explicit Tokens(char* line) : cursor_(line) {}

  char* next() {
    while (*cursor_ == ' ' || *cursor_ == '\t') cursor_++;
    if (*cursor_ == '\0') return nullptr;
    char* token = cursor_;
    while (*cursor_ != '\0' && *cursor_ != ' ' && *cursor_ != '\t') cursor_++;
    if (*cursor_ != '\0') *cursor_++ = '\0';
    return token;
  }

  // Everything left, leading spaces skipped: the string a `draw text` carries, or null when the
  // line is spent.
  char* rest() {
    while (*cursor_ == ' ' || *cursor_ == '\t') cursor_++;
    return *cursor_ == '\0' ? nullptr : cursor_;
  }

 private:
  char* cursor_;
};

}  // namespace gardener
