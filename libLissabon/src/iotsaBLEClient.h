#ifndef _IOTSABLECLIENT_H_
#define _IOTSABLECLIENT_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include <BLEDevice.h>
#include "iotsaBLEClientConnection.h"

typedef std::function<void(BLEAdvertisedDevice&)> BleDeviceFoundCallback;
typedef const char *UUIDString;

class IotsaBLEClientMod : public IotsaApiMod, public BLEAdvertisedDeviceCallbacks {
public:
  //xxxjack IotsaBLEClientMod(IotsaApplication& app) : IotsaApiMod(app) {}
  using IotsaApiMod::IotsaApiMod;
  virtual ~IotsaBLEClientMod() {}
  virtual bool getHandler(const char *path, JsonObject& reply) override;
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#if 0
  bool formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) override;
  void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) override;
#endif
  virtual void setup() override;
  virtual void serverSetup() override;
  virtual void loop() override;
  virtual String info() override { return ""; }
  void findUnknownDevices(bool on);
  void setUnknownDeviceFoundCallback(BleDeviceFoundCallback _callback);
  void setServiceFilter(const BLEUUID& serviceUUID);
  void setManufacturerFilter(uint16_t manufacturerID);
protected:
  // These are all the known devices (known by the application, not by this module)
  std::map<std::string, IotsaBLEClientConnection*> devices;
  // These are all known devices by address
  std::map<std::string, IotsaBLEClientConnection *>devicesByAddress;
public:
  IotsaBLEClientConnection* addDevice(std::string id);
  IotsaBLEClientConnection* addDevice(String id);
  IotsaBLEClientConnection* getDevice(std::string id);
  IotsaBLEClientConnection* getDevice(String id);
  void delDevice(std::string id);
  void delDevice(String id);
  void deviceNotSeen(std::string id);
  void deviceNotSeen(String id);
  bool canConnect();
protected:
  void configLoad();
  void configSave();
  void onResult(BLEAdvertisedDevice advertisedDevice);
  void updateScanning();
  void startScanning();
  void stopScanning();
  static void scanComplete(BLEScanResults results);
  static IotsaBLEClientMod *scanningMod;
  bool scanForUnknownClients = false;
  bool shouldUpdateScan = false;
  BLEScan *scanner = NULL;
  BleDeviceFoundCallback callback = NULL;
  BLEUUID* serviceFilter = NULL;
  uint16_t manufacturerFilter;
  bool hasManufacturerFilter = false;
};

#endif
