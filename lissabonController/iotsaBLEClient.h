#ifndef _IOTSABLECLIENT_H_
#define _IOTSABLECLIENT_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include <BLEDevice.h>
#include "iotsaBLEClientConnection.h"

typedef std::function<void(BLEAdvertisedDevice&)> BleDeviceFoundCallback;

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

  // These are all the devices ever seen. They are saved persistently.
  std::map<std::string, IotsaBLEClientConnection*> devices;
  bool addDevice(std::string id, BLEAdvertisedDevice& device);
  void delDevice(std::string id);
  void clearDevicesSeen();
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
  std::map<std::string, IotsaBLEClientConnection*> connections;
};

#endif
