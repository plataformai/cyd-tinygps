#pragma once
#include <cstdint>

constexpr int HSPI = 1;

class SPIClass {
 public:
  explicit SPIClass(int) {}
  void begin(int = -1, int = -1, int = -1, int = -1) {}
};
