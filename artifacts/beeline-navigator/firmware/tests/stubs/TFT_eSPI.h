#pragma once
#include <Arduino.h>
constexpr int TL_DATUM = 0, TC_DATUM = 1, MC_DATUM = 2;
struct TFT_eSPI {
  struct RectSample { int x, y, width, height; uint16_t color; };
  struct ImageSample { int x, y; uint16_t color; bool swapped, uniform; };
  std::vector<RectSample> rectangles;
  std::vector<ImageSample> images;
  bool swapBytes = false;
  int rotation = 1, fullDraws = 0, panelDraws = 0, calls = 0;
  std::vector<std::string> text;
  std::vector<int> graduations;
  int imageRows = 0, imageWrites = 0;
  uint16_t firstImagePixel = 0, lastImagePixel = 0, lastFillColor = 0;
  void touch() { assert(!inBleWrite && !criticalDepth); ++calls; }
  int width() const { return rotation % 2 ? 320 : 240; }
  int height() const { return rotation % 2 ? 240 : 320; }
  void init() { touch(); }
  void setRotation(int v) { touch(); rotation = v; }
  bool getSwapBytes() const { return swapBytes; }
  void setSwapBytes(bool value) { touch(); swapBytes = value; }
  void fillScreen(int color) { touch(); ++fullDraws; lastFillColor = color; }
  void fillRect(int x, int y, int width, int height, int color) {
    touch(); if (x == 96 && y == 207) ++panelDraws;
    lastFillColor = static_cast<uint16_t>(color);
    rectangles.push_back({x, y, width, height, static_cast<uint16_t>(color)});
  }
  void pushImage(int x, int y, int width, int height, uint16_t* pixels) {
    touch();
    if (imageWrites == 0 && width > 0 && height > 0) firstImagePixel = pixels[0];
    if (width > 0 && height > 0) lastImagePixel = pixels[width * height - 1];
    if (width > 0 && height > 0) {
      bool uniform = true;
      for (int i = 1; i < width * height; ++i) uniform &= pixels[i] == pixels[0];
      images.push_back({x, y, pixels[0], swapBytes, uniform});
    }
    ++imageWrites;
    imageRows += height;
    if (x == 0 && y == 0 && width == 320) ++fullDraws;
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
