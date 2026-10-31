#pragma once
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <type_traits>
#include <vector>

class String {
 public:
  std::string value;
  String() = default;
  String(const char* v) : value(v) {}
  String(const std::string& v) : value(v) {}
  template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
  explicit String(T v) : value(std::to_string(v)) {}
  unsigned int length() const { return value.size(); }
  const char* c_str() const { return value.c_str(); }
  String substring(unsigned int from, unsigned int to) const {
    return String(value.substr(from, to - from));
  }
  String& operator+=(const String& other) { value += other.value; return *this; }
  friend String operator+(const String& a, const String& b) { return String(a.value + b.value); }
  friend bool operator==(const String& a, const String& b) { return a.value == b.value; }
  friend bool operator!=(const String& a, const String& b) { return !(a == b); }
};
inline uint32_t fakeNow = 0;
inline bool inBleWrite = false;
inline int criticalDepth = 0;
inline uint32_t millis() { return fakeNow; }
inline void yield() {}
constexpr int OUTPUT = 1, HIGH = 1;
constexpr float PI = 3.14159265358979323846f;
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
template <typename T> T constrain(T v, T lo, T hi) { return std::min(hi, std::max(lo, v)); }
struct FakeSerial {
  std::deque<char> input;
  std::string output;
  void begin(int) {}
  int available() const { return input.size(); }
  int read() { char c = input.front(); input.pop_front(); return c; }
  void send(const std::string& v) { for (char c : v) input.push_back(c); }
  void println(const char* v) { assert(!criticalDepth && !inBleWrite); output += v; output += '\n'; }
  template <typename... Args> void printf(const char* format, Args... args) {
    assert(!criticalDepth && !inBleWrite);
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), format, args...);
    output += buffer;
  }
};
inline FakeSerial Serial;
