#include "CaptureDisplay.h"

#include <algorithm>
#include <string.h>

#include "gfx/BuiltinFont.h"
#include "gfx/Canvas.h"
#include "gfx/Text.h"

namespace gardener {

namespace {

std::string uppercase(std::string text) {
  for (char& c : text) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
  }
  return text;
}

bool isDigits(const std::string& text) {
  if (text.empty()) return false;
  for (char c : text) {
    if (c < '0' || c > '9') return false;
  }
  return true;
}

std::string keyLabel(const std::string& key) {
  if (key == "ok") return "OK";
  if (key == "cancel") return "CANCEL";
  if (key == "up") return "UP";
  if (key == "down") return "DOWN";
  if (key == "forward") return "FORWARD";
  if (key == "back") return "BACK";
  return uppercase(key);
}

// One part of a gesture, key:form: single keys name themselves, a double appends the times sign, a
// hold its seconds, and anything else falls back to the raw token uppercased.
std::string partLabel(const std::string& part) {
  const size_t colon = part.find(':');
  const std::string key = part.substr(0, colon);
  const std::string form = colon == std::string::npos ? "" : part.substr(colon + 1);

  std::string label = keyLabel(key);
  if (form.empty() || form == "single") {
    return label;
  }
  if (form == "double") {
    return label + "\xD7" "2";  // OK×2
  }
  if (isDigits(form)) {
    return label + " " + form + "s";  // CANCEL 5s
  }
  return uppercase(key + ":" + form);
}

// Whether the built-in face has no ink for a code, so a callout can fall back rather than print a
// blank where a glyph is missing. The classic face's high half is shifted by one, as Adafruit's
// printer shifts it.
bool glyphBlank(uint8_t c) {
  if (c < gfx::BuiltinFont.first || c > gfx::BuiltinFont.last) return true;
  const gfx::Glyph& glyph = gfx::BuiltinFont.glyph[c - gfx::BuiltinFont.first];
  const uint8_t* bitmap = gfx::BuiltinFont.bitmap;
  uint16_t offset = glyph.bitmapOffset;
  uint8_t bits = 0;
  uint8_t bit = 0;
  for (uint8_t row = 0; row < glyph.height; row++) {
    for (uint8_t column = 0; column < glyph.width; column++) {
      if ((bit++ & 7) == 0) bits = bitmap[offset++];
      if ((bits & 0x80) != 0) return false;
      bits = static_cast<uint8_t>(bits << 1);
    }
  }
  return true;
}

// What a gesture token renders as: ok:single -> OK, up:double -> OK×2, cancel:5 -> CANCEL 5s, and
// a sum joins its parts with " + ". Anything unrecognised comes back as the token uppercased.
std::string gestureLabel(const std::string& gesture) {
  std::string out;
  size_t start = 0;
  while (start <= gesture.size()) {
    size_t end = gesture.find('+', start);
    if (end == std::string::npos) end = gesture.size();
    if (!out.empty()) out += " + ";
    out += partLabel(gesture.substr(start, end - start));
    start = end + 1;
  }
  return out;
}

// Clears a full-width strip at the top of a packed frame, then centres the label in ink in the
// panel's own 5x7 face.
void drawGestureStrip(uint8_t* packed, int16_t width, int16_t height, const std::string& label) {
  const int16_t rowBytes = (int16_t)((width + 7) / 8);
  constexpr int16_t StripHeight = 10;  // one pad, the 8-row face, one pad
  const int16_t stripRows = StripHeight < height ? StripHeight : height;
  for (int16_t y = 0; y < stripRows; y++) {
    memset(packed + y * rowBytes, 0, (size_t)rowBytes);
  }

  const int16_t labelWidth = 6 * (int16_t)label.size();  // five columns plus the spacer
  int16_t x = (width - labelWidth) / 2;
  const int16_t y = 1;
  gfx::Canvas canvas(packed, gfx::Rect(0, 0, width, height), gfx::Rect(0, 0, width, height),
                     gfx::PixelLayout::Packed1);
  for (char c : label) {
    uint8_t code = static_cast<uint8_t>(c);
    if (code >= 176 && code < 255) code++;
    if (glyphBlank(code)) code = 'x';
    gfx::shapes::drawGlyph(canvas, x, y, code, 1, 255);
    x += 6;
  }
}

// Pixels between diagonal strokes: wide enough to read as a hash at the panel's size, dense enough
// that it cannot be mistaken for a drawn line.
constexpr int HashSpacing = 4;

void setPacked(uint8_t* packed, int16_t width, int16_t x, int16_t y) {
  packed[y * ((width + 7) / 8) + x / 8] |= (uint8_t)(0x80 >> (x & 7));
}

// Clears the marked rows, outlines them, and fills them with a diagonal ink hash.
void drawOverlayHash(uint8_t* packed, int16_t width, int16_t height, int16_t top, int16_t rows) {
  if (top < 0 || rows <= 0 || top >= height) return;
  const int16_t bottom = top + rows < height ? (int16_t)(top + rows) : height;
  const int16_t rowBytes = (int16_t)((width + 7) / 8);

  for (int16_t y = top; y < bottom; y++) {
    memset(packed + y * rowBytes, 0, (size_t)rowBytes);
  }

  for (int16_t x = 0; x < width; x++) {
    setPacked(packed, width, x, top);
    setPacked(packed, width, x, (int16_t)(bottom - 1));
  }
  for (int16_t y = top; y < bottom; y++) {
    setPacked(packed, width, 0, y);
    setPacked(packed, width, (int16_t)(width - 1), y);
    for (int16_t x = (int16_t)((y - top) % HashSpacing); x < width; x += HashSpacing) {
      setPacked(packed, width, x, y);
    }
  }
}

}  // namespace

