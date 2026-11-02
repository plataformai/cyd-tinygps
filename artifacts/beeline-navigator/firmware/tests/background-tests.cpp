#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../beeline_background_renderer.h"
#include "../beeline_color_test.h"

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

  // Diagnostic compares identical logical pixels with false/true byte swap.
  display.rotation = 1;
  for (const bool initialSwap : {false, true}) {
    display.rectangles.clear();
    display.images.clear();
    display.setSwapBytes(initialSwap);
    BeeLineColorTest::draw(display);
    assert(display.getSwapBytes() == initialSwap);
    assert(display.rectangles.size() == 6);
    assert(display.images.size() == 6 * 2 * BeeLineColorTest::SWATCH_HEIGHT);
    for (size_t i = 0; i < 6; ++i) {
      const auto& reference = BeeLineColorTest::REFERENCES[i];
      assert(display.rectangles[i].x == BeeLineColorTest::DIRECT_X);
      assert(display.rectangles[i].color == reference.rgb565);
      for (int variant = 0; variant < 2; ++variant) {
        for (int row = 0; row < BeeLineColorTest::SWATCH_HEIGHT; ++row) {
          const auto& image = display.images[
              (i * 2 + variant) * BeeLineColorTest::SWATCH_HEIGHT + row];
          assert(image.color == reference.rgb565 && image.uniform);
          assert(image.swapped == (variant == 1));
          assert(image.x == (variant ? BeeLineColorTest::SWAP_X : BeeLineColorTest::NORMAL_X));
          assert(image.y == BeeLineColorTest::FIRST_Y + int(i) * BeeLineColorTest::ROW_STEP + row);
        }
      }
    }
  }
  return 0;
}
