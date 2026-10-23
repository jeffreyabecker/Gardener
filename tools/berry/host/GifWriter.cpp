#include "GifWriter.h"

#include <stdio.h>
#include <unordered_map>

namespace gardener {

namespace {

void put8(FILE* file, uint8_t value) { fputc(value, file); }

void put16(FILE* file, uint16_t value) {
  fputc(value & 0xFF, file);
  fputc((value >> 8) & 0xFF, file);
}

// One frame unpacked to one byte per pixel and enlarged by the scale: each source pixel becomes a
// scale x scale block, the nearest neighbour of a one-bit picture. The LZW stage wants one pixel
// per byte, so the packed row is spread first.
std::vector<uint8_t> unpackScaled(const uint8_t* packed, int16_t width, int16_t height,
                                  int scale) {
  const int16_t rowBytes = (width + 7) / 8;
  const int16_t outRowBytes = width * scale;
  std::vector<uint8_t> out((size_t)outRowBytes * height * scale);
  for (int16_t y = 0; y < height; y++) {
    for (int16_t x = 0; x < width; x++) {
      const uint8_t bit = (packed[y * rowBytes + x / 8] >> (7 - (x & 7))) & 1;
      for (int16_t dy = 0; dy < scale; dy++) {
        uint8_t* row = out.data() + (size_t)(y * scale + dy) * outRowBytes;
        for (int16_t dx = 0; dx < scale; dx++) row[x * scale + dx] = bit;
      }
    }
  }
  return out;
}

// The one-byte-per-pixel image, compressed.
std::vector<uint8_t> lzwEncode(const uint8_t* indices, size_t count) {
  constexpr int kMinCodeSize = 2;
  constexpr int kClearCode = 1 << kMinCodeSize;  // 4
  constexpr int kEndCode = kClearCode + 1;       // 5
  constexpr int kMaxCode = 4096;

  // The dictionary: (prefix << 8 | pixel) -> code. A clear wipes it, which a per-node table full
  // of -1 would make O(4096) for no gain.
  std::unordered_map<int, int> children;
  children.reserve(kMaxCode);

  int codeSize = kMinCodeSize + 1;
  int nextCode = kEndCode + 1;

  // GIF packs codes least-significant bit first into each byte.
  std::vector<uint8_t> out;
  uint32_t acc = 0;
  int accBits = 0;
  auto emit = [&](int code) {
    acc |= (uint32_t)code << accBits;
    accBits += codeSize;
    while (accBits >= 8) {
      out.push_back((uint8_t)(acc & 0xFF));
      acc >>= 8;
      accBits -= 8;
    }
  };

  emit(kClearCode);
  int current = indices[0];
  for (size_t i = 1; i < count; i++) {
    const uint8_t pixel = indices[i];
    const int key = (current << 8) | pixel;
    const auto found = children.find(key);
    if (found != children.end()) {
      current = found->second;
      continue;
    }
    emit(current);
    if (nextCode < kMaxCode) {
      // The code being assigned is what decides the size, checked before it is assigned: the
      // decoder grows its code size the moment it receives a code that fills the table.
      if (nextCode == (1 << codeSize) && codeSize < 12) codeSize++;
      children[key] = nextCode++;
    } else {
      // The table is full: clear it at the size in effect, then start over smaller.
      emit(kClearCode);
      children.clear();
      codeSize = kMinCodeSize + 1;
      nextCode = kEndCode + 1;
    }
    current = pixel;
  }
  emit(current);
  emit(kEndCode);
  if (accBits > 0) out.push_back((uint8_t)(acc & 0xFF));
  return out;
}

}  // namespace

bool writeGif(const char* path, int16_t width, int16_t height,
              const std::vector<std::vector<uint8_t>>& frames, uint16_t delayCs) {
  if (frames.empty()) return false;

  const int16_t outWidth = width * kRenderScale;
  const int16_t outHeight = height * kRenderScale;

  FILE* file = fopen(path, "wb");
  if (file == nullptr) return false;

  fputs("GIF89a", file);
  // Logical screen descriptor: a global colour table, 8-bit colour resolution, two entries.
  put16(file, (uint16_t)outWidth);
  put16(file, (uint16_t)outHeight);
  put8(file, 0xF0);  // GCT | colour resolution 8 | GCT size 2^(0+1) = 2 entries
  put8(file, 0);     // background colour index
  put8(file, 0);     // pixel aspect ratio
  // Global colour table: index 0 white paper, index 1 black ink.
  fputc(0xFF, file);
  fputc(0xFF, file);
  fputc(0xFF, file);
  fputc(0x00, file);
  fputc(0x00, file);
  fputc(0x00, file);

  // Loop forever.
  const uint8_t kLoopExtension[] = {0x21, 0xFF, 0x0B, 'N', 'E', 'T', 'S', 'C',
                                    'A',  'P',  'E',  '2', '.', '0', 0x03, 0x01,
                                    0x00, 0x00, 0x00};
  fwrite(kLoopExtension, 1, sizeof kLoopExtension, file);

  for (const std::vector<uint8_t>& frame : frames) {
    // Graphic control extension: no disposal, no transparency, the delay.
    put8(file, 0x21);
    put8(file, 0xF9);
    put8(file, 0x04);
    put8(file, 0x00);
    put16(file, delayCs);
    put8(file, 0x00);
    put8(file, 0x00);

    // Image descriptor, then the LZW stream as sub-blocks.
    put8(file, 0x2C);
    put16(file, 0);
    put16(file, 0);
    put16(file, (uint16_t)outWidth);
    put16(file, (uint16_t)outHeight);
    put8(file, 0x00);  // no local colour table, not interlaced

    const std::vector<uint8_t> pixels = unpackScaled(frame.data(), width, height, kRenderScale);
    const std::vector<uint8_t> compressed = lzwEncode(pixels.data(), pixels.size());
    put8(file, 2);  // min code size
    size_t at = 0;
    while (at < compressed.size()) {
      const size_t block = compressed.size() - at < 255 ? compressed.size() - at : 255;
      put8(file, (uint8_t)block);
      fwrite(compressed.data() + at, 1, block, file);
      at += block;
    }
    put8(file, 0);  // end of the sub-blocks
  }

  put8(file, 0x3B);  // trailer
  return fclose(file) == 0;
}

}  // namespace gardener
