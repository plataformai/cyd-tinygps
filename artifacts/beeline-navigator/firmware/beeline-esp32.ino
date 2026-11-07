/*
 * PetitGPS ESP32 BLE receiver
 *
 * Arduino IDE libraries:
 *   - NimBLE-Arduino 2.x
 *   - ArduinoJson 7.x
 *   - TFT_eSPI
 *
 * BLE GATT:
 *   Service:         7b1e0001-6d2a-4f7a-9c21-8e8a4b12a001
 *   Write command:   7b1e0002-6d2a-4f7a-9c21-8e8a4b12a001
 *
 * TFT setup in the companion README matches the dual-USB ESP32-2432S028
 * confirmed for this project. Check other board revisions before powering up.
 */

#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <freertos/FreeRTOS.h>
#include <math.h>
#include <limits.h>
#include "navigation-font.h"
#include "touch-calibration.h"

static constexpr char DEVICE_NAME[] = "PetitGPS-ESP32";
static constexpr char SERVICE_UUID[] = "7b1e0001-6d2a-4f7a-9c21-8e8a4b12a001";
static constexpr char COMMAND_UUID[] = "7b1e0002-6d2a-4f7a-9c21-8e8a4b12a001";
static constexpr uint8_t TFT_BACKLIGHT_PIN = 21;
static constexpr uint8_t TOUCH_CS_PIN = 33;
static constexpr uint8_t TOUCH_IRQ_PIN = 36;
static constexpr uint8_t TOUCH_MOSI_PIN = 32;
static constexpr uint8_t TOUCH_MISO_PIN = 39;
static constexpr uint8_t TOUCH_SCLK_PIN = 25;
static constexpr uint8_t DEFAULT_DISPLAY_ROTATION = 1;
static constexpr int16_t DISPLAY_LANDSCAPE_WIDTH = 320;
static constexpr int16_t DISPLAY_LANDSCAPE_HEIGHT = 240;
static constexpr uint16_t DISPLAY_ORIENTATION_CHECK_MS = 1200;
static constexpr size_t MAX_COMMAND_BYTES = 180;
static constexpr uint8_t MAX_JSON_DEPTH = 5;
static constexpr uint32_t SPEED_TIMEOUT_MS = 8000;
static constexpr uint32_t HEADING_TIMEOUT_MS = 8000;
static constexpr uint32_t ROUTE_TIMEOUT_MS = 30000;
static constexpr uint32_t MAX_ROUTE_METERS = 40000000;
static constexpr uint32_t SCREEN_UPDATE_INTERVAL_MS = 100;
static constexpr uint32_t TOUCH_POLL_INTERVAL_MS = 20;
static constexpr uint8_t SERIAL_BYTES_PER_LOOP = 16;
namespace Layout {
constexpr int16_t PANEL_TOP = 193;
constexpr int16_t MARGIN = 12;
constexpr int16_t SPEED_VALUE_X = 13, SPEED_VALUE_MAX_WIDTH = 45;
constexpr int16_t SPEED_TO_SCALE_GAP = 10;
constexpr int16_t SCALE_X = SPEED_VALUE_X + SPEED_VALUE_MAX_WIDTH + SPEED_TO_SCALE_GAP;
constexpr int16_t BAR_Y = 207;
constexpr int16_t BAR_HEIGHT = 10;
constexpr int16_t SCALE_BUTTON_CLEARANCE = 32;
constexpr int16_t MARKER_WIDTH = 2, MARKER_TOP_OFFSET = 8;
constexpr int16_t MARKER_TRIANGLE_HALF_WIDTH = 4, MARKER_TRIANGLE_HEIGHT = 6;
constexpr int16_t SCALE_BACKING_PADDING = 4;
constexpr int16_t SPEED_VALUE_Y = 195, SPEED_UNIT_X = 15, SPEED_UNIT_Y = 225;
constexpr int16_t SCALE_LABEL_Y = 222;
constexpr int16_t THEME_BUTTON_CENTER_Y = BAR_Y + BAR_HEIGHT / 2;
constexpr int16_t THEME_ICON_EDGE = 11, THEME_TOUCH_RADIUS = 17;
constexpr int16_t FONT1_CHAR_WIDTH = 6;
constexpr uint16_t SCALE_MAX = 140, GREEN_LIMIT = 50, YELLOW_LIMIT = 100;
constexpr uint16_t GRADUATIONS[] = {0, 20, 40, 50, 60, 80, 100, 120, 140};
constexpr int16_t ICON_CENTER_Y = 81;
constexpr int16_t DIRECTION_Y = 5, DISTANCE_Y = 137, ROAD_Y = 171;
constexpr unsigned int ROAD_MAX_BYTES = 23, ROAD_PREFIX_BYTES = 20;
constexpr int16_t TEXT_DISTANCE_Y = 143, TEXT_STATUS_Y = 175;
constexpr int16_t CALIBRATION_TARGETS_X[] = {28, 292, 292};
constexpr int16_t CALIBRATION_TARGETS_Y[] = {34, 34, 226};
}
static constexpr uint16_t COLOR_BACKGROUND = 0x2104;  // neutral dark gray
static constexpr uint16_t COLOR_SURFACE = 0x1A2C;
static constexpr uint16_t COLOR_BLACK = 0x0000;
static constexpr uint16_t COLOR_AMBER = 0xF6A8;
static constexpr uint16_t COLOR_DAY_BACKGROUND = 0xFFFF;
static constexpr uint16_t COLOR_DAY_AMBER = 0x9B40;
static constexpr uint16_t COLOR_DAY_SPEED_IDLE = 0xDEB4;
static constexpr uint16_t COLOR_SPEED_MIDDLE = 0xB4A4;
static constexpr uint16_t COLOR_SPEED_HIGH = 0x7B24;
static constexpr uint16_t COLOR_TEXT = 0xFFFF;
static constexpr uint16_t COLOR_MUTED = 0x9DB1;
static constexpr uint16_t COLOR_RED = 0xF986;

