#include "TextStream.h"

namespace gardener::host {

void TextStream::type(const char* text) {
  if (text != nullptr) typed_ += text;
}

void TextStream::type(const std::string& text) { typed_ += text; }

int TextStream::read() {
  if (readAt_ >= typed_.size()) return -1;
  return static_cast<unsigned char>(typed_[readAt_++]);
}

int TextStream::peek() {
  if (readAt_ >= typed_.size()) return -1;
  return static_cast<unsigned char>(typed_[readAt_]);
}

size_t TextStream::write(uint8_t byte) {
  printed_ += static_cast<char>(byte);
  return 1;
}

}  // namespace gardener::host
