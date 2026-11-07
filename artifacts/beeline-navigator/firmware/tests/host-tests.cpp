// Compile the production sketch, not a reimplementation of its logic.
// ArduinoJson is real; hardware interfaces below are deliberately simulated.
#include <Arduino.h>
#include "../beeline-esp32.ino"
#include <iostream>

static void write(const std::string& raw) {
  fakeCharacteristic.value = raw;
  NimBLEConnInfo info;
  const int draws = display.calls;
  inBleWrite = true;
  fakeCharacteristic.callbacks->onWrite(&fakeCharacteristic, info);
  inBleWrite = false;
  assert(display.calls == draws);
}
static void tick(uint32_t delta = 100) { fakeNow += delta; loop(); }
static void reject(const std::string& raw) {
  assert(!navigationPending && !speedPending);
  const String road = currentInstruction.road;
  const auto distance = currentInstruction.distanceMeters;
  const bool distanceAvailable = currentInstruction.distanceAvailable;
  const float speed = currentSpeedKmh;
  const bool available = speedAvailable;
  const uint32_t timestamp = lastSpeedUpdateMs;
  const int draws = display.calls;
  write(raw);
  assert(!navigationPending && !speedPending);
  loop();
  assert(currentInstruction.road == road && currentInstruction.distanceMeters == distance);
  assert(currentInstruction.distanceAvailable == distanceAvailable);
  assert(currentSpeedKmh == speed && speedAvailable == available && lastSpeedUpdateMs == timestamp);
  assert(display.calls == draws);
}
#include "telemetry-checks.h"
#include "visual-checks.h"

