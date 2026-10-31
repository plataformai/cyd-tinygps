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
 * TFT wiring targets the common ESP32-2432S028R/CYD revision. Confirm your
 * board revision before powering the backlight; some revisions differ.
 */

#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <TFT_eSPI.h>
#include <math.h>

static constexpr char DEVICE_NAME[] = "BeeLine-ESP32";
static constexpr char SERVICE_UUID[] = "7b1e0001-6d2a-4f7a-9c21-8e8a4b12a001";
static constexpr char COMMAND_UUID[] = "7b1e0002-6d2a-4f7a-9c21-8e8a4b12a001";
static constexpr uint8_t TFT_BACKLIGHT_PIN = 21;
static constexpr uint8_t DISPLAY_ROTATION_LANDSCAPE = 1;
static constexpr uint8_t DISPLAY_ROTATION_LANDSCAPE_INVERTED = 3;
static constexpr int16_t DISPLAY_LANDSCAPE_WIDTH = 320;
static constexpr int16_t DISPLAY_LANDSCAPE_HEIGHT = 240;
static constexpr uint16_t DISPLAY_ORIENTATION_CHECK_MS = 1200;
static constexpr uint16_t COLOR_BACKGROUND = 0x1124;
static constexpr uint16_t COLOR_SURFACE = 0x1A2C;
static constexpr uint16_t COLOR_AMBER = 0xF6A8;
static constexpr uint16_t COLOR_TEXT = 0xFFFF;
static constexpr uint16_t COLOR_MUTED = 0x9DB1;
static constexpr uint16_t COLOR_GREEN = 0x36E6;
static constexpr uint16_t COLOR_YELLOW = 0xF7E0;
static constexpr uint16_t COLOR_RED = 0xF986;

TFT_eSPI display = TFT_eSPI();
float currentSpeedKmh = 0.0f;
bool speedAvailable = false;
unsigned long lastSpeedUpdateMs = 0;

struct NavigationInstruction {
  String maneuver;
  String road;
  uint16_t distanceMeters;
};

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
  const float centerY = 81.0f;
  const float tipX = centerX + dirX * 53.0f;
  const float tipY = centerY + dirY * 53.0f;
  const float neckX = tipX - dirX * 17.0f;
  const float neckY = tipY - dirY * 17.0f;
  const float baseX = centerX - dirX * 17.0f;
  const float baseY = centerY - dirY * 17.0f;
  const float shaftHalfWidth = 7.0f;
  const float headHalfWidth = 18.0f;

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

void drawSpeedPanel() {
  const int16_t width = display.width();
  const int16_t panelTop = 193;
  const int16_t scaleX = 96;
  const int16_t scaleWidth = width - scaleX - 12;
  const int16_t barY = 207;
  const int16_t barHeight = 10;

  display.fillRect(0, panelTop, width, display.height() - panelTop, COLOR_BACKGROUND);
  display.drawFastHLine(12, panelTop, width - 24, COLOR_SURFACE);

  display.setTextDatum(TL_DATUM);
  display.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
  display.drawString("VELOCIDADE", 13, 196, 1);
  display.setTextColor(speedAvailable ? COLOR_TEXT : COLOR_MUTED, COLOR_BACKGROUND);
  display.drawString(speedAvailable ? String(static_cast<int>(roundf(currentSpeedKmh))) : "--",
                     13, 204, 4);
  display.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
  display.drawString("km/h", 15, 232, 1);

  const int16_t mark50 = scaleX + static_cast<int16_t>(scaleWidth * 50.0f / 140.0f);
  const int16_t mark100 = scaleX + static_cast<int16_t>(scaleWidth * 100.0f / 140.0f);
  display.fillRect(scaleX, barY, mark50 - scaleX, barHeight, COLOR_GREEN);
  display.fillRect(mark50, barY, mark100 - mark50, barHeight, COLOR_YELLOW);
  display.fillRect(mark100, barY, scaleX + scaleWidth - mark100, barHeight, COLOR_RED);

  const uint16_t graduations[] = {0, 20, 40, 50, 60, 80, 100, 120, 140};
  for (const uint16_t value : graduations) {
    const int16_t x = scaleX + static_cast<int16_t>(scaleWidth * value / 140.0f);
    const bool special = value == 50 || value == 100;
    display.drawFastVLine(x, barY - (special ? 7 : 4), barHeight + (special ? 14 : 8),
                          special ? COLOR_TEXT : COLOR_MUTED);
    display.setTextDatum(TC_DATUM);
    display.setTextColor(special ? COLOR_TEXT : COLOR_MUTED, COLOR_BACKGROUND);
    display.drawString(String(value), x, 222, 1);
  }

  if (speedAvailable) {
    const float boundedSpeed = constrain(currentSpeedKmh, 0.0f, 140.0f);
    const int16_t markerX = scaleX + static_cast<int16_t>(scaleWidth * boundedSpeed / 140.0f);
    display.drawFastVLine(markerX, barY - 8, barHeight + 16, COLOR_TEXT);
  }
}

