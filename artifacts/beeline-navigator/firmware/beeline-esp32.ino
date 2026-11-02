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
#include "beeline_background_renderer.h"

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
static constexpr uint32_t SCREEN_UPDATE_INTERVAL_MS = 100;
static constexpr uint8_t SERIAL_BYTES_PER_LOOP = 16;
namespace Layout {
constexpr int16_t PANEL_TOP = 193;
constexpr int16_t MARGIN = 12;
constexpr int16_t SCALE_X = 96;
constexpr int16_t BAR_Y = 207;
constexpr int16_t BAR_HEIGHT = 10;
constexpr int16_t SPEED_LABEL_X = 13, SPEED_LABEL_Y = 196;
constexpr int16_t SPEED_VALUE_Y = 204, SPEED_UNIT_X = 15, SPEED_UNIT_Y = 232;
constexpr int16_t SCALE_LABEL_Y = 222;
constexpr uint16_t SCALE_MAX = 140, GREEN_LIMIT = 50, YELLOW_LIMIT = 100;
constexpr uint16_t GRADUATIONS[] = {0, 20, 40, 50, 60, 80, 100, 120, 140};
constexpr float ARROW_CENTER_Y = 81, ARROW_LENGTH = 53, ARROW_NECK = 17;
constexpr float SHAFT_HALF_WIDTH = 7, HEAD_HALF_WIDTH = 18;
constexpr int16_t DIRECTION_Y = 5, DISTANCE_Y = 137, ROAD_Y = 171;
constexpr unsigned int ROAD_MAX_BYTES = 27, ROAD_PREFIX_BYTES = 24;
}
static constexpr uint16_t COLOR_BACKGROUND = 0x1124;
static constexpr uint16_t COLOR_SURFACE = 0x1A2C;
static constexpr uint16_t COLOR_AMBER = 0xF6A8;
static constexpr uint16_t COLOR_TEXT = 0xFFFF;
static constexpr uint16_t COLOR_MUTED = 0x9DB1;
static constexpr uint16_t COLOR_GREEN = 0x36E6;
static constexpr uint16_t COLOR_YELLOW = 0xF7E0;
static constexpr uint16_t COLOR_RED = 0xF986;

// The four-line transfer buffer is small and never allocates a screen sprite.
static BeeLineBackgroundRenderer backgroundRenderer;

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
uint32_t lastScreenUpdateMs = 0;
int8_t pendingRotation = DEFAULT_DISPLAY_ROTATION;
bool pendingRotationTest = false;

// Two independent, fixed-size latest-wins mailboxes. Speed bursts cannot evict
// navigation. Only POD copies happen under this cross-core critical section:
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
portMUX_TYPE pendingMux = portMUX_INITIALIZER_UNLOCKED;
PendingNavigation pendingNavigation = {};
PendingSpeed pendingSpeed = {};
bool navigationPending = false;
bool speedPending = false;

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
  const float centerX = display.width() * 0.5f;
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
  backgroundRenderer.draw(display, COLOR_BACKGROUND);
}

