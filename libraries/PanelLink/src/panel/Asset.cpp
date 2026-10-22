#include "panel/Asset.h"

#include <LittleFS.h>
#include <RLEBitmap.h>
#include <string.h>

#include "hal/Display.h"

namespace gardener {
namespace {

// The container: ten bytes of header, then the palette, then the runs. See docs/specs/assets.md.
constexpr uint8_t kMagic[4] = {'G', 'R', 'L', '1'};
constexpr size_t kHeaderBytes = 10;

uint16_t read16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

// The renderer resolves each run to its palette colour before handing it over. This panel has one
// bit, so a run is ink or nothing: the paper is what the buffer already holds, and a run of it is
// not drawn.
class InkContext : public RLEGraphicsContext {
 public:
  explicit InkContext(Display& display) : display_(display) {}

  void startWrite() override {}
  void endWrite() override {}
  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    if (color != 0) display_.hLine(x, y, w);
  }

 private:
  Display& display_;
};

// The renderer's view of the file: the size, the flags, and where the palette and the runs start.
bool readInfo(const uint8_t* bytes, size_t length, RLEBitmapInfo& info) {
  if (length < kHeaderBytes) return false;

  const uint8_t paletteEntries = bytes[9];
  const size_t paletteBytes = (size_t)paletteEntries * sizeof(uint16_t);
  const uint16_t width = read16(bytes + 4);
  const uint16_t height = read16(bytes + 6);
  if (memcmp(bytes, kMagic, sizeof kMagic) != 0 || paletteEntries == 0 || width == 0 ||
      height == 0 || length < kHeaderBytes + paletteBytes) {
    return false;
  }

  info.pRLEBM_palette_far = (uint32_t)(uintptr_t)(bytes + kHeaderBytes);
  info.pRLEBM_data_far = (uint32_t)(uintptr_t)(bytes + kHeaderBytes + paletteBytes);
  info.width = width;
  info.height = height;
  info.flags = bytes[8];
  return true;
}

}  // namespace

void drawAsset(Display& display, const char* path, int16_t x, int16_t y) {
  File file = LittleFS.open(path, "r");
  if (!file) return;
  const size_t size = (size_t)file.size();
  if (size <= kHeaderBytes) {
    file.close();
    return;
  }

  uint8_t* bytes = new uint8_t[size];
  if (bytes == nullptr) {
    file.close();
    return;
  }
  const size_t got = file.read(bytes, size);
  file.close();

  RLEBitmapInfo info{};
  if (got > kHeaderBytes && readInfo(bytes, got, info)) {
    InkContext context(display);
    renderRLEBitmap(info, x, y, &context, true);
  }

  delete[] bytes;
}

}  // namespace gardener