CaptureDisplay::CaptureDisplay(int16_t width, int16_t height, uint8_t bitsPerPixel)
    : width_(width),
      height_(height),
      depth_(bitsPerPixel == 1 ? 1 : 8),
      stride_(bitsPerPixel == 1 ? (int16_t)((width + 7) / 8) : width),
      frame_(static_cast<size_t>(stride_) * static_cast<size_t>(height), 0),
      sent_(frame_.size(), 0) {}

bool CaptureDisplay::begin() {
  clear();
  return true;
}

void CaptureDisplay::flush(const gfx::Rect& dirty) {
  flushes_++;
  lastDirty_ = dirty;

  const int16_t firstRow = std::max<int16_t>(0, dirty.y);
  const int16_t lastRow = std::min<int16_t>((int16_t)(height_ - 1), dirty.bottom());
  const int16_t firstColumn = std::max<int16_t>(0, dirty.x);
  const int16_t lastColumn = std::min<int16_t>((int16_t)(width_ - 1), dirty.right());
  if (lastRow < firstRow || lastColumn < firstColumn) return;

  if (depth_ == 8) {
    const size_t columns = static_cast<size_t>(lastColumn - firstColumn + 1);
    for (int16_t row = firstRow; row <= lastRow; row++) {
      const size_t at = static_cast<size_t>(row) * stride_ + firstColumn;
      memcpy(sent_.data() + at, frame_.data() + at, columns);
    }
  } else {
    for (int16_t row = firstRow; row <= lastRow; row++) {
      for (int16_t column = firstColumn; column <= lastColumn; column++) {
        const size_t index = static_cast<size_t>(row) * stride_ + (column >> 3);
        const uint8_t mask = static_cast<uint8_t>(0x80 >> (column & 7));
        if ((frame_[index] & mask) != 0) {
          sent_[index] |= mask;
        } else {
          sent_[index] &= static_cast<uint8_t>(~mask);
        }
      }
    }
  }

  if (!capturing_) return;
  keepFrame();
}

