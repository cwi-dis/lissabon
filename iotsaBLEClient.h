#ifndef _IOTSABLECLIENT_H_
#define _IOTSABLECLIENT_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include <BLEDevice.h>
#include "iotsaBLEClientConnection.h"

typedef std::function<void(BLEAdvertisedDevice&)> BleDeviceFoundCallback;
typedef const char *UUIDString;

class IotsaBLEClientMod : public IotsaMod, public BLEAdvertisedDeviceCallbacks {
public:
  IotsaBLEClientMod(IotsaApplication& app) : IotsaMod(app) {}
  using IotsaMod::IotsaMod;
  void setup();
  void serverSetup();
  void loop();
  String info() { return ""; }
  void setDeviceFoundCallback(BleDeviceFoundCallback _callback);
  void setServiceFilter(const BLEUUID& serviceUUID);
  void setManufacturerFilter(uint16_t manufacturerID);

  // These are all the known devices. They are saved persistently.
  std::map<std::string, IotsaBLEClientConnection*> devices;
  IotsaBLEClientConnection* addDevice(std::string id);
  IotsaBLEClientConnection* addDevice(String id);
  IotsaBLEClientConnection* getDevice(std::string id);
  IotsaBLEClientConnection* getDevice(String id);
  void delDevice(std::string id);
  void delDevice(String id);
  bool deviceSeen(std::string id, BLEAdvertisedDevice& device, bool add=false);
protected:
  void configLoad();
  void configSave();
  void onResult(BLEAdvertisedDevice advertisedDevice);
  void startScanning();
  void stopScanning();
  static void scanComplete(BLEScanResults results);
  BLEScan *scanner = NULL;
  BleDeviceFoundCallback callback = NULL;
  BLEUUID* serviceFilter = NULL;
  uint16_t manufacturerFilter;
  bool hasManufacturerFilter = false;
};

#endif
