#pragma once
#include <cstdint>
#include "SPI.h"

struct TS_Point {
  int16_t x = 0;
  int16_t y = 0;
  int16_t z = 0;
};

class XPT2046_Touchscreen {
 public:
  XPT2046_Touchscreen(uint8_t, uint8_t) {}
  bool begin(SPIClass&) { return true; }
  void setRotation(uint8_t) {}
  bool touched() { return pressed; }
  TS_Point getPoint() { return point; }
  bool pressed = false;
  TS_Point point;
};
