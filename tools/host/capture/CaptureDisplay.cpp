#include "CaptureDisplay.h"

#include <algorithm>
#include <string.h>

#include "Canvas.h"

namespace gardener {

namespace {

// Pixels between diagonal strokes: wide enough to read as a hash at the panel's size, dense enough
// that it cannot be mistaken for a drawn line.
constexpr int HashSpacing = 4;

// The pixel a buffer holds at a point, in the layout the depth says: the byte itself on a Gray8
// one, 255 or 0 on a Packed1 one.
uint8_t pixelAt(const std::vector<uint8_t>& buffer, int16_t stride, uint8_t depth, int16_t x,
                int16_t y) {
  const size_t index = static_cast<size_t>(y) * stride + (depth == 8 ? x : (x >> 3));
  if (depth == 8) return buffer[index];
  return (buffer[index] & (0x80 >> (x & 7))) != 0 ? 255 : 0;
}

// Clears the marked rows of the glass, outlines them, and fills them with a diagonal ink hash. The
// rows are the glass's own; the frame only says where the glass is drawn.
void drawOverlayHash(gfx::Canvas& canvas, const gfx::Rect& glass, int16_t top, int16_t rows) {
  if (top < 0 || rows <= 0 || top >= glass.height) return;
  const int16_t bottom =
      top + rows < glass.height ? static_cast<int16_t>(top + rows) : glass.height;
  const int16_t left = glass.x;
  const int16_t right = static_cast<int16_t>(glass.x + glass.width - 1);
  const int16_t first = static_cast<int16_t>(glass.y + top);
  const int16_t last = static_cast<int16_t>(glass.y + bottom - 1);

  for (int16_t y = first; y <= last; y++) {
    for (int16_t x = left; x <= right; x++) canvas.setPixel(x, y, gfx::Paper);
  }
  for (int16_t x = left; x <= right; x++) {
    canvas.setPixel(x, first, gfx::Ink);
    canvas.setPixel(x, last, gfx::Ink);
  }
  for (int16_t y = first; y <= last; y++) {
    canvas.setPixel(left, y, gfx::Ink);
    canvas.setPixel(right, y, gfx::Ink);
    for (int16_t x = static_cast<int16_t>(left + (y - first) % HashSpacing); x <= right;
         x = static_cast<int16_t>(x + HashSpacing)) {
      canvas.setPixel(x, y, gfx::Ink);
    }
  }
}

// The picture a capture composes for one frame: a packed one-bit frame over the bytes the frame
// writer takes. The glass, the overlay hash and the components are drawn through it, so the drawing
// gets a device to write to; nothing ever flushes it, because the capture hands the bytes on itself.
class PackedFrame : public gfx::DisplayDevice {
 public:
  PackedFrame(uint8_t* bytes, int16_t width, int16_t height)
      : bytes_(bytes), width_(width), height_(height) {}

  bool begin() override { return true; }
  int16_t width() const override { return width_; }
  int16_t height() const override { return height_; }
  uint8_t bitsPerPixel() const override { return 1; }

  void setPixel(int16_t x, int16_t y, uint8_t color) override {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    uint8_t& cell = bytes_[y * stride() + (x >> 3)];
    const uint8_t mask = static_cast<uint8_t>(0x80 >> (x & 7));
    if (color >= 128) {
      cell |= mask;
    } else {
      cell &= static_cast<uint8_t>(~mask);
    }
  }

  void flush() override {}

 private:
  int16_t stride() const { return static_cast<int16_t>((width_ + 7) / 8); }

  uint8_t* bytes_;
  int16_t width_;
  int16_t height_;
};

}  // namespace

CaptureDisplay::CaptureDisplay(int16_t width, int16_t height, uint8_t bitsPerPixel)
    : width_(width),
      height_(height),
      depth_(bitsPerPixel == 1 ? 1 : 8),
      stride_(bitsPerPixel == 1 ? (int16_t)((width + 7) / 8) : width),
      drawn_(static_cast<size_t>(stride_) * static_cast<size_t>(height), 0),
      sent_(drawn_.size(), 0) {}

bool CaptureDisplay::begin() {
  clear();
  return true;
}

void CaptureDisplay::setPixel(int16_t x, int16_t y, uint8_t color) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
  include(x, y);
  if (depth_ == 8) {
    drawn_[static_cast<size_t>(y) * stride_ + x] = color;
    return;
  }
  uint8_t& cell = drawn_[static_cast<size_t>(y) * stride_ + (x >> 3)];
  const uint8_t mask = static_cast<uint8_t>(0x80 >> (x & 7));
  if (color >= 128) {
    cell |= mask;
  } else {
    cell &= static_cast<uint8_t>(~mask);
  }
}