void CaptureDisplay::keepFrame() {
  std::vector<uint8_t> packed(Capture::frameBytes(width_, height_));
  pack(packed.data());
  if (calloutFrames_ > 0 && !callout_.empty()) {
    drawGestureStrip(packed.data(), width_, height_, gestureLabel(callout_));
  }
  if (overlayTop_ >= 0) {
    drawOverlayHash(packed.data(), width_, height_, overlayTop_, overlayRows_);
  }
  capture_.add(packed.data());
  if (calloutFrames_ > 0) calloutFrames_--;
}

void CaptureDisplay::press(const char* gesture) {
  if (gesture == nullptr || gesture[0] == '\0') return;
  queue_.push_back(gesture);
  gestures_.push_back(gesture);
}

const char* CaptureDisplay::take() {
  if (queue_.empty()) return nullptr;
  taken_ = queue_.front();
  queue_.pop_front();
  showGesture(taken_.c_str());
  return taken_.c_str();
}

void CaptureDisplay::showGesture(const char* gesture) {
  callout_ = gesture != nullptr ? gesture : "";
  calloutFrames_ = callout_.empty() ? 0 : CalloutFrames;
}

void CaptureDisplay::markOverlay(int16_t top, int16_t rows) {
  overlayTop_ = top;
  overlayRows_ = rows;
}

void CaptureDisplay::beginCapture() {
  capture_.arm(width_, height_);
  capturing_ = true;
}

void CaptureDisplay::record() {
  if (!capturing_) return;
  keepFrame();
}

void CaptureDisplay::endCapture() {
  capturing_ = false;
  capture_.disarm();
}

bool CaptureDisplay::save(const char* path) const { return capture_.save(path); }

int CaptureDisplay::frameCount() const { return capture_.frameCount(); }

const std::vector<uint8_t>* CaptureDisplay::frame(int index) const { return capture_.frame(index); }

void CaptureDisplay::clear() {
  std::fill(frame_.begin(), frame_.end(), 0);
  std::fill(sent_.begin(), sent_.end(), 0);
  flushes_ = 0;
  lastDirty_ = gfx::Rect(0, 0, 0, 0);
  endCapture();
  queue_.clear();
  gestures_.clear();
  taken_.clear();
  callout_.clear();
  calloutFrames_ = 0;
  overlayTop_ = -1;
  overlayRows_ = 0;
}

uint8_t CaptureDisplay::at(int16_t x, int16_t y) const {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return 0;
  const size_t index = static_cast<size_t>(y) * stride_ + (depth_ == 8 ? x : (x >> 3));
  if (depth_ == 8) return frame_[index];
  return (frame_[index] & (0x80 >> (x & 7))) != 0 ? 255 : 0;
}

bool CaptureDisplay::anyInk(int16_t x, int16_t y, int16_t w, int16_t h) const {
  for (int16_t row = y; row < y + h; row++) {
    for (int16_t column = x; column < x + w; column++) {
      if (row < 0 || column < 0 || row >= height_ || column >= width_) continue;
      if (at(column, row) != 0) return true;
    }
  }
  return false;
}

int CaptureDisplay::inkCount(int16_t x, int16_t y, int16_t w, int16_t h) const {
  int count = 0;
  for (int16_t row = y; row < y + h; row++) {
    for (int16_t column = x; column < x + w; column++) {
      if (row < 0 || column < 0 || row >= height_ || column >= width_) continue;
      if (at(column, row) != 0) count++;
    }
  }
  return count;
}

void CaptureDisplay::pack(uint8_t* out) const {
  if (out == nullptr) return;
  if (depth_ == 1) {
    memcpy(out, sent_.data(), sent_.size());
    return;
  }
  const int16_t rowBytes = (int16_t)((width_ + 7) / 8);
  memset(out, 0, static_cast<size_t>(rowBytes) * height_);
  for (int16_t y = 0; y < height_; y++) {
    for (int16_t x = 0; x < width_; x++) {
      if (sent_[static_cast<size_t>(y) * stride_ + x] == 0) continue;
      out[static_cast<size_t>(y) * rowBytes + (x >> 3)] |= static_cast<uint8_t>(0x80 >> (x & 7));
    }
  }
}

}  // namespace gardener
