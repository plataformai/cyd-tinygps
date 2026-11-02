#pragma once
#include <Arduino.h>
struct NimBLEConnInfo {};
struct NimBLECharacteristic;
struct NimBLEServer;
struct NimBLECharacteristicCallbacks {
  virtual ~NimBLECharacteristicCallbacks() = default;
  virtual void onWrite(NimBLECharacteristic*, NimBLEConnInfo&) {}
};
struct NimBLEServerCallbacks {
  virtual ~NimBLEServerCallbacks() = default;
  virtual void onConnect(NimBLEServer*, NimBLEConnInfo&) {}
  virtual void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) {}
};
struct NimBLECharacteristic {
  std::string value;
  NimBLECharacteristicCallbacks* callbacks = nullptr;
  std::string getValue() const { return value; }
  void setCallbacks(NimBLECharacteristicCallbacks* v) { callbacks = v; }
};
inline NimBLECharacteristic fakeCharacteristic;
struct NimBLEService {
  NimBLECharacteristic* createCharacteristic(const char*, int) { return &fakeCharacteristic; }
  void start() {}
};
struct NimBLEServer {
  NimBLEServerCallbacks* callbacks = nullptr;
  void setCallbacks(NimBLEServerCallbacks* v) { callbacks = v; }
  NimBLEService* createService(const char*) { static NimBLEService service; return &service; }
};
struct NimBLEAdvertising {
  int starts = 0;
  bool scanResponseEnabled = false;
  std::string nameInAdvertisement;
  std::string nameInScanResponse;
  std::string serviceUUID;
  void enableScanResponse(bool enabled) { scanResponseEnabled = enabled; }
  bool setName(const char* value) {
    if (scanResponseEnabled) nameInScanResponse = value;
    else nameInAdvertisement = value;
    return true;
  }
  bool addServiceUUID(const char* value) { serviceUUID = value; return true; }
  bool start() { ++starts; return true; }
};
namespace NIMBLE_PROPERTY { constexpr int WRITE = 1, WRITE_NR = 2; }
struct NimBLEDevice {
  static void init(const char*) {}
  static void setMTU(int) {}
  static NimBLEServer* createServer() { static NimBLEServer server; return &server; }
  static NimBLEAdvertising* getAdvertising() { static NimBLEAdvertising advertising; return &advertising; }
  static void startAdvertising() { getAdvertising()->start(); }
};
