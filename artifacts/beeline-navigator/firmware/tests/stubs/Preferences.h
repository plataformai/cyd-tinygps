#pragma once
#include <cstddef>
#include <cstdint>

class Preferences {
 public:
  bool begin(const char*, bool) { return true; }
  void end() {}
  size_t getBytesLength(const char*) const { return 0; }
  uint8_t getUChar(const char*, uint8_t fallback) const { return fallback; }
  size_t getBytes(const char*, void*, size_t) const { return 0; }
  size_t putBytes(const char*, const void*, size_t length) { return length; }
  size_t putUChar(const char*, uint8_t) { return 1; }
};
