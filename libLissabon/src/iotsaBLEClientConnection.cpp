#include "iotsaBLEClientConnection.h"

IotsaBLEClientConnection::IotsaBLEClientConnection(std::string& _name, std::string _address)
: name(_name),
  address(_address),
  addressType(BLE_ADDR_TYPE_PUBLIC),
  addressValid(false),
  client()
{
  if (_address != "") {
    // address and addressType have already been set
    addressValid = true;
  }
}

std::string IotsaBLEClientConnection::getAddress() {
  if (!addressValid || addressType != BLE_ADDR_TYPE_PUBLIC) return "";
  return address.toString();
}

bool IotsaBLEClientConnection::receivedAdvertisement(BLEAdvertisedDevice& _device) {
  // Check whether the address is the same, then we don't have to add anything.
  if (addressValid && _device.getAddressType() == addressType && _device.getAddress().equals(address)) {
    return false;
  }
  disconnect();
  address = _device.getAddress();
  addressType = _device.getAddressType();
  addressValid = true;
  return true;
}

void IotsaBLEClientConnection::clearDevice() {
  addressValid = false;
  disconnect();
}

bool IotsaBLEClientConnection::available() {
  return addressValid;
}

bool IotsaBLEClientConnection::connect() {
  if (!addressValid) return false;
  if (client.isConnected()) return true;
  return client.connect(address, addressType);
}

void IotsaBLEClientConnection::disconnect() {
  if (client.isConnected()) {
    client.disconnect();
  }
}

bool IotsaBLEClientConnection::isConnected() {
  return client.isConnected();
}

BLERemoteCharacteristic *IotsaBLEClientConnection::_getCharacteristic(BLEUUID& serviceUUID, BLEUUID& charUUID) {
  BLERemoteService *service = client.getService(serviceUUID);
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
bool IotsaBLEClientConnection::get(BLEUUID& serviceUUID, BLEUUID& charUUID, uint8_t& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  if (size != sizeof(uint8_t)) return false;
  value = *(uint8_t *)ptr;
  return true;
}

bool IotsaBLEClientConnection::get(BLEUUID& serviceUUID, BLEUUID& charUUID, uint16_t& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  if (size != sizeof(uint16_t)) return false;
  value = *(uint16_t *)ptr;
  return true;
}

bool IotsaBLEClientConnection::get(BLEUUID& serviceUUID, BLEUUID& charUUID, uint32_t& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  if (size != sizeof(uint32_t)) return false;
  value = *(uint32_t *)ptr;
  return true;
}

bool IotsaBLEClientConnection::get(BLEUUID& serviceUUID, BLEUUID& charUUID, std::string& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  value = std::string((const char *)ptr, size);
  return true;
}
#ifdef oldapi
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
#endif // oldapi

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

