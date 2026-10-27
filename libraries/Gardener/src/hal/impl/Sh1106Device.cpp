#include "Build.h"

#if GARDENER_PANEL

#include "hal/impl/Sh1106Device.h"

#include <Arduino.h>
#include <string.h>

// The init sequence and page write below follow Adafruit_SH110X's SH1106 driver.
//
// Software License Agreement (BSD License)
//
// Copyright (c) 2012 Adafruit Industries.  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
// - Redistributions of source code must retain the above copyright notice, this list of conditions
//   and the following disclaimer.
// - Redistributions in binary form must reproduce the above copyright notice, this list of
//   conditions and the following disclaimer in the documentation and/or other materials provided
//   with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
// WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

namespace gardener {
namespace {

// The SH1106 has 132 columns of RAM for a 128-pixel glass; the panel's first column is RAM column 2.
constexpr uint8_t PageColumnOffset = 2;

}  // namespace

Sh1106Device::Sh1106Device(TwoWire& wire, uint8_t address, int16_t width, int16_t height)
    : wire_(wire),
      address_(address),
      width_(width),
      height_(height),
      frame_(new uint8_t[static_cast<size_t>(stride()) * static_cast<size_t>(height)]) {
  memset(frame_, 0, static_cast<size_t>(stride()) * static_cast<size_t>(height_));
}

Sh1106Device::~Sh1106Device() { delete[] frame_; }

bool Sh1106Device::begin() {
  const uint8_t init[] = {
      0xAE,        // display off
      0xD5, 0x80,  // display clock divide
      0xA8, 0x3F,  // multiplex to 64 rows
      0xD3, 0x00,  // display offset
      0x40,        // start line 0
      0x8B,        // DC/DC on
      0xA1,        // segment remap
      0xC8,        // COM scan direction
      0xDA, 0x12,  // COM pins
      0x81, 0xFF,  // contrast
      0xD9, 0x1F,  // precharge
      0xDB, 0x40,  // VCOM detect
      0x33,        // VPP 9 V
      0xA6,        // normal display
      0x20, 0x10,  // memory mode
      0xA4,        // display all on resume
  };
  if (!commands(init, sizeof(init))) return false;
  delay(100);
  return command(0xAF);  // display on
}

void Sh1106Device::flush(const gfx::Rect& dirty) {
  if (dirty.width <= 0 || dirty.height <= 0) return;

  const int16_t firstColumn = dirty.x < 0 ? 0 : dirty.x;
  const int16_t lastColumn =
      dirty.right() >= width_ ? static_cast<int16_t>(width_ - 1) : dirty.right();
  const int16_t firstPage = dirty.y < 0 ? 0 : static_cast<int16_t>(dirty.y / 8);
  const int16_t lastPage = dirty.bottom() >= height_ ? static_cast<int16_t>(pages() - 1)
                                                     : static_cast<int16_t>(dirty.bottom() / 8);
  if (firstColumn > lastColumn || firstPage > lastPage) return;

  // One control byte shares each transmission with the pixel data, so a chunk carries one byte less
  // than the bus buffer holds.
  uint8_t chunk[I2C_BUFFER_LENGTH];
  const int16_t capacity = static_cast<int16_t>(I2C_BUFFER_LENGTH - 1);

  for (int16_t page = firstPage; page <= lastPage; page++) {
    // The window is opened once and the column address walks from there, across the chunk
    // transmissions as well as inside one.
    const uint8_t window[] = {
        static_cast<uint8_t>(0xB0 + page),
        static_cast<uint8_t>(0x10 | ((firstColumn + PageColumnOffset) >> 4)),
        static_cast<uint8_t>((firstColumn + PageColumnOffset) & 0x0F),
    };
    commands(window, sizeof(window));

    int16_t column = firstColumn;
    while (column <= lastColumn) {
      int16_t count = 0;
      while (column <= lastColumn && count < capacity) {
        chunk[count++] = pageColumn(page, column);
        column++;
      }
      data(chunk, static_cast<size_t>(count));
    }
  }
}

uint8_t Sh1106Device::pageColumn(int16_t page, int16_t column) const {
  const int16_t strideBytes = stride();
  uint8_t bits = 0;
  for (uint8_t bit = 0; bit < 8; bit++) {
    const int16_t y = static_cast<int16_t>(page * 8 + bit);
    if (y < height_ && (frame_[y * strideBytes + (column >> 3)] & (0x80 >> (column & 7))) != 0) {
      bits = static_cast<uint8_t>(bits | (1 << bit));
    }
  }
  return bits;
}

void Sh1106Device::invertDisplay(bool on) { command(on ? 0xA7 : 0xA6); }

bool Sh1106Device::command(uint8_t value) { return commands(&value, 1); }

bool Sh1106Device::commands(const uint8_t* values, size_t count) {
  wire_.beginTransmission(address_);
  wire_.write(static_cast<uint8_t>(0x00));
  for (size_t i = 0; i < count; i++) wire_.write(values[i]);
  return wire_.endTransmission() == 0;
}

bool Sh1106Device::data(const uint8_t* values, size_t count) {
  wire_.beginTransmission(address_);
  wire_.write(static_cast<uint8_t>(0x40));
  for (size_t i = 0; i < count; i++) wire_.write(values[i]);
  return wire_.endTransmission() == 0;
}

}  // namespace gardener

#endif
