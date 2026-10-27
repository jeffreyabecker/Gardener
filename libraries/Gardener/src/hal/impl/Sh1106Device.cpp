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
      frame_(new uint8_t[static_cast<size_t>(stride()) * static_cast<size_t>(height)]),
      pageBuffer_(new uint8_t[static_cast<size_t>(width) * static_cast<size_t>(pages())]) {
  memset(frame_, 0, static_cast<size_t>(stride()) * static_cast<size_t>(height_));
  memset(pageBuffer_, 0, static_cast<size_t>(width_) * static_cast<size_t>(pages()));
}

Sh1106Device::~Sh1106Device() {
  delete[] frame_;
  delete[] pageBuffer_;
}

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

void Sh1106Device::flush() {
  const int16_t strideBytes = stride();
  const int16_t pageCount = pages();

  // The Canvas frame is row-major, most-significant bit first; the panel wants each column's eight
  // rows in one byte, least-significant row first.
  for (int16_t page = 0; page < pageCount; page++) {
    for (int16_t x = 0; x < width_; x++) {
      uint8_t bits = 0;
      for (uint8_t bit = 0; bit < 8; bit++) {
        const int16_t y = static_cast<int16_t>(page * 8 + bit);
        if (y < height_ && (frame_[y * strideBytes + x / 8] & (0x80 >> (x & 7))) != 0) {
          bits = static_cast<uint8_t>(bits | (1 << bit));
        }
      }
      pageBuffer_[page * width_ + x] = bits;
    }
  }

  // One control byte shares the transmission with the pixel data.
  const size_t chunk = I2C_BUFFER_LENGTH - 1;
  for (int16_t page = 0; page < pageCount; page++) {
    const uint8_t window[] = {
        static_cast<uint8_t>(0xB0 + page),
        static_cast<uint8_t>(0x10 | (PageColumnOffset >> 4)),
        static_cast<uint8_t>(PageColumnOffset & 0x0F),
    };
    commands(window, sizeof(window));

    const uint8_t* bytes = pageBuffer_ + page * width_;
    int16_t remaining = width_;
    while (remaining > 0) {
      const size_t count = static_cast<size_t>(remaining) < chunk ? static_cast<size_t>(remaining)
                                                                  : chunk;
      data(bytes, count);
      bytes += count;
      remaining = static_cast<int16_t>(remaining - static_cast<int16_t>(count));
    }
  }
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
