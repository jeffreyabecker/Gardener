#include "app/Asset.h"

#include <new>
#include <string.h>

#include "app/Storage.h"
#include "hal/Display.h"

namespace gardener {
namespace {

// The container: ten bytes of header, then the palette, then the runs. See docs/assets.md.
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

}  // namespace

Asset::~Asset() { delete[] bytes_; }

bool Asset::load(Storage& storage, const char* path) {
  delete[] bytes_;
  bytes_ = nullptr;
  info_ = RLEBitmapInfo{};

  const size_t size = storage.size(path);
  if (size <= kHeaderBytes) return false;

  uint8_t* bytes = new uint8_t[size];
  if (bytes == nullptr) return false;

  size_t got = 0;
  if (!storage.read(path, bytes, size, got) || got < kHeaderBytes) {
    delete[] bytes;
    return false;
  }

  const uint8_t paletteEntries = bytes[9];
  const size_t paletteBytes = (size_t)paletteEntries * sizeof(uint16_t);
  const uint16_t width = read16(bytes + 4);
  const uint16_t height = read16(bytes + 6);
  if (memcmp(bytes, kMagic, sizeof kMagic) != 0 || paletteEntries == 0 || width == 0 ||
      height == 0 || got < kHeaderBytes + paletteBytes) {
    delete[] bytes;
    return false;
  }

  bytes_ = bytes;
  info_.pRLEBM_palette_far = (uint32_t)(uintptr_t)(bytes + kHeaderBytes);
  info_.pRLEBM_data_far = (uint32_t)(uintptr_t)(bytes + kHeaderBytes + paletteBytes);
  info_.width = width;
  info_.height = height;
  info_.flags = bytes[8];
  return true;
}

void Asset::draw(Display& display, int16_t x, int16_t y) const {
  if (!loaded()) return;
  InkContext context(display);
  renderRLEBitmap(info_, x, y, &context, true);
}

}  // namespace gardener