int main() {
  static_assert(COLOR_BACKGROUND == 0x2104, "Use neutral dark gray");
  static_assert(COLOR_BLACK == 0x0000, "Use black speed-scale backing");
  static_assert(COLOR_AMBER == 0xF6A8, "Speed band one matches the BEELINE title");
  static_assert(COLOR_AMBER > COLOR_SPEED_MIDDLE &&
                COLOR_SPEED_MIDDLE > COLOR_SPEED_HIGH,
                "Speed bands get progressively darker");
  static_assert(COLOR_DAY_BACKGROUND == 0xFFFF && COLOR_DAY_AMBER < COLOR_AMBER,
                "Day palette uses white and a darker yellow");
  const float rawX[] = {100, 3900, 3900};
  const float rawY[] = {100, 100, 3900};
  const float screenX[] = {28, 292, 292};
  const float screenY[] = {34, 34, 226};
  TouchCalibration fitted;
  assert(TouchCalibration::fromThreePoints(rawX, rawY, screenX, screenY, fitted));
  int16_t mappedX, mappedY;
  assert(fitted.map(100, 100, 320, 240, mappedX, mappedY));
  assert(mappedX == 28 && mappedY == 34);
  assert(fitted.map(4080, 4070, 320, 240, mappedX, mappedY));
  assert(isThemeButtonHit(mappedX, mappedY));
  assert(isThemeButtonHit(308, 234));
  assert(!isThemeButtonHit(279, 234));
  assert(!isThemeButtonHit(308, 213));
  setup();
  assert(touchControllerReady && touchCalibrationActive);
  acceptTouchCalibrationPoint(TS_Point{100, 100, 700});
  acceptTouchCalibrationPoint(TS_Point{3900, 100, 700});
  acceptTouchCalibrationPoint(TS_Point{3900, 3900, 700});
  assert(!touchCalibrationActive && touchCalibration.isValid());
  touchCalibration = fitted;
  assert(!headingRemembered && !headingAvailable);
  const auto* advertisingConfig = NimBLEDevice::getAdvertising();
  assert(advertisingConfig->scanResponseEnabled);
  assert(advertisingConfig->nameInAdvertisement.empty());
  assert(advertisingConfig->nameInScanResponse == DEVICE_NAME);
  assert(advertisingConfig->serviceUUID == SERVICE_UUID);
  loop();
  assert(orientationCheckActive && display.fullDraws == 1);
  assert(display.lastFillScreenColor == COLOR_BACKGROUND);
  write(R"({"type":"navigation","maneuver":"slight_right","distanceMeters":350,"road":"Rua Haddock Lobo","timestamp":1761600000000})");
  write(R"({"type":"speed","speedKmh":72.4,"valid":true,"timestamp":1761600000000})");
  tick(1199);
  assert(orientationCheckActive && hasNavigationInstruction && speedAvailable && display.fullDraws == 1);
  tick(1);
  assert(!orientationCheckActive && display.fullDraws == 2);
  assert(display.lastFillScreenColor == COLOR_BACKGROUND);
  assert(std::none_of(display.horizontalLines.begin(), display.horizontalLines.end(),
      [](const auto& line) { return line.y == Layout::PANEL_TOP; }));
  assert(display.roundRectCalls == 0); // No outline around the Android connection prompt.
  assert(currentInstruction.maneuver == "slight_right" && currentInstruction.distanceMeters == 350);
  assert(currentInstruction.distanceAvailable);
  const std::vector<int> expected = {0, 20, 40, 50, 60, 80, 100, 120, 140};
  assert(display.graduations == expected);
  const int scaleWidth = display.width() - Layout::SCALE_X - Layout::MARGIN;
  const int mark50 = Layout::SCALE_X + scaleWidth * 50 / Layout::SCALE_MAX;
  const int mark100 = Layout::SCALE_X + scaleWidth * 100 / Layout::SCALE_MAX;
  const auto hasRectangle = [&](int x, int y, int width, int height, uint16_t color) {
    return std::any_of(display.rectangles.begin(), display.rectangles.end(),
        [&](const auto& rectangle) {
          return rectangle.x == x && rectangle.y == y && rectangle.width == width &&
                 rectangle.height == height && rectangle.color == color;
        });
  };
  assert(hasRectangle(0, Layout::PANEL_TOP, display.width(),
                      display.height() - Layout::PANEL_TOP, COLOR_BLACK));
  assert(hasRectangle(Layout::SCALE_X, Layout::BAR_Y, mark50 - Layout::SCALE_X,
                      Layout::BAR_HEIGHT, COLOR_SPEED_HIGH));
  assert(hasRectangle(mark50, Layout::BAR_Y, mark100 - mark50,
                      Layout::BAR_HEIGHT, COLOR_AMBER));
  assert(hasRectangle(mark100, Layout::BAR_Y,
                      display.width() - Layout::MARGIN - mark100,
                      Layout::BAR_HEIGHT, COLOR_SPEED_HIGH));
  for (const int mark : {mark50, mark100}) {
    assert(std::any_of(display.verticalLines.begin(), display.verticalLines.end(),
        [&](const auto& line) {
          return line.x == mark && line.y == Layout::BAR_Y - 7 &&
                 line.color == COLOR_AMBER;
        }));
    assert(std::any_of(display.textSamples.begin(), display.textSamples.end(),
        [&](const auto& sample) {
          return sample.y == Layout::SCALE_LABEL_Y &&
                 sample.value == std::to_string(mark == mark50 ? 50 : 100) &&
                 sample.color == COLOR_AMBER;
        }));
  }

  // The display's sun control toggles only on the lower-right touch target.
  fakeNow = 5000;
  lastTouchPollMs = 0;
  touchControllerReady = true;
  touchController.point = {4080, 4070, 700};
  touchController.pressed = true;
  touchWasDown = false;
  dayMode = false;
  pollTouchInput();
  assert(dayMode && screenDirty);
  assert(screenBackgroundColor() == COLOR_DAY_BACKGROUND);
  assert(panelBackgroundColor() == COLOR_DAY_BACKGROUND);
  assert(mainTextColor() == COLOR_BLACK);
  assert(highlightColor() == COLOR_DAY_AMBER);
  assert(COLOR_SPEED_MIDDLE == 0xB4A4); // Intermediate yellows are unchanged.
  pollTouchInput(); // A held press must not toggle repeatedly.
  assert(dayMode);
  drawCurrentScreen();
  assert(display.lastFillScreenColor == COLOR_DAY_BACKGROUND);
  touchController.pressed = false;
  fakeNow += TOUCH_POLL_INTERVAL_MS;
  pollTouchInput();
  assert(!touchWasDown);
  touchController.pressed = true;
  fakeNow += TOUCH_POLL_INTERVAL_MS;
  pollTouchInput();
  assert(!dayMode);
  touchController.pressed = false;
  fakeNow += TOUCH_POLL_INTERVAL_MS;
  pollTouchInput();
  assert(screenBackgroundColor() == COLOR_BACKGROUND);
  screenDirty = speedPanelDirty = sidePanelDirty = false;
  lastScreenUpdateMs = fakeNow;

  // Empty, oversized, malformed, wrong types, NULs, non-object and trailing input.
  for (const std::string& raw : std::vector<std::string>{
      "", std::string(181, ' '), "{", "null", "[]", "42",
      R"({"type":false})", R"({"type":"other"})",
      R"({"type":"speed","valid":"true","speedKmh":80})",
      R"({"type":"speed","valid":1,"speedKmh":80})",
      R"({"type":"speed","speedKmh":80})",
      R"({"type":"speed","valid":true,"speedKmh":"80"})",
      R"({"type":"speed","valid":true,"speedKmh":true})",
      R"({"type":"speed","valid":true,"speedKmh":null})",
      R"({"type":"speed","valid":false,"speedKmh":[]})",
      R"({"type":"navigation","maneuver":false})",
      R"({"type":"navigation","maneuver":""})",
      R"({"type":"navigation","maneuver":"left","road":null})",
      R"({"type":"navigation","maneuver":"left","road":15})",
      R"({"type":"navigation","maneuver":"left","distanceMeters":"12"})",
      R"({"type":"navigation","maneuver":"left","distanceMeters":true})",
      R"({"type":"navigation","maneuver":"left","distanceMeters":12.5})",
      R"({"type":"navigation","maneuver":"left","distanceMeters":null})",
      R"({"type":"navigation","maneuver":"left\u0000right"})",
      R"({"type":"navigation","maneuver":"left","road":"a\u0000b"})",
      R"({"type":"speed\u0000x","valid":true,"speedKmh":80})",
      R"({"type":"speed","valid":true,"speedKmh":80}garbage)",
      R"({"type":"speed","valid":true,"speedKmh":80}{})",
      R"({"type":"speed","valid":true,"speedKmh":80,"extra":[[[[[]]]]]})",
      std::string(R"({"type":"speed","valid":true,"speedKmh":80})") + '\0'}) {
    reject(raw);
  }
  std::cout << "PASS: packet rejection preserves state\n";

  // Depth five is accepted; exactly 180 bytes accepted, 181 rejected.
  std::string boundary = R"({"type":"speed","valid":true,"speedKmh":72.4,"extra":[[[[]]]]})";
  boundary.resize(180, ' ');
  write(boundary); tick();
  assert(speedAvailable && currentSpeedKmh == 72.4f);
  reject(boundary + " ");
  // Rejection also must not clear a previously queued, valid command.
  write(R"({"type":"navigation","maneuver":"left","road":"Burst","distanceMeters":42})");
  write("{");
  for (int i = 0; i < 10000; ++i) {
    write("{\"type\":\"speed\",\"valid\":true,\"speedKmh\":" + std::to_string(i % 140) + "}");
  }
  assert(navigationPending && speedPending);
  tick();
  assert(currentInstruction.road == "Burst" && currentInstruction.distanceMeters == 42);
  assert(currentSpeedKmh == 59.0f);
  write(R"({"type":"navigation","maneuver":"left","road":"First"})");
  write(R"({"type":"navigation","maneuver":"right","road":"Latest"})");
  tick();
  assert(currentInstruction.road == "Latest" && currentInstruction.maneuver == "right");
  std::cout << "PASS: bounded independent latest-wins mailboxes and depth/size boundaries\n";

  // Repeated visible content refreshes freshness without redrawing.
  const int repeatedDraws = display.calls;
  write(R"({"type":"navigation","maneuver":"right","road":"Latest"})");
  write(R"({"type":"speed","valid":true,"speedKmh":59})");
  tick();
  assert(display.calls == repeatedDraws);
  const uint32_t refreshedAt = lastSpeedUpdateMs;
  tick(7899);
  assert(speedAvailable);
  tick(1);
  assert(!speedAvailable && currentSpeedKmh == 0 && lastSpeedUpdateMs == refreshedAt);
  assert(display.text.back() == "140"); // No speed marker when unavailable.
  assert(std::find(display.text.begin(), display.text.end(), "--") != display.text.end());
  tick(100);
  for (const std::string& raw : std::vector<std::string>{
      R"({"type":"speed","valid":false,"speedKmh":null})",
      R"({"type":"speed","valid":true,"speedKmh":-2})",
      R"({"type":"speed","valid":true,"speedKmh":1e100})"}) {
    write(R"({"type":"speed","valid":true,"speedKmh":50})"); tick();
    write(raw); tick();
    assert(!speedAvailable && currentSpeedKmh == 0);
  }
  // Delayed processing cannot revive an eight-second-old pending sample.
  write(R"({"type":"speed","valid":true,"speedKmh":50})");
  tick(8000);
  assert(!speedAvailable);
  // Timestamp arithmetic remains correct across millis() wraparound.
  fakeNow = UINT32_MAX - 4000;
  write(R"({"type":"speed","valid":true,"speedKmh":50})");
  loop(); tick(7999); assert(speedAvailable); tick(1); assert(!speedAvailable);
  std::cout << "PASS: invalid readings, duplicate suppression, eight-second timeout and clock wrap\n";

  tick(100);
  write(R"({"type":"speed","valid":true,"speedKmh":80})");
  loop();
  const int panels = display.panelDraws;
  const size_t rectanglesBeforeSpeedRedraw = display.rectangles.size();
  write(R"({"type":"speed","valid":true,"speedKmh":81})");
  tick(20);
  assert(display.panelDraws == panels);
  tick(80);
  assert(display.panelDraws == panels + 1);
  bool restoredBlackPanel = false;
  bool blackSpeedTrack = false;
  for (size_t i = rectanglesBeforeSpeedRedraw; i < display.rectangles.size(); ++i) {
    const auto& rectangle = display.rectangles[i];
    restoredBlackPanel |= rectangle.x == 0 && rectangle.y == Layout::PANEL_TOP &&
        rectangle.width == display.width() &&
        rectangle.height == display.height() - Layout::PANEL_TOP &&
        rectangle.color == COLOR_BLACK;
    blackSpeedTrack |= rectangle.x == Layout::SCALE_X - Layout::SCALE_BACKING_PADDING &&
        rectangle.y == Layout::BAR_Y - Layout::SCALE_BACKING_PADDING &&
        rectangle.width == display.width() - Layout::SCALE_X - Layout::MARGIN +
                               2 * Layout::SCALE_BACKING_PADDING &&
        rectangle.height == Layout::BAR_HEIGHT + 2 * Layout::SCALE_BACKING_PADDING &&
        rectangle.color == COLOR_BLACK;
  }
  assert(restoredBlackPanel && blackSpeedTrack);
  const int unchanged = display.calls;
  write(R"({"type":"speed","valid":true,"speedKmh":81.01})"); tick();
  assert(display.calls == unchanged);
  // Numbers above the ruler remain unmodified, while only the marker clamps.
  write(R"({"type":"speed","valid":true,"speedKmh":180})"); tick();
  assert(currentSpeedKmh == 180 && speedMarker(180) == speedMarker(140));
  assert(std::find(display.text.begin(), display.text.end(), "180") != display.text.end());
  const int beforeCombined = display.fullDraws;
  const int panelsBeforeCombined = display.panelDraws;
  write(R"({"type":"navigation","maneuver":"left","road":"Combined"})");
  write(R"({"type":"speed","valid":true,"speedKmh":0})"); tick();
  assert(currentSpeedKmh == 0 && speedAvailable);
  assert(display.fullDraws == beforeCombined + 1 && display.panelDraws == panelsBeforeCombined + 1);

  for (int rotation = 0; rotation < 4; ++rotation) {
    Serial.send("r" + std::to_string(rotation) + "\r\n");
    loop();
    assert(activeDisplayRotation == rotation && orientationCheckActive);
    const int full = display.fullDraws;
    write(R"({"type":"navigation","maneuver":"uturn","road":"During rotation","distanceMeters":777})");
    write(R"({"type":"speed","valid":false,"speedKmh":null})");
    tick(1199);
    assert(display.fullDraws == full && orientationCheckActive);
    tick(1);
    assert(!orientationCheckActive && display.fullDraws == full + 1);
    assert(currentInstruction.road == "During rotation" && currentInstruction.distanceMeters == 777);
    assert(!speedAvailable);
  }
  Serial.send("xxxxr1\nr9\n"); loop();
  assert(activeDisplayRotation == 3); // Overlong suffix must not become a command.
  Serial.send(std::string(80, 'x') + "\nr1\n");
  loop(); assert(Serial.available() > 0); // Serial input work is bounded per loop.
  while (Serial.available()) loop();
  assert(activeDisplayRotation == 1);
  tick(1200);
  write(R"({"type":"navigation","maneuver":"straight","distanceMeters":999999})"); tick();
  assert(currentInstruction.distanceMeters == 65535 && currentInstruction.road == "");
  write(R"({"type":"navigation","maneuver":"straight","distanceMeters":-10})"); tick();
  assert(currentInstruction.distanceMeters == 0);
  for (const char* maneuver : {"straight", "slight_left", "left", "slight_right", "right", "uturn"}) {
    write(std::string("{\"type\":\"navigation\",\"maneuver\":\"") + maneuver + "\"}");
    tick();
    assert(currentInstruction.maneuver == maneuver);
    const int index = maneuverIconIndex(maneuver);
    assert(index >= 0 && index < NavigationFont::kCount);
    const auto& bitmap = display.bitmaps.back();
    assert(bitmap.data == NavigationFont::kMasks[index]);
    assert(bitmap.color == COLOR_AMBER);
    assert(bitmap.x >= 0 && bitmap.x + bitmap.width <= navigationWidth());
    assert(bitmap.y > Layout::DIRECTION_Y && bitmap.y + bitmap.height < Layout::DISTANCE_Y);
  }
  static_assert(sizeof(NavigationFont::kMasks) == 6912, "Keep masks in flash, no framebuffer");
  assert(NavigationFont::kCodepoints[maneuverIconIndex("straight")] == 0xE91E);
  assert(NavigationFont::kCodepoints[maneuverIconIndex("left")] == 0xE922);
  assert(NavigationFont::kCodepoints[maneuverIconIndex("right")] == 0xE921);
  assert(NavigationFont::kCodepoints[maneuverIconIndex("uturn")] == 0xE92D);
  const auto bitmapsBeforeUnknown = display.bitmaps.size();
  write(R"({"type":"navigation","maneuver":"unrecognized"})"); tick();
  assert(maneuverIconIndex(currentInstruction.maneuver) == -1);
  assert(display.bitmaps.size() == bitmapsBeforeUnknown);
  assert(display.textSamples.back().value != "straight");
  assert(std::find(display.text.begin(), display.text.end(), "?") != display.text.end());
  // Unknown and straight must not share the old angle-based redraw identity.
  const int fullBeforeStraight = display.fullDraws;
  write(R"({"type":"navigation","maneuver":"straight"})"); tick();
  assert(display.fullDraws == fullBeforeStraight + 1);
  assert(display.bitmaps.back().data == NavigationFont::kMasks[0]);
  for (int rotation = 0; rotation < 4; ++rotation) {
    display.rotation = rotation;
    drawManeuverIcon("left");
    const auto& sample = display.bitmaps.back();
    assert(sample.x >= 0 && sample.x + sample.width <= navigationWidth());
    assert(sample.y >= 0 && sample.y + sample.height < Layout::PANEL_TOP);
  }
  display.rotation = activeDisplayRotation;
  std::cout << "PASS: six mapped TTF masks, flash budget, bounds and neutral unknown\n";
  // This is the contextual command emitted by the Android parser (no turn distance).
  const int drawsBeforeText = display.fullDraws;
  const auto iconsBeforeText = display.bitmaps.size();
  display.textSamples.clear();
  write(R"({"type":"navigation","maneuver":"text","road":"em direcao a R. Exemplo","timestamp":42})");
  tick();
  assert(display.fullDraws == drawsBeforeText + 1); // Erases the previous arrow.
  assert(currentInstruction.maneuver == "text" && hasNavigationInstruction);
  assert(!currentInstruction.distanceAvailable);
  assert(display.bitmaps.size() == iconsBeforeText);
  bool sawGuidance = false;
  for (const auto& sample : display.textSamples) {
    if (sample.x < navigationWidth() && sample.y < Layout::PANEL_TOP) {
      assert(sample.value != "0 m" && sample.value != "4300 m" && sample.value != "?");
      if (sample.value == "em direcao a R. Exemplo") sawGuidance = true;
      if (sample.y >= 47 && sample.y <= 125)
        assert(display.textWidth(String(sample.value), sample.font) <= navigationWidth() - 16);
    }
  }
  assert(sawGuidance);
  const int textDraws = display.fullDraws;
  write(R"({"type":"navigation","maneuver":"text","road":"em direcao a R. Exemplo","timestamp":43})");
  tick();
  assert(display.fullDraws == textDraws);
  reject(R"({"type":"navigation","maneuver":"text","road":""})");
  display.textSamples.clear();
  write(R"({"type":"navigation","maneuver":"text","distanceMeters":250,"road":"em direcao a R. Exemplo","timestamp":44})");
  tick();
  assert(currentInstruction.distanceAvailable && currentInstruction.distanceMeters == 250);
  bool sawTextDistance = false;
  for (const auto& sample : display.textSamples) {
    if (sample.value == "250 m" && sample.y == Layout::TEXT_DISTANCE_Y) sawTextDistance = true;
  }
  assert(sawTextDistance);
  display.textSamples.clear();
  write(R"({"type":"navigation","maneuver":"text","distanceMeters":0,"road":"em direcao a R. Exemplo","timestamp":45})");
  tick();
  bool sawZeroTextDistance = false;
  for (const auto& sample : display.textSamples) {
    if (sample.value == "0 m" && sample.y == Layout::TEXT_DISTANCE_Y) sawZeroTextDistance = true;
  }
  assert(sawZeroTextDistance);
  display.textSamples.clear();
  drawTextGuidance(String(std::string(170, 'A')));
  for (const auto& sample : display.textSamples) {
    if (sample.y < 165) {
      assert(display.textWidth(String(sample.value), sample.font) <= navigationWidth() - 16);
      assert(sample.y < Layout::PANEL_TOP);
    }
  }
  write(R"({"type":"navigation","maneuver":"right","distanceMeters":200,"road":"Rua Exemplo"})");
  tick();
  assert(display.bitmaps.size() == iconsBeforeText + 1);
  assert(display.bitmaps.back().data == NavigationFont::kMasks[maneuverIconIndex("right")]);
  std::cout << "PASS: contextual text replaces arrow, wraps, omits fake distance and restores maneuvers\n";
  NimBLEConnInfo info;
  NimBLEServer* server = NimBLEDevice::createServer();
  const int advertising = NimBLEDevice::getAdvertising()->starts;
  server->callbacks->onConnect(server, info);
  server->callbacks->onDisconnect(server, info, 0);
  assert(NimBLEDevice::getAdvertising()->starts == advertising + 1);
  assert(hasNavigationInstruction);
  const size_t idlePanelStart = display.rectangles.size();
  const int roundRectanglesBeforeIdle = display.roundRectCalls;
  drawIdleScreen();
  assert(std::none_of(display.horizontalLines.begin(), display.horizontalLines.end(),
      [](const auto& line) { return line.y == Layout::PANEL_TOP; }));
  assert(display.roundRectCalls == roundRectanglesBeforeIdle);
  assert(std::find(display.text.begin(), display.text.end(),
                   "Conecte pelo app Android") != display.text.end());
  assert(std::any_of(display.rectangles.begin() + idlePanelStart, display.rectangles.end(),
      [](const auto& rectangle) {
        return rectangle.x == 0 && rectangle.y == Layout::PANEL_TOP &&
               rectangle.width == display.width() &&
               rectangle.height == display.height() - Layout::PANEL_TOP &&
               rectangle.color == COLOR_BLACK;
      }));
  std::cout << "PASS: dark upper screen, full black speed panel and speed-panel-only refresh\n";
  std::cout << "PASS: refresh limit, r0-r3, diagnostic restoration, maneuvers and advertising restart\n";
  runTelemetryChecks();
  runVisualChecks();
  std::cout << "All host firmware checks passed (hardware APIs simulated).\n";
  std::cout << "PASS: explicit service UUID and device name in scan response\n";
}
