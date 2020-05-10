#include "iotsa.h"
#include "iotsaBLEClient.h"
#include "iotsaConfigFile.h"

void IotsaBLEClientMod::configLoad() {
#if 0
  IotsaConfigFileLoad cf("/config/bleclient.cfg");
  devices.clear();
  int nDevice;
  cf.get("nDevice", nDevice, 0);
  for (int i=0; i < nDevice; i++) {
    String name = "device" + String(i);
    String id;
    cf.get(name, id, "");
    if (id) {
      std::string idd(id.c_str());
      IotsaBLEClientConnection* conn = new IotsaBLEClientConnection(idd);
      devices[idd] = conn;
    }
  }
#endif
}

void IotsaBLEClientMod::configSave() {
#if 0
  IotsaConfigFileSave cf("/config/bleclient.cfg");
  cf.put("nDevice", (int)devices.size());
  int i = 0;
  for (auto it : devices) {
    String name = "device" + String(i);
    String id(it.first.c_str());
    cf.put(name, id);
  }
#endif
}

void IotsaBLEClientMod::setup() {
  IFDEBUG IotsaSerial.println("BLEClientmod::setup()");
  configLoad();
  BLEDevice::init(iotsaConfig.hostName.c_str());
  // The scanner is a singleton. We initialize it once.
  scanner = BLEDevice::getScan();
  scanner->setAdvertisedDeviceCallbacks(this, true);
  scanner->setActiveScan(false);
  scanner->setInterval(155);
  scanner->setWindow(151);
  scanner = NULL;
}

void IotsaBLEClientMod::findUnknownClients(bool on) {
  scanForUnknownClients = on;
  shouldUpdateScan = true;
}

void IotsaBLEClientMod::updateScanning() {
  // We should be scanning if scanForUnknownClients is true or any of
  // our known devices does not have an address associated with it yet.
  IotsaSerial.println("xxxjack updateScanning");
  bool shouldScan = scanForUnknownClients;
  if (!shouldScan) {
    for (auto it: devices) {
      if (!it.second->available()) {
        shouldScan = true;
        break;
      }
    }
  }
  if (shouldScan) {
    // if we should be scanning and we aren't we start scanning
    if (scanner == NULL) startScanning();
  } else {
    // if we should not be scanning but we are: we stop
    if (scanner != NULL) stopScanning();
  }
}

void IotsaBLEClientMod::startScanning() {
  scanner = BLEDevice::getScan();
  IFDEBUG IotsaSerial.println("BLE scan start");
  scanningMod = this;
  scanner->start(6, &IotsaBLEClientMod::scanComplete, false);
  iotsaConfig.pauseSleep();
}

void IotsaBLEClientMod::stopScanning() {
  if (scanner) {
    IFDEBUG IotsaSerial.println("BLE scan stop");
    scanner->stop();
    iotsaConfig.resumeSleep();
    scanner = NULL;
    scanningMod = NULL;
  }
  shouldUpdateScan = true;
}

IotsaBLEClientMod* IotsaBLEClientMod::scanningMod = NULL;

void IotsaBLEClientMod::scanComplete(BLEScanResults results) {
    IFDEBUG IotsaSerial.println("BLE scan complete");
    iotsaConfig.resumeSleep();
    if (scanningMod) scanningMod->stopScanning();
}

void IotsaBLEClientMod::serverSetup() {
}

void IotsaBLEClientMod::setDeviceFoundCallback(BleDeviceFoundCallback _callback) {
  callback = _callback;
}

void IotsaBLEClientMod::setServiceFilter(const BLEUUID& serviceUUID) {
  if (serviceFilter) delete serviceFilter;
  serviceFilter = new BLEUUID(serviceUUID);
}

void IotsaBLEClientMod::setManufacturerFilter(uint16_t manufacturerID) {
  manufacturerFilter = manufacturerID;
  hasManufacturerFilter = true;
}

void IotsaBLEClientMod::loop() {
  if (shouldUpdateScan) {
    shouldUpdateScan = false;
    updateScanning();
  }
}

void IotsaBLEClientMod::onResult(BLEAdvertisedDevice advertisedDevice) {
  IFDEBUG IotsaSerial.printf("BLEClientMod::onResult(%s)\n", advertisedDevice.toString().c_str());
  // Do we want callbacks?
  if (callback == NULL) return;
  // Do we filter on services?
  if (serviceFilter != NULL) {
    if (!advertisedDevice.isAdvertisingService(*serviceFilter)) return;
  }
  // Do we filter on manufacturer data?
  if (hasManufacturerFilter) {
    std::string mfgData(advertisedDevice.getManufacturerData());
    if (mfgData.length() < 2) return;
    const uint16_t *mfg = (const uint16_t *)mfgData.c_str();
    if (*mfg != manufacturerFilter) return;
  }
  callback(advertisedDevice);
}

IotsaBLEClientConnection* IotsaBLEClientMod::addDevice(std::string id) {
  auto it = devices.find(id);
  if (it == devices.end()) {
    // Device with this ID doesn't exist yet. Add it.
    IotsaBLEClientConnection* dev = new IotsaBLEClientConnection(id);
    devices[id] = dev;
    configSave();
    return dev;
  }
  shouldUpdateScan = true; // We probably want to scan for the new device
  return it->second;
}

IotsaBLEClientConnection* IotsaBLEClientMod::addDevice(String id) {
  return addDevice(std::string(id.c_str()));
}

IotsaBLEClientConnection* IotsaBLEClientMod::getDevice(std::string id) {
  auto it = devices.find(id);
  if (it == devices.end()) {
    return NULL;
  }
  return it->second;
}

IotsaBLEClientConnection* IotsaBLEClientMod::getDevice(String id) {
  return getDevice(std::string(id.c_str()));
}

bool IotsaBLEClientMod::deviceSeen(std::string id, BLEAdvertisedDevice& device, bool add) {
  IotsaBLEClientConnection *dev;
  if (add) {
    dev = addDevice(id);
  } else {
    dev = getDevice(id);
  }
  if (dev == NULL) return false;
  // And we tell the device about the advertisement getManufacturerData
  dev->setDevice(device);
  shouldUpdateScan = true;  // We may be able to stop scanning
  return true;
}

void IotsaBLEClientMod::deviceNotSeen(std::string id) {
  IotsaBLEClientConnection *dev;
  dev = addDevice(id);
  if (dev == NULL) return;
  dev->clearDevice();
  shouldUpdateScan = true; // We may want to start scanning again
}

void IotsaBLEClientMod::deviceNotSeen(String id) {
  deviceNotSeen(std::string(id.c_str()));
}

void IotsaBLEClientMod::delDevice(std::string id) {
  shouldUpdateScan = true;  // We may be able to stop scanning
  devices.erase(id);
  configSave();
}

void IotsaBLEClientMod::delDevice(String id) {
  delDevice(std::string(id.c_str()));
}
