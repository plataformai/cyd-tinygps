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
static constexpr uint16_t COLOR_BACKGROUND = 0x1124;
static constexpr uint16_t COLOR_SURFACE = 0x1A2C;
static constexpr uint16_t COLOR_AMBER = 0xF6A8;
static constexpr uint16_t COLOR_TEXT = 0xFFFF;
static constexpr uint16_t COLOR_MUTED = 0x9DB1;

TFT_eSPI display = TFT_eSPI();

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
  const float centerY = 111.0f;
  const float tipX = centerX + dirX * 75.0f;
  const float tipY = centerY + dirY * 75.0f;
  const float neckX = tipX - dirX * 24.0f;
  const float neckY = tipY - dirY * 24.0f;
  const float baseX = centerX - dirX * 24.0f;
  const float baseY = centerY - dirY * 24.0f;
  const float shaftHalfWidth = 7.0f;
  const float headHalfWidth = 24.0f;

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
}

void renderInstruction(const NavigationInstruction& instruction) {
  display.fillScreen(COLOR_BACKGROUND);
  display.setTextDatum(TC_DATUM);
  display.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
  display.drawString("PROXIMA DIRECAO", display.width() / 2, 12, 2);
  drawArrow(instruction.maneuver);

  display.setTextDatum(MC_DATUM);
  display.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  display.drawString(String(instruction.distanceMeters) + " m",
                     display.width() / 2, 195, 4);
  if (instruction.road.length() > 0) {
    display.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
    String road = instruction.road;
    if (road.length() > 27) road = road.substring(0, 24) + "...";
    display.drawString(road, display.width() / 2, 226, 2);
  }

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
  display.setRotation(1);
  drawIdleScreen();

  NimBLEDevice::init(DEVICE_NAME);
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
  delay(100);
}
