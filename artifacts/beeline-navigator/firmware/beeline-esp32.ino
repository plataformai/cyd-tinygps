/*
 * BeeLine ESP32 BLE receiver
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
#include <TFT_eSPI.h>
#include <freertos/FreeRTOS.h>
#include <math.h>
#include <limits.h>

static constexpr char DEVICE_NAME[] = "BeeLine-ESP32";
static constexpr char SERVICE_UUID[] = "7b1e0001-6d2a-4f7a-9c21-8e8a4b12a001";
static constexpr char COMMAND_UUID[] = "7b1e0002-6d2a-4f7a-9c21-8e8a4b12a001";
static constexpr uint8_t TFT_BACKLIGHT_PIN = 21;
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
static constexpr uint8_t SERIAL_BYTES_PER_LOOP = 16;
namespace Layout {
constexpr int16_t PANEL_TOP = 193;
constexpr int16_t MARGIN = 12;
constexpr int16_t SCALE_X = 96;
constexpr int16_t BAR_Y = 207;
constexpr int16_t BAR_HEIGHT = 10;
constexpr int16_t SCALE_BACKING_PADDING = 4;
constexpr int16_t SPEED_LABEL_X = 13, SPEED_LABEL_Y = 196;
constexpr int16_t SPEED_VALUE_Y = 204, SPEED_UNIT_X = 15, SPEED_UNIT_Y = 232;
constexpr int16_t SCALE_LABEL_Y = 222;
constexpr uint16_t SCALE_MAX = 140, GREEN_LIMIT = 50, YELLOW_LIMIT = 100;
constexpr uint16_t GRADUATIONS[] = {0, 20, 40, 50, 60, 80, 100, 120, 140};
constexpr float ARROW_CENTER_Y = 81, ARROW_LENGTH = 53, ARROW_NECK = 17;
constexpr float SHAFT_HALF_WIDTH = 7, HEAD_HALF_WIDTH = 18;
constexpr int16_t DIRECTION_Y = 5, DISTANCE_Y = 137, ROAD_Y = 171;
constexpr unsigned int ROAD_MAX_BYTES = 23, ROAD_PREFIX_BYTES = 20;
}
static constexpr uint16_t COLOR_BACKGROUND = 0x2104;  // neutral dark gray
static constexpr uint16_t COLOR_SURFACE = 0x1A2C;
static constexpr uint16_t COLOR_BLACK = 0x0000;
static constexpr uint16_t COLOR_AMBER = 0xF6A8;
static constexpr uint16_t COLOR_SPEED_MIDDLE = 0xB4A4;
static constexpr uint16_t COLOR_SPEED_HIGH = 0x7B24;
static constexpr uint16_t COLOR_TEXT = 0xFFFF;
static constexpr uint16_t COLOR_MUTED = 0x9DB1;
static constexpr uint16_t COLOR_RED = 0xF986;

struct NavigationInstruction {
  String maneuver;
  String road;
  uint16_t distanceMeters;
};

TFT_eSPI display = TFT_eSPI();
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
float currentHeading = 0;
uint32_t remainingMeters = 0, lastHeadingUpdateMs = 0, lastRouteUpdateMs = 0;
uint32_t routeValidityMs = ROUTE_TIMEOUT_MS;
uint32_t lastScreenUpdateMs = 0;
int8_t pendingRotation = DEFAULT_DISPLAY_ROTATION;
bool pendingRotationTest = false;

// Independent, fixed-size latest-wins mailboxes. Speed bursts cannot evict
// navigation, heading or route. Only POD copies happen under this critical section:
// never JSON parsing, allocation, logging or TFT/serial IO.
struct PendingNavigation {
  char maneuver[MAX_COMMAND_BYTES + 1];
  char road[MAX_COMMAND_BYTES + 1];
  uint16_t distanceMeters;
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
void applyDisplayRotation(uint8_t rotation, bool rotationTest);

int16_t navigationWidth() { return display.width() * 3 / 4; }

float normalizedHeading(float degrees) {
  const float angle = fmodf(degrees, 360.0f);
  return angle < 0 ? angle + 360.0f : angle;
}

const char* headingCardinal(float degrees) {
  static const char* const names[] = {"N", "NE", "L", "SE", "S", "SO", "O", "NO"};
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

void drawSidePanel() {
  const int16_t left = navigationWidth();
  const int16_t width = display.width() - left;
  const int16_t cx = left + width / 2, cy = 60;
  display.fillRect(left, 0, width, Layout::PANEL_TOP, COLOR_BACKGROUND);
  display.fillCircle(cx, cy, width / 2 - 18, COLOR_SPEED_HIGH);
  // Fixed north-up pointer shape; only cardinal positions orbit with the course.
  display.fillTriangle(cx, cy - 16, cx - 9, cy + 12, cx, cy + 7, COLOR_BACKGROUND);
  display.fillTriangle(cx, cy - 16, cx + 9, cy + 12, cx, cy + 7, COLOR_BACKGROUND);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(COLOR_TEXT);
  if (headingAvailable) {
    static const char* const labels[] = {"N", "L", "S", "O"};
    for (uint8_t i = 0; i < 4; ++i) {
      int16_t x, y;
      cardinalPosition(i, currentHeading, x, y);
      display.drawString(labels[i], x, y, 1);
    }
    char number[8];
    snprintf(number, sizeof(number), "%u", static_cast<unsigned int>(roundf(currentHeading)) % 360);
    display.drawString(String(number) + "\xC2\xB0 " + headingCardinal(currentHeading), cx, 112, 2);
  } else {
    // Do not leave old cardinal letters suggesting a currently valid course.
    display.setTextColor(COLOR_MUTED);
    display.drawString("--", cx, 112, 2);
  }
  display.setTextColor(COLOR_MUTED);
  display.drawString("DESTINO", cx, 145, 1);
  display.setTextColor(routeAvailable ? COLOR_TEXT : COLOR_MUTED);
  display.drawString(remainingDistanceText(), cx, 166, 2);
}

float maneuverAngle(const String& maneuver) {
  if (maneuver == "slight_left") return -35.0f;
  if (maneuver == "left") return -90.0f;
  if (maneuver == "slight_right") return 35.0f;
  if (maneuver == "right") return 90.0f;
  if (maneuver == "uturn") return 180.0f;
  return 0.0f;
}

void drawArrow(const String& maneuver) {
  const float angle = maneuverAngle(maneuver) * PI / 180.0f;
  const float dirX = sinf(angle);
  const float dirY = -cosf(angle);
  const float perpX = -dirY;
  const float perpY = dirX;
  const float centerX = navigationWidth() * 0.5f;
  const float centerY = Layout::ARROW_CENTER_Y;
  const float tipX = centerX + dirX * Layout::ARROW_LENGTH;
  const float tipY = centerY + dirY * Layout::ARROW_LENGTH;
  const float neckX = tipX - dirX * Layout::ARROW_NECK;
  const float neckY = tipY - dirY * Layout::ARROW_NECK;
  const float baseX = centerX - dirX * Layout::ARROW_NECK;
  const float baseY = centerY - dirY * Layout::ARROW_NECK;
  const float shaftHalfWidth = Layout::SHAFT_HALF_WIDTH;
  const float headHalfWidth = Layout::HEAD_HALF_WIDTH;

  display.fillTriangle(
      baseX + perpX * shaftHalfWidth, baseY + perpY * shaftHalfWidth,
      baseX - perpX * shaftHalfWidth, baseY - perpY * shaftHalfWidth,
      neckX + perpX * shaftHalfWidth, neckY + perpY * shaftHalfWidth,
      COLOR_AMBER);
  display.fillTriangle(
      baseX - perpX * shaftHalfWidth, baseY - perpY * shaftHalfWidth,
      neckX + perpX * shaftHalfWidth, neckY + perpY * shaftHalfWidth,
      neckX - perpX * shaftHalfWidth, neckY - perpY * shaftHalfWidth,
      COLOR_AMBER);
  display.fillTriangle(
      tipX, tipY,
      neckX + perpX * headHalfWidth, neckY + perpY * headHalfWidth,
      neckX - perpX * headHalfWidth, neckY - perpY * headHalfWidth,
      COLOR_AMBER);
}

void drawBackground() {
  display.fillScreen(COLOR_BACKGROUND);
}

void drawSpeedPanel() {
  using namespace Layout;
  const int16_t width = display.width();
  const int16_t panelTop = PANEL_TOP;
  const int16_t scaleX = SCALE_X;
  const int16_t scaleWidth = width - scaleX - MARGIN;
  const int16_t barY = BAR_Y;
  const int16_t barHeight = BAR_HEIGHT;

  display.fillRect(0, panelTop, width, display.height() - panelTop, COLOR_BLACK);

  display.setTextDatum(TL_DATUM);
  display.setTextColor(COLOR_MUTED);
  display.drawString("VELOCIDADE", SPEED_LABEL_X, SPEED_LABEL_Y, 1);
  display.setTextColor(speedAvailable ? COLOR_TEXT : COLOR_MUTED);
  display.drawString(speedAvailable ? String(static_cast<int>(roundf(currentSpeedKmh))) : "--",
                     SPEED_LABEL_X, SPEED_VALUE_Y, 4);
  display.setTextColor(COLOR_MUTED);
  display.drawString("km/h", SPEED_UNIT_X, SPEED_UNIT_Y, 1);

  // Leave a visible black border around the colored speed bands.
  display.fillRect(scaleX - SCALE_BACKING_PADDING,
                   barY - SCALE_BACKING_PADDING,
                   scaleWidth + 2 * SCALE_BACKING_PADDING,
                   barHeight + 2 * SCALE_BACKING_PADDING,
                   COLOR_BLACK);
  const int16_t mark50 = scaleX + static_cast<int16_t>(scaleWidth * float(GREEN_LIMIT) / SCALE_MAX);
  const int16_t mark100 = scaleX + static_cast<int16_t>(scaleWidth * float(YELLOW_LIMIT) / SCALE_MAX);
  display.fillRect(scaleX, barY, mark50 - scaleX, barHeight, COLOR_AMBER);
  display.fillRect(mark50, barY, mark100 - mark50, barHeight, COLOR_SPEED_MIDDLE);
  display.fillRect(mark100, barY, scaleX + scaleWidth - mark100, barHeight, COLOR_SPEED_HIGH);

  for (const uint16_t value : GRADUATIONS) {
    const int16_t x = scaleX + static_cast<int16_t>(scaleWidth * float(value) / SCALE_MAX);
    const bool special = value == GREEN_LIMIT || value == YELLOW_LIMIT;
    display.drawFastVLine(x, barY - (special ? 7 : 4), barHeight + (special ? 14 : 8),
                          special ? COLOR_AMBER : COLOR_MUTED);
    display.setTextDatum(TC_DATUM);
    display.setTextColor(special ? COLOR_AMBER : COLOR_MUTED);
    display.drawString(String(value), x, SCALE_LABEL_Y, 1);
  }

  if (speedAvailable) {
    const float boundedSpeed = constrain(currentSpeedKmh, 0.0f, float(SCALE_MAX));
    const int16_t markerX = scaleX + static_cast<int16_t>(scaleWidth * boundedSpeed / SCALE_MAX);
    display.drawFastVLine(markerX, barY - 8, barHeight + 16, COLOR_TEXT);
  }
}

void drawOrientationCheck(bool rotationTest) {
  const int16_t width = display.width();
  const int16_t height = display.height();
  const bool landscape = width == DISPLAY_LANDSCAPE_WIDTH &&
                         height == DISPLAY_LANDSCAPE_HEIGHT;

  drawBackground();
  display.drawRect(0, 0, width, height, COLOR_AMBER);
  display.setTextDatum(TC_DATUM);
  display.setTextColor(COLOR_AMBER);
  display.drawCentreString("BEELINE TFT", width / 2, height / 2 - 46, 2);
  String status;
  if (rotationTest) {
    status = "ROTATION r";
    status += String(activeDisplayRotation);
  } else {
    status = landscape ? "LANDSCAPE OK" : "CHECK TFT SETUP";
  }
  display.setTextColor(rotationTest || landscape ? COLOR_TEXT : COLOR_RED);
  display.drawCentreString(status, width / 2, height / 2 - 14, 2);
  display.setTextColor(COLOR_MUTED);
  display.drawCentreString(String(width) + " x " + String(height),
                           width / 2, height / 2 + 18, 2);
}

void drawIdleScreen() {
  drawBackground();
  display.setTextDatum(TC_DATUM);
  display.setTextColor(COLOR_AMBER);
  display.drawString("BEELINE", navigationWidth() / 2, 25, 4);
  display.setTextColor(COLOR_MUTED);
  display.drawCentreString("Aguardando instrucoes", navigationWidth() / 2, 81, 2);
  display.setTextColor(COLOR_TEXT);
  display.drawCentreString("Conecte pelo app Android", navigationWidth() / 2, 157,
                           navigationWidth() < 220 ? 1 : 2);
  drawSidePanel();
  drawSpeedPanel();
}

void renderInstruction(const NavigationInstruction& instruction) {
  drawBackground();
  display.setTextDatum(TC_DATUM);
  display.setTextColor(COLOR_MUTED);
  display.drawString("PROXIMA DIRECAO", navigationWidth() / 2, Layout::DIRECTION_Y, 2);
  drawArrow(instruction.maneuver);

  display.setTextDatum(MC_DATUM);
  display.setTextColor(COLOR_TEXT);
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
    if (strlen(road) > MAX_COMMAND_BYTES || strlen(maneuver) > MAX_COMMAND_BYTES ||
        (!object["road"].isUnbound() && object["road"].as<JsonString>().size() != strlen(road))) return;
    const int64_t distance = object["distanceMeters"] | int64_t(0);
    PendingNavigation update = {};
    strcpy(update.maneuver, maneuver);
    strcpy(update.road, road);
    update.distanceMeters = static_cast<uint16_t>(distance < 0 ? 0 : distance > 65535 ? 65535 : distance);
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
  return static_cast<int16_t>((display.width() - Layout::SCALE_X - Layout::MARGIN) *
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
        maneuverAngle(currentInstruction.maneuver) != maneuverAngle(navigation.maneuver) ||
        currentInstruction.distanceMeters != navigation.distanceMeters ||
        visibleRoad(currentInstruction.road) != visibleRoad(String(navigation.road))) {
      screenDirty = true;
    }
    currentInstruction = {String(navigation.maneuver), String(navigation.road), navigation.distanceMeters};
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
                       speedMarker(currentSpeedKmh) != speedMarker(speed.kmh)))) {
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
    currentHeading = available ? heading.degrees : 0;
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

  if (speedAvailable && static_cast<uint32_t>(now - lastSpeedUpdateMs) >= SPEED_TIMEOUT_MS) {
    speedAvailable = false;
    currentSpeedKmh = 0.0f;
    speedPanelDirty = true;
    Serial.println("SPEED=stale");
  }
  if (headingAvailable && static_cast<uint32_t>(now - lastHeadingUpdateMs) >= HEADING_TIMEOUT_MS) {
    headingAvailable = false;
    currentHeading = 0;
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
