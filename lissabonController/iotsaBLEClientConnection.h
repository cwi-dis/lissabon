#ifndef _IOTSABLECLIENTCONNECTION_H_
#define _IOTSABLECLIENTCONNECTION_H_
#include "iotsa.h"
#include <BLEDevice.h>

typedef std::function<void(uint8_t *, size_t)> BleNotificationCallback;

class IotsaBLEClientConnection {
  friend class IotsaBLEClientMod;
public:
  IotsaBLEClientConnection(std::string& _name);
  bool setDevice(BLEAdvertisedDevice& _device);
  void clearDevice();
  bool available();
  bool connect();
  void disconnect();
  bool set(BLEUUID& service, BLEUUID& charUUID, const uint8_t *data, size_t size);
  bool set(BLEUUID& service, BLEUUID& charUUID, uint8_t value);
  bool set(BLEUUID& service, BLEUUID& charUUID, uint16_t value);
  bool set(BLEUUID& service, BLEUUID& charUUID, uint32_t value);
  bool set(BLEUUID& service, BLEUUID& charUUID, const std::string& value);
  bool set(BLEUUID& service, BLEUUID& charUUID, const String& value);
  bool getAsBuffer(BLEUUID& service, BLEUUID& charUUID, uint8_t **datap, size_t *sizep);
  int getAsInt(BLEUUID& service, BLEUUID& charUUID);
  std::string getAsString(BLEUUID& service, BLEUUID& charUUID);
  bool getAsNotification(BLEUUID& service, BLEUUID& charUUID, BleNotificationCallback callback);
protected:
  std::string name;
  BLERemoteCharacteristic *_getCharacteristic(BLEUUID& service, BLEUUID& charUUID);
  BLEAdvertisedDevice *device;
  BLEClient *client;
};
#endif