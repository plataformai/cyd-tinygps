#pragma once
#include <Arduino.h>
constexpr int TL_DATUM = 0, TC_DATUM = 1, MC_DATUM = 2;
struct TFT_eSPI {
  struct RectSample { int x, y, width, height; uint16_t color; };
  struct LineSample { int x, y, length; uint16_t color; };
  struct TextSample { std::string value; int x, y, font, color; };
  std::vector<RectSample> rectangles;
  std::vector<LineSample> horizontalLines, verticalLines;
  std::vector<TextSample> textSamples;
  int rotation = 1, fullDraws = 0, panelDraws = 0, calls = 0;
  int roundRectCalls = 0, activeTextColor = 0;
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
  struct TriangleSample { int x0, y0, x1, y1, x2, y2, color; };
  std::vector<TriangleSample> triangles;
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, int color) {
    touch(); triangles.push_back({x0, y0, x1, y1, x2, y2, color});
  }
  struct CircleSample { int x, y, radius, color; };
  std::vector<CircleSample> circles;
  void fillCircle(int x, int y, int radius, int color) {
    touch(); circles.push_back({x, y, radius, color});
  }
  int textWidth(const String& value, int font) { touch(); return value.length() * (font == 1 ? 6 : 8); }
  void drawFastHLine(int x, int y, int width, int color) {
    touch(); horizontalLines.push_back({x, y, width, static_cast<uint16_t>(color)});
  }
  void drawFastVLine(int x, int y, int height, int color) {
    touch(); verticalLines.push_back({x, y, height, static_cast<uint16_t>(color)});
  }
  template <typename... T> void drawRect(T...) { touch(); }
  template <typename... T> void drawRoundRect(T...) { touch(); ++roundRectCalls; }
  template <typename... T> void setTextDatum(T...) { touch(); }
  void setTextColor(int color) { touch(); activeTextColor = color; }
  void drawString(const String& v, int x, int y, int font) {
    touch(); text.push_back(v.value);
    textSamples.push_back({v.value, x, y, font, activeTextColor});
    if (y == 222) graduations.push_back(std::stoi(v.value));
  }
  void drawCentreString(const String& v, int, int, int) { touch(); text.push_back(v.value); }
};
