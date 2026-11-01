#include "Bitmap.h"

#include <stddef.h>
#include <string.h>

#include <utility>

#include "layout/Drawing.h"

namespace gardener::gfx {
namespace {

// One hex digit as the value it names, or -1 for a character that is not one.
int8_t hexDigit(char digit) {
  if (digit >= '0' && digit <= '9') return digit - '0';
  if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
  if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
  return -1;
}

// The bytes a string of hex digits names, and nothing for a string that does not name whole bytes.
std::vector<uint8_t> hexBytes(const char* text) {
  std::vector<uint8_t> bytes;
  if (text == nullptr) return bytes;
  const size_t count = strlen(text);
  if (count % 2 != 0) return bytes;
  bytes.reserve(count / 2);
  for (size_t at = 0; at < count; at += 2) {
    const int8_t high = hexDigit(text[at]);
    const int8_t low = hexDigit(text[at + 1]);
    if (high < 0 || low < 0) return std::vector<uint8_t>();
    bytes.push_back(static_cast<uint8_t>((high << 4) | low));
  }
  return bytes;
}

// The bytes one pixel of a format takes, and none of a bitmap that is a bit a pixel.
uint8_t bytesPerPixel(BitmapFormat format) {
  switch (format) {
    case BitmapFormat::Grayscale:
      return 1;
    case BitmapFormat::RGB565:
      return 2;
    case BitmapFormat::RGB888:
      return 3;
    case BitmapFormat::Monochrome:
      break;
  }
  return 0;
}

// The bytes a bitmap of this width and height takes: a byte of row a whole byte of columns for a
// monochrome one, and a pixel's own bytes for the rest.
size_t bytesNeeded(int16_t width, int16_t height, BitmapFormat format) {
  if (width <= 0 || height <= 0) return 0;
  const size_t rows = static_cast<size_t>(height);
  if (format == BitmapFormat::Monochrome) {
    const size_t stride = (static_cast<size_t>(width) + 7) / 8;
    return stride * rows;
  }
  return static_cast<size_t>(width) * rows * bytesPerPixel(format);
}

// What a colour comes to on a frame that keeps a byte a pixel.
uint8_t luminance(uint8_t red, uint8_t green, uint8_t blue) {
  return static_cast<uint8_t>((77u * red + 150u * green + 29u * blue) >> 8);
}

// The three bytes a bitmap's string opens with, read without decoding the pixels after them:
// `valid` is false for a string that is not all hex digits, that is too short to hold a header, or
// that names a width, a height or a format the drawing has no case for.
struct Header {
  int16_t width = 0;
  int16_t height = 0;
  BitmapFormat format = BitmapFormat::Monochrome;
  bool valid = false;
};

Header headerOf(const char* text) {
  Header header;
  if (text == nullptr) return header;
  const size_t count = strlen(text);
  if (count % 2 != 0) return header;
  for (size_t at = 0; at < count; at++) {
    if (hexDigit(text[at]) < 0) return header;
  }
  if (count / 2 < 3) return header;

  const auto byteAt = [text](size_t at) {
    return static_cast<uint8_t>((hexDigit(text[2 * at]) << 4) | hexDigit(text[2 * at + 1]));
  };
  header.width = byteAt(0);
  header.height = byteAt(1);
  const uint8_t format = byteAt(2);
  if (header.width <= 0 || header.height <= 0) return header;
  if (format > static_cast<uint8_t>(BitmapFormat::RGB888)) return header;
  header.format = static_cast<BitmapFormat>(format);
  header.valid = true;
  return header;
}

// Whether the string holds the pixels its header asks for: a header with no width, height or format
// the drawing has no case for is not one, and neither is a string of a byte less than it names.
bool wholeBitmap(const Header& header, const char* text) {
  if (!header.valid) return false;
  return strlen(text) / 2 - 3 >= bytesNeeded(header.width, header.height, header.format);
}

}  // namespace

bool isBitmap(const char* text) { return wholeBitmap(headerOf(text), text); }

Size bitmapSize(const char* text) {
  const Header header = headerOf(text);
  if (!wholeBitmap(header, text)) return Size(0, 0);
  return Size(header.width, header.height);
}

Bitmap::Bitmap(std::vector<uint8_t> bits, int16_t width, int16_t height, BitmapFormat format)
    : bits_(std::move(bits)), width_(width), height_(height), format_(format) {}

PaintNodeRef Bitmap::fromHex(const char* hexString) {
  if (!isBitmap(hexString)) return PaintNodeRef();
  const std::vector<uint8_t> bytes = hexBytes(hexString);
  const BitmapFormat format = static_cast<BitmapFormat>(bytes[2]);
  return std::make_unique<Bitmap>(std::vector<uint8_t>(bytes.begin() + 3, bytes.end()), bytes[0],
                                  bytes[1], format);
}

void Bitmap::draw(Canvas& canvas) const {
  if (bits_.size() < bytesNeeded(width_, height_, format_)) return;
  const size_t stride = (static_cast<size_t>(width_) + 7) / 8;
  for (int16_t row = 0; row < height_; row++) {
    for (int16_t col = 0; col < width_; col++) {
      const size_t pixel = static_cast<size_t>(row) * width_ + col;
      switch (format_) {
        case BitmapFormat::Monochrome:
          if ((bits_[row * stride + col / 8] & (0x80 >> (col & 7))) != 0) {
            canvas.setPixel(col, row, Ink);
          }
          break;
        case BitmapFormat::Grayscale:
          canvas.setPixel(col, row, bits_[pixel]);
          break;
        case BitmapFormat::RGB565: {
          const size_t at = 2 * pixel;
          const uint16_t value = static_cast<uint16_t>((bits_[at] << 8) | bits_[at + 1]);
          const uint8_t red = static_cast<uint8_t>(((value >> 11) & 0x1F) * 255 / 31);
          const uint8_t green = static_cast<uint8_t>(((value >> 5) & 0x3F) * 255 / 63);
          const uint8_t blue = static_cast<uint8_t>((value & 0x1F) * 255 / 31);
          canvas.setPixel(col, row, luminance(red, green, blue));
          break;
        }
        case BitmapFormat::RGB888: {
          const size_t at = 3 * pixel;
          canvas.setPixel(col, row, luminance(bits_[at], bits_[at + 1], bits_[at + 2]));
          break;
        }
      }
    }
  }
}

}  // namespace gardener::gfx