void drawOrientationCheck() {
  const int16_t width = display.width();
  const int16_t height = display.height();
  const bool landscape = width == DISPLAY_LANDSCAPE_WIDTH &&
                         height == DISPLAY_LANDSCAPE_HEIGHT;

  display.fillScreen(COLOR_BACKGROUND);
  display.drawRect(0, 0, width, height, COLOR_AMBER);
  display.setTextDatum(TC_DATUM);
  display.setTextColor(COLOR_AMBER, COLOR_BACKGROUND);
  display.drawCentreString("BEELINE TFT", width / 2, height / 2 - 46, 2);
  display.setTextColor(landscape ? COLOR_TEXT : COLOR_RED, COLOR_BACKGROUND);
  display.drawCentreString(landscape ? "LANDSCAPE OK" : "CHECK TFT SETUP",
                           width / 2, height / 2 - 14, 2);
  display.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
  display.drawCentreString(String(width) + " x " + String(height),
                           width / 2, height / 2 + 18, 2);
}

void drawIdleScreen() {
  display.fillScreen(COLOR_BACKGROUND);
  display.setTextDatum(TC_DATUM);
  display.setTextColor(COLOR_AMBER, COLOR_BACKGROUND);
  display.drawString("BEELINE", display.width() / 2, 25, 4);
  display.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
  display.drawCentreString("Aguardando instrucoes", display.width() / 2, 81, 2);
  display.drawRoundRect(28, 139, display.width() - 56, 51, 12, COLOR_SURFACE);
  display.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  display.drawCentreString("Conecte pelo app Android", display.width() / 2, 157, 2);
  drawSpeedPanel();
}

void renderInstruction(const NavigationInstruction& instruction) {
  display.fillScreen(COLOR_BACKGROUND);
  display.setTextDatum(TC_DATUM);
  display.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
  display.drawString("PROXIMA DIRECAO", display.width() / 2, 5, 2);
  drawArrow(instruction.maneuver);

  display.setTextDatum(MC_DATUM);
  display.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  display.drawString(String(instruction.distanceMeters) + " m",
                     display.width() / 2, 137, 4);
  if (instruction.road.length() > 0) {
    display.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
    String road = instruction.road;
    if (road.length() > 27) road = road.substring(0, 24) + "...";
    display.drawString(road, display.width() / 2, 171, 2);
  }
  drawSpeedPanel();

  Serial.printf("TURN=%s | DISTANCE=%um | ROAD=%s\n",
                instruction.maneuver.c_str(),
                instruction.distanceMeters,
                instruction.road.c_str());
}

class CommandCallbacks final : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
    const std::string raw = characteristic->getValue();
    if (raw.empty() || raw.size() > 180) {
      Serial.println("Ignored empty or oversized BLE command");
      return;
    }

    JsonDocument document;
    const DeserializationError error = deserializeJson(document, raw);
    if (error) {
      Serial.printf("Invalid JSON: %s\n", error.c_str());
      return;
    }

    const char* type = document["type"] | "";
    if (strcmp(type, "speed") == 0) {
      const bool valid = document["valid"] | false;
      const float speedKmh = document["speedKmh"] | -1.0f;
      if (valid && isfinite(speedKmh) && speedKmh >= 0.0f) {
        currentSpeedKmh = speedKmh;
        speedAvailable = true;
        lastSpeedUpdateMs = millis();
        Serial.printf("SPEED=%.1f km/h\n", currentSpeedKmh);
      } else {
        speedAvailable = false;
        Serial.println("SPEED=unavailable");
      }
      drawSpeedPanel();
      return;
    }

    const char* maneuver = document["maneuver"] | "";
    if (strcmp(type, "navigation") != 0 || strlen(maneuver) == 0) {
      Serial.println("Ignored command with missing type or maneuver");
      return;
    }

    NavigationInstruction instruction;
    instruction.maneuver = maneuver;
    instruction.road = document["road"] | "";
    const int distance = document["distanceMeters"] | 0;
    instruction.distanceMeters = static_cast<uint16_t>(constrain(distance, 0, 65535));
    renderInstruction(instruction);
  }
};

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
  display.setRotation(DISPLAY_ROTATION_LANDSCAPE);
  if (display.width() != DISPLAY_LANDSCAPE_WIDTH ||
      display.height() != DISPLAY_LANDSCAPE_HEIGHT) {
    display.setRotation(DISPLAY_ROTATION_LANDSCAPE_INVERTED);
  }
  const bool landscape = display.width() == DISPLAY_LANDSCAPE_WIDTH &&
                         display.height() == DISPLAY_LANDSCAPE_HEIGHT;
  Serial.printf("TFT orientation: %d x %d (expected 320 x 240)\n",
                display.width(), display.height());
  if (!landscape) {
    Serial.println("TFT is not 320x240; check User_Setup_Select.h and the CYD pin setup");
  }
  drawOrientationCheck();
  delay(DISPLAY_ORIENTATION_CHECK_MS);
  drawIdleScreen();

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
}

void loop() {
  if (speedAvailable && millis() - lastSpeedUpdateMs > 8000) {
    speedAvailable = false;
    drawSpeedPanel();
    Serial.println("SPEED=stale");
  }
  delay(100);
}
