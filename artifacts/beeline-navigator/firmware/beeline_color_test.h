#pragma once

#include <stdint.h>
#include <TFT_eSPI.h>

namespace BeeLineColorTest {
constexpr uint32_t DURATION_MS = 30000;
constexpr int16_t SWATCH_WIDTH = 78, SWATCH_HEIGHT = 24;
constexpr int16_t DIRECT_X = 52, NORMAL_X = 136, SWAP_X = 220;
constexpr int16_t COLUMN_X[] = {DIRECT_X, NORMAL_X, SWAP_X};
constexpr int16_t FIRST_Y = 40, ROW_STEP = 28;
struct Reference {
  const char* name;
  uint16_t rgb565;
};
constexpr Reference REFERENCES[] = {
    {"PRETO", 0x0000}, {"BRANCO", 0xFFFF}, {"VERM.", 0xF800},
    {"VERDE", 0x07E0}, {"AZUL", 0x001F}, {"ESCURO", 0x1124}};

// Same numeric RGB565 pixels in all three columns; only the pushImage byte
// order changes. A single 156-byte scanline avoids a framebuffer allocation.
inline void draw(TFT_eSPI& display) {
  const bool previousSwap = display.getSwapBytes();
  uint16_t scanline[SWATCH_WIDTH];
  display.fillScreen(0x0000);
  display.setTextColor(0xFFFF);
  display.setTextDatum(TC_DATUM);
  display.drawString("TESTE RGB565", 160, 3, 2);
  display.drawString("DIRETO", DIRECT_X + SWATCH_WIDTH / 2, 24, 1);
  display.drawString("PUSH NORMAL", NORMAL_X + SWATCH_WIDTH / 2, 24, 1);
  display.drawString("PUSH SWAP", SWAP_X + SWATCH_WIDTH / 2, 24, 1);

  int16_t y = FIRST_Y;
  for (const Reference& reference : REFERENCES) {
    display.setTextDatum(TL_DATUM);
    display.drawString(reference.name, 2, y + 8, 1);
    display.fillRect(DIRECT_X, y, SWATCH_WIDTH, SWATCH_HEIGHT, reference.rgb565);
    for (uint8_t variant = 0; variant < 2; ++variant) {
      display.setSwapBytes(variant == 1);
      const int16_t x = variant == 0 ? NORMAL_X : SWAP_X;
      for (int16_t row = 0; row < SWATCH_HEIGHT; ++row) {
        for (int16_t column = 0; column < SWATCH_WIDTH; ++column) {
          scanline[column] = reference.rgb565;
        }
        display.pushImage(x, y + row, SWATCH_WIDTH, 1, scanline);
      }
    }
    // Borders make even the black swatches visible.
    for (const int16_t x : COLUMN_X) {
      display.drawRect(x, y, SWATCH_WIDTH, SWATCH_HEIGHT, 0xFFFF);
    }
    y += ROW_STEP;
  }
  display.setSwapBytes(previousSwap);
  display.setTextDatum(TC_DATUM);
  display.drawString("Qual PUSH combina com DIRETO?", 160, 212, 1);
  display.drawString("Volta em 30s; c repete; r1 sai", 160, 227, 1);
}
}  // namespace BeeLineColorTest
