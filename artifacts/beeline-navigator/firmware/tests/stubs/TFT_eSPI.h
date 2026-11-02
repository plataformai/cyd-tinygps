#pragma once
#include <Arduino.h>
constexpr int TL_DATUM = 0, TC_DATUM = 1, MC_DATUM = 2;
struct TFT_eSPI {
  struct RectSample { int x, y, width, height; uint16_t color; };
  std::vector<RectSample> rectangles;
  int rotation = 1, fullDraws = 0, panelDraws = 0, calls = 0;
  std::vector<std::string> text;
  std::vector<int> graduations;
  uint16_t lastFillScreenColor = 0, lastFillColor = 0;
  void touch() { assert(!inBleWrite && !criticalDepth); ++calls; }
  int width() const { return rotation % 2 ? 320 : 240; }
  int height() const { return rotation % 2 ? 240 : 320; }
  void init() { touch(); }
  void setRotation(int v) { touch(); rotation = v; }
  void fillScreen(int color) {
    touch(); ++fullDraws; lastFillScreenColor = static_cast<uint16_t>(color);
  }
  void fillRect(int x, int y, int width, int height, int color) {
    touch(); if (x == 96 && y == 207) ++panelDraws;
    lastFillColor = static_cast<uint16_t>(color);
    rectangles.push_back({x, y, width, height, static_cast<uint16_t>(color)});
  }
  template <typename... T> void fillTriangle(T...) { touch(); }
  template <typename... T> void drawFastHLine(T...) { touch(); }
  template <typename... T> void drawFastVLine(T...) { touch(); }
  template <typename... T> void drawRect(T...) { touch(); }
  template <typename... T> void drawRoundRect(T...) { touch(); }
  template <typename... T> void setTextDatum(T...) { touch(); }
  template <typename... T> void setTextColor(T...) { touch(); }
  void drawString(const String& v, int, int y, int) {
    touch(); text.push_back(v.value);
    if (y == 222) graduations.push_back(std::stoi(v.value));
  }
  void drawCentreString(const String& v, int, int, int) { touch(); text.push_back(v.value); }
};