struct NavigationInstruction {
  String maneuver;
  String road;
  uint16_t distanceMeters;
  bool distanceAvailable;
};

TFT_eSPI display = TFT_eSPI();
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen touchController(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
Preferences touchPreferences;
float currentSpeedKmh = 0.0f;
bool speedAvailable = false;
unsigned long lastSpeedUpdateMs = 0;
uint8_t activeDisplayRotation = DEFAULT_DISPLAY_ROTATION;
bool hasNavigationInstruction = false;
bool orientationCheckActive = false;
unsigned long orientationCheckStartedMs = 0;
NavigationInstruction currentInstruction;
char serialCommand[4] = {};
uint8_t serialCommandLength = 0;
bool serialCommandOverflow = false;
bool screenDirty = true;
bool speedPanelDirty = true;
bool sidePanelDirty = true;
bool headingAvailable = false, routeAvailable = false;
bool headingRemembered = false; // RAM only: last valid GPS course until reboot.
bool dayMode = false; // The requested night palette remains the boot default.
float currentHeading = 0;
uint32_t remainingMeters = 0, lastHeadingUpdateMs = 0, lastRouteUpdateMs = 0;
uint32_t routeValidityMs = ROUTE_TIMEOUT_MS;
uint32_t lastScreenUpdateMs = 0;
int8_t pendingRotation = DEFAULT_DISPLAY_ROTATION;
bool pendingRotationTest = false;
TouchCalibration touchCalibration;
bool touchCalibrationActive = false;
bool touchWasDown = false;
bool touchControllerReady = false;
uint8_t touchCalibrationPoint = 0;
uint8_t touchCalibrationRotation = DEFAULT_DISPLAY_ROTATION;
float touchRawX[3] = {};
float touchRawY[3] = {};
uint32_t lastTouchPollMs = 0;

// Independent, fixed-size latest-wins mailboxes. Speed bursts cannot evict
// navigation, heading or route. Only POD copies happen under this critical section:
// never JSON parsing, allocation, logging or TFT/serial IO.
struct PendingNavigation {
  char maneuver[MAX_COMMAND_BYTES + 1];
  char road[MAX_COMMAND_BYTES + 1];
  uint16_t distanceMeters;
  bool distanceAvailable;
};
struct PendingSpeed {
  float kmh;
  bool available;
  uint32_t receivedAtMs;
};
struct PendingHeading {
  float degrees;
  bool available;
  uint32_t receivedAtMs;
};
struct PendingRoute {
  uint32_t meters;
  bool available;
  uint32_t receivedAtMs, ttlMs;
};
portMUX_TYPE pendingMux = portMUX_INITIALIZER_UNLOCKED;
PendingNavigation pendingNavigation = {};
PendingSpeed pendingSpeed = {};
bool navigationPending = false;
bool speedPending = false;
PendingHeading pendingHeading = {};
PendingRoute pendingRoute = {};
bool headingPending = false, routePending = false;

// ArduinoJson stops at the closing brace. Track consumption to reject trailing
// garbage/second documents and embedded NULs instead of accepting a valid prefix.
struct CommandReader {
  const std::string& raw;
  size_t position = 0;
  explicit CommandReader(const std::string& value) : raw(value) {}
  int read() {
    return position < raw.size() ? static_cast<uint8_t>(raw[position++]) : -1;
  }
  size_t readBytes(char* buffer, size_t length) {
    size_t count = 0;
    while (count < length && position < raw.size()) buffer[count++] = raw[position++];
    return count;
  }
  bool onlyWhitespaceRemains() {
    while (position < raw.size()) {
      const char c = raw[position++];
      if (c != ' ' && c != '\t' && c != '\r' && c != '\n') return false;
    }
    return true;
  }
};

void drawIdleScreen();
void renderInstruction(const NavigationInstruction& instruction);
void drawCurrentScreen();
void drawBackground();
void applyDisplayRotation(uint8_t rotation, bool rotationTest);
void drawTouchCalibrationScreen();
void pollTouchInput();

int16_t navigationWidth() { return display.width() * 3 / 4; }

uint16_t screenBackgroundColor() {
  return dayMode ? COLOR_DAY_BACKGROUND : COLOR_BACKGROUND;
}

uint16_t panelBackgroundColor() {
  return dayMode ? COLOR_DAY_BACKGROUND : COLOR_BLACK;
}

uint16_t mainTextColor() {
  return dayMode ? COLOR_BLACK : COLOR_TEXT;
}

uint16_t highlightColor() {
  return dayMode ? COLOR_DAY_AMBER : COLOR_AMBER;
}

bool isThemeButtonHit(int16_t x, int16_t y) {
  const int16_t centerX = display.width() - Layout::MARGIN - Layout::THEME_ICON_EDGE;
  const int16_t dx = x - centerX;
  const int16_t dy = y - Layout::THEME_BUTTON_CENTER_Y;
  return dx * dx + dy * dy <=
         Layout::THEME_TOUCH_RADIUS * Layout::THEME_TOUCH_RADIUS;
}

void toggleDayNightMode() {
  dayMode = !dayMode;
  screenDirty = true;
  speedPanelDirty = true;
  sidePanelDirty = true;
  Serial.printf("DISPLAY_MODE=%s\n", dayMode ? "DAY" : "NIGHT");
}

float normalizedHeading(float degrees) {
  const float angle = fmodf(degrees, 360.0f);
  return angle < 0 ? angle + 360.0f : angle;
}

const char* headingCardinal(float degrees) {
  static const char* const names[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return names[static_cast<int>(floorf((normalizedHeading(degrees) + 22.5f) / 45.0f)) % 8];
}

void cardinalPosition(uint8_t index, float heading, int16_t& x, int16_t& y) {
  const float angle = (index * 90.0f - heading) * PI / 180.0f;
  const int16_t centerX = navigationWidth() + (display.width() - navigationWidth()) / 2;
  const float radius = (display.width() - navigationWidth()) / 2.0f - 8;
  x = centerX + static_cast<int16_t>(roundf(sinf(angle) * radius));
  y = 60 - static_cast<int16_t>(roundf(cosf(angle) * radius));
}

String remainingDistanceText() {
  if (!routeAvailable) return "--";
  char buffer[24];
  if (remainingMeters < 1000) snprintf(buffer, sizeof(buffer), "%lu m", static_cast<unsigned long>(remainingMeters));
  else if (remainingMeters < 100000) snprintf(buffer, sizeof(buffer), "%.1f km", remainingMeters / 1000.0);
  else snprintf(buffer, sizeof(buffer), "%lu km", static_cast<unsigned long>(remainingMeters / 1000));
  if (char* decimal = strchr(buffer, '.')) *decimal = ',';
  return String(buffer);
}

void drawThemeToggle() {
  const int16_t cx = display.width() - Layout::MARGIN - Layout::THEME_ICON_EDGE;
  const int16_t cy = Layout::THEME_BUTTON_CENTER_Y;
  const uint16_t glyphColor = highlightColor();
  if (dayMode) {
    // A crescent indicates that tapping will restore the night palette.
    display.fillCircle(cx, cy, 5, glyphColor);
    display.fillCircle(cx + 3, cy - 3, 5, panelBackgroundColor());
    display.drawCircle(cx, cy, 5, glyphColor);
  } else {
    display.drawCircle(cx, cy, 3, glyphColor);
    display.fillCircle(cx, cy, 2, glyphColor);
    display.drawFastVLine(cx, cy - 7, 2, glyphColor);
    display.drawFastVLine(cx, cy + 6, 2, glyphColor);
    display.drawFastHLine(cx - 7, cy, 2, glyphColor);
    display.drawFastHLine(cx + 6, cy, 2, glyphColor);
    display.drawLine(cx - 5, cy - 5, cx - 4, cy - 4, glyphColor);
    display.drawLine(cx + 4, cy + 4, cx + 5, cy + 5, glyphColor);
    display.drawLine(cx + 4, cy - 4, cx + 5, cy - 5, glyphColor);
    display.drawLine(cx - 5, cy + 5, cx - 4, cy + 4, glyphColor);
  }
}

void drawTouchCalibrationScreen() {
  drawBackground();
  display.setTextDatum(TC_DATUM);
  display.setTextColor(highlightColor());
  display.drawCentreString("CALIBRAR TOQUE", display.width() / 2, 10, 2);
  display.setTextColor(mainTextColor());
  display.drawCentreString("Toque no centro de cada X", display.width() / 2, 32, 1);
  display.setTextColor(COLOR_MUTED);
  display.drawCentreString(String(touchCalibrationPoint + 1) + " DE 3",
                           display.width() / 2, display.height() / 2 - 5, 1);

  const int16_t x = Layout::CALIBRATION_TARGETS_X[touchCalibrationPoint];
  const int16_t y = Layout::CALIBRATION_TARGETS_Y[touchCalibrationPoint];
  const uint16_t color = highlightColor();
  display.drawCircle(x, y, 9, color);
  display.drawFastHLine(x - 15, y, 31, color);
  display.drawFastVLine(x, y - 15, 31, color);
  display.fillCircle(x, y, 2, color);
}

bool loadTouchCalibration() {
  if (!touchPreferences.begin("bl_touch", true)) return false;
  TouchCalibration saved;
  const bool hasData =
      touchPreferences.getBytesLength("affine") == sizeof(saved) &&
      touchPreferences.getUChar("rotation", 255) == activeDisplayRotation &&
      touchPreferences.getBytes("affine", &saved, sizeof(saved)) == sizeof(saved);
  touchPreferences.end();
  if (!hasData || !saved.isValid()) return false;
  touchCalibration = saved;
  return true;
}

void saveTouchCalibration() {
  if (!touchPreferences.begin("bl_touch", false)) {
    Serial.println("[TOUCH] NVS indisponivel; recalibracao sera necessaria no proximo boot.");
    return;
  }
  const size_t written = touchPreferences.putBytes("affine", &touchCalibration,
                                                    sizeof(touchCalibration));
  touchPreferences.putUChar("rotation", activeDisplayRotation);
  touchPreferences.end();
  if (written != sizeof(touchCalibration)) {
    Serial.println("[TOUCH] Falha ao salvar calibracao; recalibracao sera necessaria no proximo boot.");
  } else {
    Serial.println("[TOUCH] Calibracao salva.");
  }
}

void initializeTouchController() {
  // The XPT2046 on this ESP32-2432S028 revision has its own SPI pins,
  // separate from the validated TFT_eSPI bus.
  touchSPI.begin(TOUCH_SCLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
  touchControllerReady = touchController.begin(touchSPI);
  touchController.setRotation(0);
  if (!touchControllerReady) {
    Serial.println("[TOUCH] Controlador XPT2046 nao inicializou.");
    return;
  }

  if (loadTouchCalibration()) {
    Serial.println("[TOUCH] Calibracao carregada.");
    return;
  }
  touchCalibrationActive = true;
  touchCalibrationPoint = 0;
  Serial.println("[TOUCH] Toque nos tres alvos para calibrar o modo dia/noite.");
}

void acceptTouchCalibrationPoint(const TS_Point& point) {
  touchRawX[touchCalibrationPoint] = point.x;
  touchRawY[touchCalibrationPoint] = point.y;
  ++touchCalibrationPoint;
  if (touchCalibrationPoint < 3) {
    screenDirty = true;
    return;
  }

  const float screenX[] = {
      static_cast<float>(Layout::CALIBRATION_TARGETS_X[0]),
      static_cast<float>(Layout::CALIBRATION_TARGETS_X[1]),
      static_cast<float>(Layout::CALIBRATION_TARGETS_X[2]),
  };
  const float screenY[] = {
      static_cast<float>(Layout::CALIBRATION_TARGETS_Y[0]),
      static_cast<float>(Layout::CALIBRATION_TARGETS_Y[1]),
      static_cast<float>(Layout::CALIBRATION_TARGETS_Y[2]),
  };
  if (!TouchCalibration::fromThreePoints(touchRawX, touchRawY, screenX, screenY,
                                         touchCalibration)) {
    Serial.println("[TOUCH] Calibracao invalida; iniciando novamente.");
    touchCalibrationPoint = 0;
    screenDirty = true;
    return;
  }
  saveTouchCalibration();
  touchCalibrationActive = false;
  screenDirty = true;
  Serial.println("[TOUCH] Calibracao concluida.");
}

void pollTouchInput() {
  if (!touchControllerReady || orientationCheckActive) return;
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastTouchPollMs) < TOUCH_POLL_INTERVAL_MS) return;
  lastTouchPollMs = now;

  if (!touchController.touched()) {
    touchWasDown = false;
    return;
  }
  if (touchWasDown) return;
  touchWasDown = true;
  const TS_Point point = touchController.getPoint();
  if (touchCalibrationActive) {
    acceptTouchCalibrationPoint(point);
    return;
  }

  int16_t x, y;
  if (touchCalibration.map(point.x, point.y, display.width(), display.height(), x, y) &&
      isThemeButtonHit(x, y)) {
    toggleDayNightMode();
  }
}

void drawSidePanel() {
  const int16_t left = navigationWidth();
  const int16_t width = display.width() - left;
  const int16_t cx = left + width / 2, cy = 60;
  display.fillRect(left, 0, width, Layout::PANEL_TOP, screenBackgroundColor());
  display.fillCircle(cx, cy, width / 2 - 18, COLOR_SPEED_MIDDLE);
  // Fixed north-up pointer shape; only cardinal positions orbit with the course.
  display.fillTriangle(cx, cy - 16, cx - 9, cy + 12, cx, cy + 7, screenBackgroundColor());
  display.fillTriangle(cx, cy - 16, cx + 9, cy + 12, cx, cy + 7, screenBackgroundColor());
  display.setTextDatum(MC_DATUM);
  display.setTextColor(mainTextColor());
  if (headingRemembered) {
    static const char* const labels[] = {"N", "E", "S", "W"};
    for (uint8_t i = 0; i < 4; ++i) {
      int16_t x, y;
      cardinalPosition(i, currentHeading, x, y);
      display.drawString(labels[i], x, y, 1);
    }
    char number[8];
    snprintf(number, sizeof(number), "%u", static_cast<unsigned int>(roundf(currentHeading)) % 360);
    display.drawString(String(number) + "\xC2\xB0 " + headingCardinal(currentHeading), cx, 112, 2);
    if (!headingAvailable) {
      display.setTextColor(COLOR_MUTED);
      display.drawString("ULTIMO RUMO", cx, 128, 1);
    }
  } else {
    // No direction is assumed before the first valid GPS course.
    display.setTextColor(COLOR_MUTED);
    display.drawString("--", cx, 112, 2);
  }
  display.setTextColor(COLOR_MUTED);
  display.drawString("DESTINO", cx, 145, 1);
  display.setTextColor(routeAvailable ? mainTextColor() : COLOR_MUTED);
  display.drawString(remainingDistanceText(), cx, 166, 2);
}

int8_t maneuverIconIndex(const String& maneuver) {
  if (maneuver == "straight") return 0;
  if (maneuver == "slight_left") return 1;
  if (maneuver == "left") return 2;
  if (maneuver == "slight_right") return 3;
  if (maneuver == "right") return 4;
  if (maneuver == "uturn") return 5;
  return -1;
}

void drawManeuverIcon(const String& maneuver) {
  const int8_t index = maneuverIconIndex(maneuver);
  if (index < 0) {
    // A glyph named nav_invalid happens to be a straight arrow in this font.
    // Never use that drawing to suggest a direction for an unknown command.
    display.setTextDatum(MC_DATUM);
    display.setTextColor(COLOR_MUTED);
    display.drawString("?", navigationWidth() / 2, Layout::ICON_CENTER_Y, 4);
    return;
  }
  // Read a monochrome mask directly from flash; zero bits are transparent.
  // No framebuffer, filesystem, PNG decoder or runtime TTF engine is required.
  display.drawBitmap(
      (navigationWidth() - NavigationFont::kWidth) / 2,
      Layout::ICON_CENTER_Y - NavigationFont::kHeight / 2,
      NavigationFont::kMasks[index], NavigationFont::kWidth, NavigationFont::kHeight,
      highlightColor());
}

void drawBackground() {
  display.fillScreen(screenBackgroundColor());
}

int8_t speedBand(float kmh) {
  if (kmh > Layout::YELLOW_LIMIT) return 2;
  if (kmh > Layout::GREEN_LIMIT) return 1;
  return 0;
}

int16_t speedScaleWidth() {
  return display.width() - Layout::SCALE_X - Layout::MARGIN -
         Layout::SCALE_BUTTON_CLEARANCE;
}

void drawSpeedPanel() {
  using namespace Layout;
  const int16_t width = display.width();
  const int16_t panelTop = PANEL_TOP;
  const int16_t scaleX = SCALE_X;
  const int16_t scaleWidth = speedScaleWidth();
  const int16_t barY = BAR_Y;
  const int16_t barHeight = BAR_HEIGHT;

  display.fillRect(0, panelTop, width, display.height() - panelTop, panelBackgroundColor());

  display.setTextDatum(TL_DATUM);
  display.setTextColor(speedAvailable ? mainTextColor() : COLOR_MUTED);
  display.drawString(speedAvailable ? String(static_cast<int>(roundf(currentSpeedKmh))) : "--",
                     SPEED_VALUE_X, SPEED_VALUE_Y, 4);
  display.setTextColor(COLOR_MUTED);
  display.drawString("km/h", SPEED_UNIT_X, SPEED_UNIT_Y, 1);

  // Leave a visible panel-color border around the colored speed bands.
  display.fillRect(scaleX - SCALE_BACKING_PADDING,
                   barY - SCALE_BACKING_PADDING,
                   scaleWidth + 2 * SCALE_BACKING_PADDING,
                   barHeight + 2 * SCALE_BACKING_PADDING,
                    panelBackgroundColor());
  const int16_t mark50 = scaleX + static_cast<int16_t>(scaleWidth * float(GREEN_LIMIT) / SCALE_MAX);
  const int16_t mark100 = scaleX + static_cast<int16_t>(scaleWidth * float(YELLOW_LIMIT) / SCALE_MAX);
  const int8_t activeBand = speedAvailable ? speedBand(currentSpeedKmh) : -1;
  const uint16_t idleBandColor = dayMode ? COLOR_DAY_SPEED_IDLE : COLOR_SPEED_HIGH;
  display.fillRect(scaleX, barY, mark50 - scaleX, barHeight,
                     activeBand == 0 ? COLOR_AMBER : idleBandColor);
  display.fillRect(mark50, barY, mark100 - mark50, barHeight,
                     activeBand == 1 ? COLOR_AMBER : idleBandColor);
  display.fillRect(mark100, barY, scaleX + scaleWidth - mark100, barHeight,
                     activeBand == 2 ? COLOR_AMBER : idleBandColor);

  for (const uint16_t value : GRADUATIONS) {
    const int16_t x = scaleX + static_cast<int16_t>(scaleWidth * float(value) / SCALE_MAX);
    const bool special = value == GREEN_LIMIT || value == YELLOW_LIMIT;
    display.drawFastVLine(x, barY - (special ? 7 : 4), barHeight + (special ? 14 : 8),
                          special ? highlightColor() : COLOR_MUTED);
    display.setTextDatum(TC_DATUM);
    display.setTextColor(special ? highlightColor() : COLOR_MUTED);
    display.drawString(String(value), x, SCALE_LABEL_Y, 1);
  }

  if (speedAvailable) {
    const float boundedSpeed = constrain(currentSpeedKmh, 0.0f, float(SCALE_MAX));
    const int16_t markerX = scaleX + static_cast<int16_t>(scaleWidth * boundedSpeed / SCALE_MAX);
    const int16_t markerTop = barY - MARKER_TOP_OFFSET;
    display.fillRect(markerX - MARKER_WIDTH / 2, markerTop, MARKER_WIDTH,
                     barHeight + MARKER_TOP_OFFSET + 4, mainTextColor());
    // Downward tip touches the top of the 2px marker, all within the black panel.
    display.fillTriangle(markerX - MARKER_TRIANGLE_HALF_WIDTH,
                         markerTop - MARKER_TRIANGLE_HEIGHT,
                         markerX + MARKER_TRIANGLE_HALF_WIDTH,
                         markerTop - MARKER_TRIANGLE_HEIGHT,
                          markerX, markerTop, mainTextColor());
  }
  drawThemeToggle();
}

void drawOrientationCheck(bool rotationTest) {
  const int16_t width = display.width();
  const int16_t height = display.height();
  const bool landscape = width == DISPLAY_LANDSCAPE_WIDTH &&
                         height == DISPLAY_LANDSCAPE_HEIGHT;

  if (!rotationTest) {
    display.fillScreen(COLOR_AMBER);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(COLOR_BLACK);
    display.drawString("PetitGPS", width / 2, height / 2 - 18, 4);
    display.drawString("by Charles Beck Varani", width / 2, height / 2 + 21, 2);
    return;
  }

  drawBackground();
  display.drawRect(0, 0, width, height, highlightColor());
  display.setTextDatum(TC_DATUM);
  display.setTextColor(highlightColor());
  display.drawCentreString("PETITGPS TFT", width / 2, height / 2 - 46, 2);
  String status;
  if (rotationTest) {
    status = "ROTATION r";
    status += String(activeDisplayRotation);
  } else {
    status = landscape ? "LANDSCAPE OK" : "CHECK TFT SETUP";
  }
  display.setTextColor(rotationTest || landscape ? mainTextColor() : COLOR_RED);
  display.drawCentreString(status, width / 2, height / 2 - 14, 2);
  display.setTextColor(COLOR_MUTED);
  display.drawCentreString(String(width) + " x " + String(height),
                           width / 2, height / 2 + 18, 2);
}

void drawIdleScreen() {
  drawBackground();
  display.setTextDatum(MC_DATUM);
  display.setTextColor(COLOR_MUTED);
  display.drawString("Aguardando instrucoes", navigationWidth() / 2, Layout::PANEL_TOP / 2 - 16, 2);
  display.setTextColor(mainTextColor());
  display.drawString("Conecte pelo app Android", navigationWidth() / 2, Layout::PANEL_TOP / 2 + 16,
                     navigationWidth() < 220 ? 1 : 2);
  drawSidePanel();
  drawSpeedPanel();
}

void drawTextGuidance(const String& text) {
  const int16_t maxWidth = navigationWidth() - 16;
  const uint16_t length = text.length();
  uint16_t offset = 0;
  display.setTextDatum(TC_DATUM);
  display.setTextColor(mainTextColor());
  for (uint8_t line = 0; line < 4 && offset < length; ++line) {
    uint16_t end = offset;
    uint16_t lastSpace = offset;
    while (end < length &&
           display.textWidth(text.substring(offset, end + 1), 2) <= maxWidth) {
      if (text.c_str()[end] == ' ') lastSpace = end;
      ++end;
    }
    if (end == offset) ++end; // Always advance, even on diagnostic portrait rotations.
    if (end < length && lastSpace > offset) end = lastSpace;
    String fragment = text.substring(offset, end);
    if (line == 3 && end < length) {
      while (fragment.length() > 0 &&
             display.textWidth(fragment + "...", 2) > maxWidth) {
        fragment = fragment.substring(0, fragment.length() - 1);
      }
      fragment += "...";
    }
    display.drawString(fragment, navigationWidth() / 2, 47 + line * 26, 2);
    offset = end;
    while (offset < length && text.c_str()[offset] == ' ') ++offset;
  }
  display.setTextColor(COLOR_MUTED);
    display.drawString("Sem manobra informada", navigationWidth() / 2,
                       Layout::TEXT_STATUS_Y, 2);
}

void drawMapsTextHeading() {
  const String heading = "Orientacao do GMaps";
  const int16_t centerX = navigationWidth() / 2;
  const int16_t y = Layout::DIRECTION_Y;
  display.setTextDatum(TC_DATUM);
  display.setTextColor(COLOR_MUTED);
  display.drawString(heading, centerX, y, 1);

  // Font 1 is ASCII-only; add the cedilla and tilde with small strokes.
  const int16_t left = centerX - display.textWidth(heading, 1) / 2;
  const int16_t cedillaX = left + 7 * Layout::FONT1_CHAR_WIDTH + 2;
  const int16_t tildeX = left + 8 * Layout::FONT1_CHAR_WIDTH + 1;
  display.drawLine(cedillaX, y + 8, cedillaX + 1, y + 9, COLOR_MUTED);
  display.drawLine(cedillaX + 1, y + 9, cedillaX, y + 10, COLOR_MUTED);
  display.drawLine(tildeX, y - 1, tildeX + 1, y - 2, COLOR_MUTED);
  display.drawLine(tildeX + 1, y - 2, tildeX + 2, y - 1, COLOR_MUTED);
  display.drawLine(tildeX + 2, y - 1, tildeX + 3, y - 2, COLOR_MUTED);
  display.drawLine(tildeX + 3, y - 2, tildeX + 4, y - 1, COLOR_MUTED);
}

void renderInstruction(const NavigationInstruction& instruction) {
  drawBackground();
  display.setTextDatum(TC_DATUM);
  display.setTextColor(COLOR_MUTED);
  if (instruction.maneuver == "text") {
    drawMapsTextHeading();
    drawTextGuidance(instruction.road);
    if (instruction.distanceAvailable) {
      display.setTextDatum(MC_DATUM);
      display.setTextColor(mainTextColor());
      display.drawString(String(instruction.distanceMeters) + " m",
                         navigationWidth() / 2, Layout::TEXT_DISTANCE_Y, 4);
    }
    drawSidePanel();
    drawSpeedPanel();
    return;
  }
  display.drawString("PROXIMA DIRECAO", navigationWidth() / 2, Layout::DIRECTION_Y, 2);
  drawManeuverIcon(instruction.maneuver);

  display.setTextDatum(MC_DATUM);
  display.setTextColor(mainTextColor());
  display.drawString(String(instruction.distanceMeters) + " m",
                     navigationWidth() / 2, Layout::DISTANCE_Y, 4);
  if (instruction.road.length() > 0) {
    display.setTextColor(COLOR_MUTED);
    String road = instruction.road;
    if (road.length() > Layout::ROAD_MAX_BYTES) {
      road = road.substring(0, Layout::ROAD_PREFIX_BYTES) + "...";
    }
    // Fit the label to the left region, including narrower diagnostic rotations.
    while (road.length() > 0 && display.textWidth(road, 2) > navigationWidth() - 12) {
      road = road.substring(0, road.length() - 1);
    }
    display.drawString(road, navigationWidth() / 2, Layout::ROAD_Y, 2);
  }
  drawSidePanel();
  drawSpeedPanel();
}

void drawCurrentScreen() {
  if (touchCalibrationActive) {
    drawTouchCalibrationScreen();
    return;
  }
  if (hasNavigationInstruction) {
    renderInstruction(currentInstruction);
  } else {
    drawIdleScreen();
  }
}

void applyDisplayRotation(uint8_t rotation, bool rotationTest) {
  activeDisplayRotation = rotation;
  display.setRotation(activeDisplayRotation);

  Serial.printf("[INFO] Rotacao aplicada: %u (Largura=%d Altura=%d)\n",
                static_cast<unsigned int>(activeDisplayRotation),
                display.width(), display.height());
  if (!rotationTest &&
      (display.width() != DISPLAY_LANDSCAPE_WIDTH ||
       display.height() != DISPLAY_LANDSCAPE_HEIGHT)) {
    Serial.println("[AVISO] TFT nao esta em 320x240; confira User_Setup_Select.h e os pinos");
  }

  drawOrientationCheck(rotationTest);
  orientationCheckActive = true;
  orientationCheckStartedMs = millis();
}

void handleSerialCommand() {
  if (serialCommandLength == 2 && serialCommand[0] == 'r' &&
      serialCommand[1] >= '0' && serialCommand[1] <= '3') {
    pendingRotation = static_cast<int8_t>(serialCommand[1] - '0');
    pendingRotationTest = true;
    return;
  }

  Serial.println("Comando serial invalido. Use r0, r1, r2 ou r3.");
}

void pollSerialCommands() {
  for (uint8_t count = 0; count < SERIAL_BYTES_PER_LOOP && Serial.available() > 0; ++count) {
    const char character = static_cast<char>(Serial.read());
    if (character == '\r' || character == '\n') {
      if (!serialCommandOverflow && serialCommandLength > 0) {
        serialCommand[serialCommandLength] = '\0';
        handleSerialCommand();
      }
      serialCommandLength = 0;
      serialCommandOverflow = false;
    } else if (serialCommandOverflow) {
      continue;  // Discard the entire overlong line, not just its prefix.
    } else if (serialCommandLength < sizeof(serialCommand) - 1) {
      serialCommand[serialCommandLength++] = character;
    } else {
      serialCommandOverflow = true;
      Serial.println("Comando longo demais. Use r0, r1, r2 ou r3.");
    }
  }
}

class CommandCallbacks final : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
    const uint32_t receivedAtMs = millis();
    const std::string raw = characteristic->getValue();
    if (raw.empty() || raw.size() > MAX_COMMAND_BYTES) return;

    JsonDocument document;
    CommandReader reader{raw};
    const DeserializationError error = deserializeJson(
        document, reader, DeserializationOption::NestingLimit(MAX_JSON_DEPTH));
    if (error || !reader.onlyWhitespaceRemains() || !document.is<JsonObject>() ||
        !document["type"].is<const char*>()) return;

    const char* type = document["type"].as<const char*>();
    // Reject embedded NULs in fields that will become C strings.
    if (document["type"].as<JsonString>().size() != strlen(type)) return;
    if (strcmp(type, "heading") == 0) {
      if (!document["valid"].is<bool>()) return;
      const bool valid = document["valid"].as<bool>();
      const JsonVariantConst value = document["degrees"];
      if ((!value.isNull() && !value.is<double>()) || (valid && !value.is<double>())) return;
      const float degrees = value.isNull() ? -1 : value.as<float>();
      const bool available = valid && isfinite(degrees) && degrees >= 0 && degrees <= 360;
      const PendingHeading update{available ? normalizedHeading(degrees) : 0, available, receivedAtMs};
      portENTER_CRITICAL(&pendingMux);
      pendingHeading = update;
      headingPending = true;
      portEXIT_CRITICAL(&pendingMux);
      return;
    }
    if (strcmp(type, "route") == 0) {
      if (!document["valid"].is<bool>()) return;
      const bool valid = document["valid"].as<bool>();
      const JsonVariantConst value = document["remainingMeters"];
      if ((!value.isNull() && !value.is<int64_t>()) || (valid && !value.is<int64_t>())) return;
      const JsonVariantConst ttl = document["ttlMs"];
      if (!ttl.isNull() && !ttl.is<uint32_t>()) return;
      if (valid && (ttl.isNull() || ttl.as<uint32_t>() == 0 || ttl.as<uint32_t>() > ROUTE_TIMEOUT_MS)) return;
      const int64_t meters = value.isNull() ? -1 : value.as<int64_t>();
      const bool available = valid && meters >= 0 && meters <= MAX_ROUTE_METERS;
      const PendingRoute update{available ? static_cast<uint32_t>(meters) : 0, available,
                                receivedAtMs, ttl.isNull() ? ROUTE_TIMEOUT_MS : ttl.as<uint32_t>()};
      portENTER_CRITICAL(&pendingMux);
      pendingRoute = update;
      routePending = true;
      portEXIT_CRITICAL(&pendingMux);
      return;
    }
    if (strcmp(type, "speed") == 0) {
      if (!document["valid"].is<bool>()) return;
      const bool valid = document["valid"].as<bool>();
      const JsonVariantConst value = document["speedKmh"];
      if (!value.isNull() && !value.is<double>()) return;
      if (valid && !value.is<double>()) return;
      const float kmh = value.isNull() ? -1.0f : value.as<float>();
      // A correctly typed but unusable reading explicitly invalidates speed.
      // Bound conversion to the integer used for the visible number as well.
      const bool available = valid && isfinite(kmh) && kmh >= 0.0f &&
                             static_cast<double>(kmh) < static_cast<double>(INT_MAX);
      const PendingSpeed update{available ? kmh : 0.0f, available, receivedAtMs};
      portENTER_CRITICAL(&pendingMux);
      pendingSpeed = update;
      speedPending = true;
      portEXIT_CRITICAL(&pendingMux);
      return;
    }

    if (strcmp(type, "navigation") != 0 ||
        !document["maneuver"].is<const char*>()) return;
    const char* maneuver = document["maneuver"].as<const char*>();
    if (strlen(maneuver) == 0 ||
        document["maneuver"].as<JsonString>().size() != strlen(maneuver)) return;
    const JsonObjectConst object = document.as<JsonObjectConst>();
    if ((!object["road"].isUnbound() && !object["road"].is<const char*>()) ||
        (!object["distanceMeters"].isUnbound() && !object["distanceMeters"].is<int64_t>())) return;
    const char* road = object["road"] | "";
    if (strcmp(maneuver, "text") == 0 && strlen(road) == 0) return;
    if (strlen(road) > MAX_COMMAND_BYTES || strlen(maneuver) > MAX_COMMAND_BYTES ||
        (!object["road"].isUnbound() && object["road"].as<JsonString>().size() != strlen(road))) return;
    const int64_t distance = object["distanceMeters"] | int64_t(0);
    PendingNavigation update = {};
    strcpy(update.maneuver, maneuver);
    strcpy(update.road, road);
    update.distanceMeters = static_cast<uint16_t>(distance < 0 ? 0 : distance > 65535 ? 65535 : distance);
    update.distanceAvailable = !object["distanceMeters"].isUnbound();
    portENTER_CRITICAL(&pendingMux);
    pendingNavigation = update;
    navigationPending = true;
    portEXIT_CRITICAL(&pendingMux);
  }
};

