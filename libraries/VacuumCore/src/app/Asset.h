#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <RLEBitmap.h>

namespace gardener {

class Display;
class Storage;

// One image from the device's files: a run-length encoded bitmap the RLEBitmap renderer walks, held
// in the heap for as long as the asset lives. A page loads it at boot and draws it as often as it
// redraws, so nothing here opens a file twice.
//
// The file is the renderer's own three pieces -- size, palette and runs -- behind a ten-byte header;
// docs/assets.md is the format and scripts/asset.ps1 writes it from a 1-bit BMP.
class Asset {
 public:
  Asset() = default;
  ~Asset();

  // Owns heap bytes, so it is not copied.
  Asset(const Asset&) = delete;
  Asset& operator=(const Asset&) = delete;

  // Reads and checks the whole file. False when it is not there or is not a bitmap this build can
  // read; the caller reports that rather than treating it as fatal.
  bool load(Storage& storage, const char* path);

  bool loaded() const { return info_.pRLEBM_data_far != 0; }
  int16_t width() const { return (int16_t)info_.width; }
  int16_t height() const { return (int16_t)info_.height; }

  // Ink only: the paper runs are dropped, so whatever the page already drew shows through.
  void draw(Display& display, int16_t x, int16_t y) const;

 private:
  uint8_t* bytes_ = nullptr;
  RLEBitmapInfo info_{};
};

}  // namespace gardener
