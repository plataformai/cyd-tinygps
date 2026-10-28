/*
 * BeeLine ESP32 BLE receiver
 *
 * Arduino IDE libraries:
 *   - NimBLE-Arduino 2.x
 *   - ArduinoJson 7.x
 *
 * BLE GATT:
 *   Service:         7b1e0001-6d2a-4f7a-9c21-8e8a4b12a001
 *   Write command:   7b1e0002-6d2a-4f7a-9c21-8e8a4b12a001
 *
 * The display-specific implementation is intentionally isolated in
 * renderInstruction(); connect that function to the LED matrix/OLED used
 * by your particular BeeLine-style build.
 */

#include <ArduinoJson.h>
#include <NimBLEDevice.h>

static constexpr char DEVICE_NAME[] = "BeeLine-ESP32";
static constexpr char SERVICE_UUID[] = "7b1e0001-6d2a-4f7a-9c21-8e8a4b12a001";
static constexpr char COMMAND_UUID[] = "7b1e0002-6d2a-4f7a-9c21-8e8a4b12a001";

struct NavigationInstruction {
  String maneuver;
  String road;
  uint16_t distanceMeters;
};

void renderInstruction(const NavigationInstruction& instruction) {
  // Replace this Serial output with your display driver, e.g. OLED or LEDs.
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