String visibleRoad(const String& road) {
  return road.length() > Layout::ROAD_MAX_BYTES
      ? road.substring(0, Layout::ROAD_PREFIX_BYTES) + "..." : road;
}

int16_t speedMarker(float kmh) {
  return static_cast<int16_t>(speedScaleWidth() *
      constrain(kmh, 0.0f, float(Layout::SCALE_MAX)) / Layout::SCALE_MAX);
}

void processPendingCommands() {
  PendingNavigation navigation = {};
  PendingSpeed speed = {};
  PendingHeading heading = {};
  PendingRoute route = {};
  bool takeNavigation, takeSpeed, takeHeading, takeRoute;
  portENTER_CRITICAL(&pendingMux);
  takeNavigation = navigationPending;
  takeSpeed = speedPending;
  takeHeading = headingPending;
  takeRoute = routePending;
  if (takeNavigation) navigation = pendingNavigation;
  if (takeSpeed) speed = pendingSpeed;
  if (takeHeading) heading = pendingHeading;
  if (takeRoute) route = pendingRoute;
  navigationPending = false;
  speedPending = false;
  headingPending = false;
  routePending = false;
  portEXIT_CRITICAL(&pendingMux);

  // At most one navigation and one speed per loop, irrespective of burst size.
  if (takeNavigation) {
    if (!hasNavigationInstruction ||
        currentInstruction.maneuver != String(navigation.maneuver) ||
        currentInstruction.distanceMeters != navigation.distanceMeters ||
        currentInstruction.distanceAvailable != navigation.distanceAvailable ||
        (String(navigation.maneuver) == "text"
          ? currentInstruction.road != String(navigation.road)
          : visibleRoad(currentInstruction.road) != visibleRoad(String(navigation.road)))) {
      screenDirty = true;
    }
    currentInstruction = {String(navigation.maneuver), String(navigation.road),
                          navigation.distanceMeters, navigation.distanceAvailable};
    hasNavigationInstruction = true;
    Serial.printf("TURN=%s | DISTANCE=%um | ROAD=%s\n",
                  navigation.maneuver, navigation.distanceMeters, navigation.road);
  }
  if (takeSpeed) {
    // Age is measured from reception, not from the eventual screen refresh.
    // Read after the snapshot: the BLE task can publish while loop() begins.
    const uint32_t now = millis();
    const bool available = speed.available &&
        static_cast<uint32_t>(now - speed.receivedAtMs) < SPEED_TIMEOUT_MS;
    if (speedAvailable != available ||
        (available && (roundf(currentSpeedKmh) != roundf(speed.kmh) ||
                       speedMarker(currentSpeedKmh) != speedMarker(speed.kmh) ||
                       speedBand(currentSpeedKmh) != speedBand(speed.kmh)))) {
      speedPanelDirty = true;
    }
    currentSpeedKmh = available ? speed.kmh : 0.0f;
    speedAvailable = available;
    lastSpeedUpdateMs = speed.receivedAtMs;
    if (available) Serial.printf("SPEED=%.1f km/h\n", currentSpeedKmh);
    else Serial.println("SPEED=unavailable");
  }
  if (takeHeading) {
    const bool available = heading.available &&
        static_cast<uint32_t>(millis() - heading.receivedAtMs) < HEADING_TIMEOUT_MS;
    if (headingAvailable != available || (available && currentHeading != heading.degrees)) sidePanelDirty = true;
    headingAvailable = available;
    if (available) {
      currentHeading = heading.degrees;
      headingRemembered = true;
    }
    lastHeadingUpdateMs = heading.receivedAtMs;
  }
  if (takeRoute) {
    const bool available = route.available &&
        static_cast<uint32_t>(millis() - route.receivedAtMs) < route.ttlMs;
    if (routeAvailable != available || (available && remainingMeters != route.meters)) sidePanelDirty = true;
    routeAvailable = available;
    remainingMeters = available ? route.meters : 0;
    lastRouteUpdateMs = route.receivedAtMs;
    routeValidityMs = route.ttlMs;
  }
}

