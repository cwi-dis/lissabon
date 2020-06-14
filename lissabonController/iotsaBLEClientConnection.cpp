#include "iotsaBLEClientConnection.h"

IotsaBLEClientConnection::IotsaBLEClientConnection(std::string& _name)
: name(_name),
  device(NULL),
  client(NULL)
{}

bool IotsaBLEClientConnection::setDevice(BLEAdvertisedDevice& _device) {
  if (device != NULL) {
    // Check whether the address is the same, then we don't have to add anything.
    if (_device.getAddressType() == device->getAddressType() && _device.getAddress().equals(device->getAddress())) {
      return false;
    }
  }
  // No device, or different address.
  if (device != NULL) delete device;
  device = new BLEAdvertisedDevice(_device);
  return true;
}

void IotsaBLEClientConnection::clearDevice() {
  if (device != NULL) delete device;
}

bool IotsaBLEClientConnection::available() {
  return device != NULL;
}

bool IotsaBLEClientConnection::connect() {
  if (device == NULL) return false;
  if (client != NULL) return true;
  client = BLEDevice::createClient();
  bool ok = client->connect(device);
  if (!ok) delete client;
  return ok;
}

void IotsaBLEClientConnection::disconnect() {
  if (client != NULL) {
    client->disconnect();
    delete client;
  }
}

BLERemoteCharacteristic *IotsaBLEClientConnection::_getCharacteristic(BLEUUID& serviceUUID, BLEUUID& charUUID) {
  if (client == NULL) return NULL;
  BLERemoteService *service = client->getService(serviceUUID);
  if (service == NULL) return NULL;
  BLERemoteCharacteristic *characteristic = service->getCharacteristic(charUUID);
  return characteristic;
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, const uint8_t *data, size_t size) {
  BLERemoteCharacteristic *characteristic = _getCharacteristic(serviceUUID, charUUID);
  if (characteristic == NULL) return false;
  if (!characteristic->canWrite()) return false;
  characteristic->writeValue((uint8_t *)data, size);
  return true;
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, uint8_t value) {
  return set(serviceUUID, charUUID, (const uint8_t *)&value, 1);
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, uint16_t value) {
  return set(serviceUUID, charUUID, (const uint8_t *)&value, 2);
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, uint32_t value) {
  return set(serviceUUID, charUUID, (const uint8_t *)&value, 4);
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, const std::string& value) {
  return set(serviceUUID, charUUID, (const uint8_t *)value.c_str(), value.length());
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, const String& value) {
  return set(serviceUUID, charUUID, (const uint8_t *)value.c_str(), value.length());
}

bool IotsaBLEClientConnection::getAsBuffer(BLEUUID& serviceUUID, BLEUUID& charUUID, uint8_t **datap, size_t *sizep) {
  BLERemoteCharacteristic *characteristic = _getCharacteristic(serviceUUID, charUUID);
  if (characteristic == NULL) return false;
  if (!characteristic->canRead()) return false;
  std::string value = characteristic->readValue();
  *datap = (uint8_t *)value.c_str();
  *sizep = value.length();
  return true;
}

int IotsaBLEClientConnection::getAsInt(BLEUUID& serviceUUID, BLEUUID& charUUID) {
  size_t size;
  uint8_t *ptr;
  int val = 0;
  int shift = 0;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return 0;
  while (size--) {
    val = val | (*ptr++ << shift);
    shift += 8;
  }
  return val;
}

std::string IotsaBLEClientConnection:: getAsString(BLEUUID& serviceUUID, BLEUUID& charUUID) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return "";
  return std::string((const char *)ptr, size);
}

static BleNotificationCallback _staticCallback;

static void _staticCallbackCaller(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (_staticCallback) _staticCallback(pData, length);
}

bool IotsaBLEClientConnection::getAsNotification(BLEUUID& serviceUUID, BLEUUID& charUUID, BleNotificationCallback callback) {
  if (_staticCallback != NULL) {
    IotsaSerial.println("IotsaBLEClientConnection: only a single notification supported");
    return false;
  }
  BLERemoteCharacteristic *characteristic = _getCharacteristic(serviceUUID, charUUID);
  if (characteristic == NULL) return false;
  if (!characteristic->canNotify()) return false;
  _staticCallback = callback;
  characteristic->registerForNotify(_staticCallbackCaller);
  return false;
}

