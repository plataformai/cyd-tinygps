#pragma once

#include <stdint.h>
#include <TFT_eSPI.h>
#include "beeline_background.h"

class BeeLineBackgroundRenderer {
 public:
  void draw(TFT_eSPI& display, uint16_t fallbackColor) {
    if (!isLandscape(display)) {
      display.fillScreen(fallbackColor);
      return;
    }
    drawRegion(display, 0, BEE_LINE_BACKGROUND_HEIGHT, fallbackColor);
  }

  void drawRegion(TFT_eSPI& display, int16_t top, int16_t height,
                  uint16_t fallbackColor) {
    const int16_t screenWidth = display.width();
    const int16_t screenHeight = display.height();
    if (!isLandscape(display)) {
      display.fillRect(0, top, screenWidth, height, fallbackColor);
      return;
    }

    const int16_t firstRow = top < 0 ? 0 : top;
    const int32_t requestedEnd = static_cast<int32_t>(top) + height;
    const int16_t lastRow = requestedEnd > screenHeight
        ? screenHeight : static_cast<int16_t>(requestedEnd);
    if (firstRow >= lastRow) return;

    for (int16_t chunkTop = firstRow; chunkTop < lastRow;) {
      int16_t rows = lastRow - chunkTop;
      if (rows > ROWS_PER_CHUNK) rows = ROWS_PER_CHUNK;

      for (int16_t row = 0; row < rows; ++row) {
        const uint32_t sourceRow =
            static_cast<uint32_t>(chunkTop + row) * BEE_LINE_BACKGROUND_WIDTH;
        const uint16_t destinationRow =
            static_cast<uint16_t>(row) * BEE_LINE_BACKGROUND_WIDTH;
        for (int16_t column = 0; column < BEE_LINE_BACKGROUND_WIDTH; ++column) {
          transferBuffer_[destinationRow + column] =
              pgm_read_word(&BEE_LINE_BACKGROUND_PIXELS[sourceRow + column]);
        }
      }

      display.pushImage(0, chunkTop, BEE_LINE_BACKGROUND_WIDTH, rows, transferBuffer_);
      chunkTop += rows;
    }
  }

 private:
  static constexpr uint8_t ROWS_PER_CHUNK = 4;
  uint16_t transferBuffer_[BEE_LINE_BACKGROUND_WIDTH * ROWS_PER_CHUNK] = {};

  static bool isLandscape(TFT_eSPI& display) {
    return display.width() == BEE_LINE_BACKGROUND_WIDTH &&
           display.height() == BEE_LINE_BACKGROUND_HEIGHT;
  }
};
