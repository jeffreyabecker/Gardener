#pragma once

#include <stdint.h>

namespace gardener {

// The line grammar: how a command line becomes tokens, and how a token becomes a value.
//
// Every command shares this half, so it is the one part of the interface with more than one
// reader. It works in place — a token is a pointer into the caller's line, with a NUL written
// where its separator was — so parsing a command allocates nothing.

void lower(char* text);

bool same(const char* a, const char* b);

// Case insensitive, for a keyword whose value keeps its case on the wire: only a field name is
// folded before it is compared, because `link term` carries a marker that has to survive as typed.
bool sameFold(const char* a, const char* b);

// A flag value: `on` or `off`, either case. False for anything else, never a silent default.
bool parseFlag(const char* text, bool& out);

// An argument that does not parse in full is a bad_argument, never a silent zero.
bool parseFloat(const char* text, float& out);

// Splits `field=value` in place at the first '=' and hands back the value, or null when the
// token has no '=' at all — which is how a read is told from a write. The shape is all this
// reports: an empty value is there for the caller to refuse.
char* splitValue(char* token);

// Walks a command line one token at a time, NUL terminating as it goes, so a command takes exactly
// as many tokens as it needs and the layer above can refuse anything left over.
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

  // Everything left, leading spaces skipped: what follows the tokens a command consumed, or null
  // when the line is spent.
  char* rest() {
    while (*cursor_ == ' ' || *cursor_ == '\t') cursor_++;
    return *cursor_ == '\0' ? nullptr : cursor_;
  }

 private:
  char* cursor_;
};

}  // namespace gardener
