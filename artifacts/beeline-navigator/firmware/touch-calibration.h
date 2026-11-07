#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Affine conversion from raw XPT2046 samples to the current display rotation.
// Three non-collinear targets are sufficient to calibrate both screen axes.
struct TouchCalibration {
  float xRawX = 0;
  float xRawY = 0;
  float xOffset = 0;
  float yRawX = 0;
  float yRawY = 0;
  float yOffset = 0;

  bool isValid() const {
    const float values[] = {xRawX, xRawY, xOffset, yRawX, yRawY, yOffset};
    for (const float value : values) {
      if (!std::isfinite(value)) return false;
    }
    int16_t x, y;
    return map(0, 0, 320, 240, x, y) &&
           map(4095, 4095, 320, 240, x, y) &&
           map(0, 4095, 320, 240, x, y) &&
           map(4095, 0, 320, 240, x, y);
  }

  bool map(float rawX, float rawY, int16_t width, int16_t height,
           int16_t& screenX, int16_t& screenY) const {
    if (width <= 0 || height <= 0) return false;
    const float mappedX = xRawX * rawX + xRawY * rawY + xOffset;
    const float mappedY = yRawX * rawX + yRawY * rawY + yOffset;
    if (!std::isfinite(mappedX) || !std::isfinite(mappedY) ||
        mappedX < -100 || mappedX > width + 100 ||
        mappedY < -100 || mappedY > height + 100) return false;
    screenX = static_cast<int16_t>(std::clamp(mappedX, 0.0f, float(width - 1)));
    screenY = static_cast<int16_t>(std::clamp(mappedY, 0.0f, float(height - 1)));
    return true;
  }

  static bool fromThreePoints(const float rawX[3], const float rawY[3],
                              const float screenX[3], const float screenY[3],
                              TouchCalibration& result) {
    const double determinant =
        rawX[0] * (rawY[1] - rawY[2]) +
        rawX[1] * (rawY[2] - rawY[0]) +
        rawX[2] * (rawY[0] - rawY[1]);
    if (!std::isfinite(determinant) || std::fabs(determinant) < 1000) return false;

    const auto solve = [&](const float target[3], float& a, float& b, float& c) {
      a = static_cast<float>(
          (target[0] * (rawY[1] - rawY[2]) +
           target[1] * (rawY[2] - rawY[0]) +
           target[2] * (rawY[0] - rawY[1])) / determinant);
      b = static_cast<float>(
          (target[0] * (rawX[2] - rawX[1]) +
           target[1] * (rawX[0] - rawX[2]) +
           target[2] * (rawX[1] - rawX[0])) / determinant);
      c = static_cast<float>(
          (target[0] * (rawX[1] * rawY[2] - rawX[2] * rawY[1]) +
           target[1] * (rawX[2] * rawY[0] - rawX[0] * rawY[2]) +
           target[2] * (rawX[0] * rawY[1] - rawX[1] * rawY[0])) / determinant);
    };

    solve(screenX, result.xRawX, result.xRawY, result.xOffset);
    solve(screenY, result.yRawX, result.yRawY, result.yOffset);
    return result.isValid();
  }
};
