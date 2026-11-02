#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../beeline_background_renderer.h"

int main() {
  constexpr uint16_t FALLBACK = 0x1124;
  TFT_eSPI display;
  BeeLineBackgroundRenderer renderer;

  renderer.draw(display, FALLBACK);
  assert(display.imageRows == BEE_LINE_BACKGROUND_HEIGHT);
  assert(display.imageWrites ==
         (BEE_LINE_BACKGROUND_HEIGHT + 3) / 4);
  assert(display.fullDraws == 1);
  assert(display.firstImagePixel ==
         pgm_read_word(&BEE_LINE_BACKGROUND_PIXELS[0]));
  assert(display.lastImagePixel == pgm_read_word(
      &BEE_LINE_BACKGROUND_PIXELS[BEE_LINE_BACKGROUND_WIDTH *
                                  BEE_LINE_BACKGROUND_HEIGHT - 1]));

  const int previousRows = display.imageRows;
  const int previousWrites = display.imageWrites;
  renderer.drawRegion(display, 193, 47, FALLBACK);
  assert(display.imageRows == previousRows + 47);
  assert(display.imageWrites == previousWrites + 12);

  const int rowsBeforeClippedDraw = display.imageRows;
  renderer.drawRegion(display, -5, 10, FALLBACK);
  assert(display.imageRows == rowsBeforeClippedDraw + 5);

  display.rotation = 0;
  const int rowsBeforePortrait = display.imageRows;
  renderer.draw(display, FALLBACK);
  assert(display.imageRows == rowsBeforePortrait);
  assert(display.lastFillColor == FALLBACK);
  renderer.drawRegion(display, 193, 47, FALLBACK);
  assert(display.imageRows == rowsBeforePortrait);
  assert(display.lastFillColor == FALLBACK);

  return 0;
}
