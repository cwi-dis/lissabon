#ifndef _IOTSABLECLIENT_H_
#define _IOTSABLECLIENT_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaBle.h"
#include "iotsaBLEClientConnection.h"

#include <set>

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
  virtual String formHandler_field_perdevice(const char *deviceName);
  virtual void setup() override;
  virtual void serverSetup() override;
  virtual void handler();
  virtual void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig);
  virtual bool formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig);

  virtual void loop() override;
  virtual String info() override { return ""; }
  //
  // Interfaces for use by subclasses (or other classes with a reference)
  // to control which BLE devices are known by this class
  //
  IotsaBLEClientConnection* addDevice(std::string id);
  IotsaBLEClientConnection* addDevice(String id) { return addDevice(std::string(id.c_str())); }
  IotsaBLEClientConnection* getDevice(std::string id);
  IotsaBLEClientConnection* getDevice(String id) { return getDevice(std::string(id.c_str())); }
  void delDevice(std::string id);
  void delDevice(String id) { delDevice(std::string(id.c_str())); }
  void deviceNotConnectable(std::string id);
  void deviceNotConnectable(String id) { deviceNotConnectable(std::string(id.c_str())); }
  bool canConnect();
  //
  // Interfaces to control which BLE devices are visible to this
  // class (and any subclass)
  //
  void findUnknownDevices(bool on);
  void setUnknownDeviceFoundCallback(BleDeviceFoundCallback _callback);
  void setDuplicateNameFilter(bool noDuplicates);
  void setServiceFilter(const BLEUUID& serviceUUID);
  void setManufacturerFilter(uint16_t manufacturerID);
protected:
  // These are all the known devices (known by the application, not by this module)
  std::map<std::string, IotsaBLEClientConnection*> devices;
  // These are all known devices by address
  std::map<std::string, IotsaBLEClientConnection *>devicesByAddress;
  // These are all names of unknown devices
  std::set<std::string> unknownDevices;
protected:
  void configLoad();
  void configSave();
  void onResult(BLEAdvertisedDevice *advertisedDevice);
  void updateScanning();
  void startScanning();
  void stopScanning();
  void startScanUnknown();
  static void scanComplete(BLEScanResults results);
  static IotsaBLEClientMod *scanningMod;
  bool scanForUnknownClients = false;
  uint32_t scanUnknownUntilMillis = 0;
  bool shouldUpdateScan = false;
  uint32_t dontUpdateScanBefore = 0;
  bool disconnectClientsForScan = false;
  BLEScan *scanner = NULL;
  BleDeviceFoundCallback callback = NULL;
  bool duplicateNameFilter = false;
  BLEUUID* serviceFilter = NULL;
  uint16_t manufacturerFilter;
  bool hasManufacturerFilter = false;
};

#endif