class ServerCallbacks final : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override {
    Serial.println("Android connected");
  }

  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    portENTER_CRITICAL(&pendingMux);
    pendingHeading = {0, false, millis()};
    pendingRoute = {0, false, millis(), ROUTE_TIMEOUT_MS};
    headingPending = routePending = true;
    portEXIT_CRITICAL(&pendingMux);
    Serial.println("Android disconnected; advertising again");
    NimBLEDevice::startAdvertising();
  }
};

void setup() {
  Serial.begin(115200);
  pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(TFT_BACKLIGHT_PIN, HIGH);
  display.init();
  initializeTouchController();

  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::setMTU(247);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* service = server->createService(SERVICE_UUID);
  NimBLECharacteristic* command = service->createCharacteristic(
      COMMAND_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  command->setCallbacks(new CommandCallbacks());
  service->start();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->enableScanResponse(true);
  const bool nameAdded = advertising->setName(DEVICE_NAME);
  const bool serviceAdded = advertising->addServiceUUID(SERVICE_UUID);
  if (!nameAdded || !serviceAdded) {
    Serial.println("[BLE] Failed to configure advertising name or service UUID");
  } else if (!advertising->start()) {
    Serial.println("[BLE] Failed to start advertising");
  } else {
    Serial.printf("%s ready; advertising and waiting for Android BLE connection\n", DEVICE_NAME);
  }
  Serial.println("Teste de rotacao: envie r0, r1, r2 ou r3 com Nova linha.");
}

void loop() {
  pollSerialCommands();
  processPendingCommands();
  uint32_t now = millis();

  if (pendingRotation >= 0) {
    applyDisplayRotation(static_cast<uint8_t>(pendingRotation), pendingRotationTest);
    pendingRotation = -1;
    screenDirty = true;
    now = millis();  // Rotation drawing may advance the clock.
  }
  if (orientationCheckActive &&
      static_cast<uint32_t>(now - orientationCheckStartedMs) >= DISPLAY_ORIENTATION_CHECK_MS) {
    orientationCheckActive = false;
    screenDirty = true;
  }
  pollTouchInput();

  if (speedAvailable && static_cast<uint32_t>(now - lastSpeedUpdateMs) >= SPEED_TIMEOUT_MS) {
    speedAvailable = false;
    currentSpeedKmh = 0.0f;
    speedPanelDirty = true;
    Serial.println("SPEED=stale");
  }
  if (headingAvailable && static_cast<uint32_t>(now - lastHeadingUpdateMs) >= HEADING_TIMEOUT_MS) {
    headingAvailable = false;
    sidePanelDirty = true;
  }
  if (routeAvailable && static_cast<uint32_t>(now - lastRouteUpdateMs) >= routeValidityMs) {
    routeAvailable = false;
    remainingMeters = 0;
    sidePanelDirty = true;
  }
  if (!orientationCheckActive &&
      static_cast<uint32_t>(now - lastScreenUpdateMs) >= SCREEN_UPDATE_INTERVAL_MS &&
      (screenDirty || speedPanelDirty || sidePanelDirty)) {
    if (screenDirty) drawCurrentScreen();
    else {
      if (speedPanelDirty) drawSpeedPanel();
      if (sidePanelDirty) drawSidePanel();
    }
    screenDirty = false;
    speedPanelDirty = false;
    sidePanelDirty = false;
    lastScreenUpdateMs = now;
  }
  yield();
}