void CaptureDisplay::include(int16_t x, int16_t y) {
  if (dirty_.width <= 0 || dirty_.height <= 0) {
    dirty_ = gfx::Rect(x, y, 1, 1);
    return;
  }
  const int16_t left = x < dirty_.x ? x : dirty_.x;
  const int16_t top = y < dirty_.y ? y : dirty_.y;
  const int16_t right = x > dirty_.right() ? x : dirty_.right();
  const int16_t bottom = y > dirty_.bottom() ? y : dirty_.bottom();
  dirty_ = gfx::Rect(left, top, static_cast<int16_t>(right - left + 1),
                     static_cast<int16_t>(bottom - top + 1));
}

void CaptureDisplay::flush() {
  flushes_++;
  lastDirty_ = dirty_;
  const gfx::Rect dirty = dirty_;
  dirty_ = gfx::Rect(0, 0, 0, 0);

  const int16_t firstRow = std::max<int16_t>(0, dirty.y);
  const int16_t lastRow = std::min<int16_t>((int16_t)(height_ - 1), dirty.bottom());
  const int16_t firstColumn = std::max<int16_t>(0, dirty.x);
  const int16_t lastColumn = std::min<int16_t>((int16_t)(width_ - 1), dirty.right());
  if (lastRow < firstRow || lastColumn < firstColumn) return;

  if (depth_ == 8) {
    const size_t columns = static_cast<size_t>(lastColumn - firstColumn + 1);
    for (int16_t row = firstRow; row <= lastRow; row++) {
      const size_t at = static_cast<size_t>(row) * stride_ + firstColumn;
      memcpy(sent_.data() + at, drawn_.data() + at, columns);
    }
  } else {
    for (int16_t row = firstRow; row <= lastRow; row++) {
      for (int16_t column = firstColumn; column <= lastColumn; column++) {
        const size_t index = static_cast<size_t>(row) * stride_ + (column >> 3);
        const uint8_t mask = static_cast<uint8_t>(0x80 >> (column & 7));
        if ((drawn_[index] & mask) != 0) {
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
  std::vector<uint8_t> pixels(Capture::frameBytes(frame_.width, frame_.height), 0);
  PackedFrame picture(pixels.data(), frame_.width, frame_.height);
  gfx::View canvas = picture.root();

  // The glass as it was sent, at the place the frame gives it.
  for (int16_t y = 0; y < height_; y++) {
    for (int16_t x = 0; x < width_; x++) {
      if (pixelAt(sent_, stride_, depth_, x, y) == 0) continue;
      canvas.setPixel(static_cast<int16_t>(frame_.glass.x + x),
                      static_cast<int16_t>(frame_.glass.y + y), gfx::Ink);
    }
  }

  if (overlayTop_ >= 0) drawOverlayHash(canvas, frame_.glass, overlayTop_, overlayRows_);

  for (const Component& component : components_) {
    if (!component.draw) continue;
    gfx::View box = canvas.child(component.box);
    component.draw(box);
  }

  capture_.add(pixels.data());
}

void CaptureDisplay::press(const char* gesture) {
  if (gesture == nullptr || gesture[0] == '\0') return;
  push(String(gesture));
  gestures_.push_back(gesture);
}

void CaptureDisplay::markOverlay(int16_t top, int16_t rows) {
  overlayTop_ = top;
  overlayRows_ = rows;
}

void CaptureDisplay::beginCapture() {
  beginCapture(Frame{width_, height_, gfx::Rect(0, 0, width_, height_)}, {});
}

void CaptureDisplay::beginCapture(const Frame& frame, const std::vector<Component>& components) {
  frame_ = frame;
  components_ = components;
  capture_.arm(frame_.width, frame_.height);
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
  std::fill(drawn_.begin(), drawn_.end(), 0);
  std::fill(sent_.begin(), sent_.end(), 0);
  dirty_ = gfx::Rect(0, 0, 0, 0);
  flushes_ = 0;
  lastDirty_ = gfx::Rect(0, 0, 0, 0);
  endCapture();
  frame_ = Frame{};
  components_.clear();
  GestureQueue::clear();
  gestures_.clear();
  overlayTop_ = -1;
  overlayRows_ = 0;
}

uint8_t CaptureDisplay::at(int16_t x, int16_t y) const {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return 0;
  return pixelAt(drawn_, stride_, depth_, x, y);
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

}  // namespace gardener