void drawSpeedPanel(bool restoreBackground = true) {
  using namespace Layout;
  const int16_t width = display.width();
  const int16_t panelTop = PANEL_TOP;
  const int16_t scaleX = SCALE_X;
  const int16_t scaleWidth = width - scaleX - MARGIN;
  const int16_t barY = BAR_Y;
  const int16_t barHeight = BAR_HEIGHT;

  if (restoreBackground) {
    backgroundRenderer.drawRegion(
        display, panelTop, display.height() - panelTop, COLOR_BACKGROUND);
  }
  display.drawFastHLine(MARGIN, panelTop, width - 2 * MARGIN, COLOR_SURFACE);

  display.setTextDatum(TL_DATUM);
  display.setTextColor(COLOR_MUTED);
  display.drawString("VELOCIDADE", SPEED_LABEL_X, SPEED_LABEL_Y, 1);
  display.setTextColor(speedAvailable ? COLOR_TEXT : COLOR_MUTED);
  display.drawString(speedAvailable ? String(static_cast<int>(roundf(currentSpeedKmh))) : "--",
                     SPEED_LABEL_X, SPEED_VALUE_Y, 4);
  display.setTextColor(COLOR_MUTED);
  display.drawString("km/h", SPEED_UNIT_X, SPEED_UNIT_Y, 1);

  const int16_t mark50 = scaleX + static_cast<int16_t>(scaleWidth * float(GREEN_LIMIT) / SCALE_MAX);
  const int16_t mark100 = scaleX + static_cast<int16_t>(scaleWidth * float(YELLOW_LIMIT) / SCALE_MAX);
  display.fillRect(scaleX, barY, mark50 - scaleX, barHeight, COLOR_GREEN);
  display.fillRect(mark50, barY, mark100 - mark50, barHeight, COLOR_YELLOW);
  display.fillRect(mark100, barY, scaleX + scaleWidth - mark100, barHeight, COLOR_RED);

  for (const uint16_t value : GRADUATIONS) {
    const int16_t x = scaleX + static_cast<int16_t>(scaleWidth * float(value) / SCALE_MAX);
    const bool special = value == GREEN_LIMIT || value == YELLOW_LIMIT;
    display.drawFastVLine(x, barY - (special ? 7 : 4), barHeight + (special ? 14 : 8),
                          special ? COLOR_TEXT : COLOR_MUTED);
    display.setTextDatum(TC_DATUM);
    display.setTextColor(special ? COLOR_TEXT : COLOR_MUTED);
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
  display.drawString("BEELINE", display.width() / 2, 25, 4);
  display.setTextColor(COLOR_MUTED);
  display.drawCentreString("Aguardando instrucoes", display.width() / 2, 81, 2);
  display.drawRoundRect(28, 139, display.width() - 56, 51, 12, COLOR_SURFACE);
  display.setTextColor(COLOR_TEXT);
  display.drawCentreString("Conecte pelo app Android", display.width() / 2, 157, 2);
  drawSpeedPanel(false);
}

void renderInstruction(const NavigationInstruction& instruction) {
  drawBackground();
  display.setTextDatum(TC_DATUM);
  display.setTextColor(COLOR_MUTED);
  display.drawString("PROXIMA DIRECAO", display.width() / 2, Layout::DIRECTION_Y, 2);
  drawArrow(instruction.maneuver);

  display.setTextDatum(MC_DATUM);
  display.setTextColor(COLOR_TEXT);
  display.drawString(String(instruction.distanceMeters) + " m",
                     display.width() / 2, Layout::DISTANCE_Y, 4);
  if (instruction.road.length() > 0) {
    display.setTextColor(COLOR_MUTED);
    String road = instruction.road;
    if (road.length() > Layout::ROAD_MAX_BYTES) {
      road = road.substring(0, Layout::ROAD_PREFIX_BYTES) + "...";
    }
    display.drawString(road, display.width() / 2, Layout::ROAD_Y, 2);
  }
  drawSpeedPanel(false);
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
  bool takeNavigation, takeSpeed;
  portENTER_CRITICAL(&pendingMux);
  takeNavigation = navigationPending;
  takeSpeed = speedPending;
  if (takeNavigation) navigation = pendingNavigation;
  if (takeSpeed) speed = pendingSpeed;
  navigationPending = false;
  speedPending = false;
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
}

class ServerCallbacks final : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override {
    Serial.println("Android connected");
  }

  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
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
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->start();
  Serial.printf("%s ready; waiting for Android BLE connection\n", DEVICE_NAME);
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
  if (!orientationCheckActive &&
      static_cast<uint32_t>(now - lastScreenUpdateMs) >= SCREEN_UPDATE_INTERVAL_MS &&
      (screenDirty || speedPanelDirty)) {
    if (screenDirty) drawCurrentScreen();
    else drawSpeedPanel();
    screenDirty = false;
    speedPanelDirty = false;
    lastScreenUpdateMs = now;
  }
  yield();
}
