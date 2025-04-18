#ifdef IOTSA_WITH_BLE
#include "iotsa.h"
#include "iotsaBLEClient.h"
#include "iotsaConfigFile.h"

//
// IotsaBLEClientMod is intended to be used as a base class
// for other modules (which will then save configurations, etc, for the 
// devices the module is interested in). 
//
// But defining the following will allow IotsaBLEClient to be used to connect to generic
// BLE devices.
#undef IOTSA_BLE_GENERIC

//
// For debugging: define this to print all client advertisements received
#undef DEBUG_PRINT_ALL_CLIENTS

const int SCAN_START_RETRY_MS = 1000; // How long to wait before retrying start scan
const int SCAN_UNKNOWN_DURATION_MS = 20000; // How long to scan for unknown clients

void IotsaBLEClientMod::configLoad() {
  IotsaConfigFileLoad cf("/config/bleclient.cfg");
  cf.get("scan_interval", scan_interval, scan_interval);
  cf.get("scan_window", scan_window, scan_window);
#ifdef IOTSA_BLE_GENERIC
  devices.clear();
  int nDevice;
  cf.get("nDevice", nDevice, 0);
  for (int i=0; i < nDevice; i++) {
    String name = "bledevice." + String(i);
    String id;
    cf.get(name, id, "");
    if (id) {
      std::string idd(id.c_str());
      IotsaBLEClientConnection* conn = new IotsaBLEClientConnection(idd);
      devices[idd] = conn;
    }
  }
#endif // IOTSA_BLE_GENERIC
}

void IotsaBLEClientMod::configSave() {
  IotsaConfigFileSave cf("/config/bleclient.cfg");
  cf.put("scan_interval", scan_interval);
  cf.put("scan_window", scan_window);
#ifdef IOTSA_BLE_GENERIC
  cf.put("nDevice", (int)devices.size());
  int i = 0;
  for (auto it : devices) {
    String name = "bledevice." + String(i);
    String id(it.first.c_str());
    cf.put(name, id);
  }
#endif // IOTSA_BLE_GENERIC
}

void IotsaBLEClientMod::setup() {
  IFDEBUG IotsaSerial.println("BLEClientmod::setup()");
  configLoad();
  BLEDevice::init(iotsaConfig.hostName.c_str());
  setupScanner();
#ifdef IOTSA_BLE_GENERIC
  //
  // Setup callback so we are informaed of unknown dimmers.
  //
  auto callback = std::bind(&IotsaBLEClientMod::unknownBLEDeviceFound, this, std::placeholders::_1);
  setUnknownDeviceFoundCallback(callback);
#endif // IOTSA_BLE_GENERIC
}

void IotsaBLEClientMod::setupScanner() {
  // The scanner is a singleton. We initialize it once.
  scanner = BLEDevice::getScan();
  scanner->setScanCallbacks(this, false);
  scanner->setActiveScan(true);
  scanner->setInterval(scan_interval);
  scanner->setWindow(scan_window);
  scanner = NULL;

}

#ifdef IOTSA_BLE_GENERIC
void IotsaBLEClientMod::unknownBLEDeviceFound(BLEAdvertisedDevice& deviceAdvertisement) {
  IFDEBUG IotsaSerial.printf("unknownBLEDeviceFound:  \"%s\"\n", deviceAdvertisement.getName().c_str());
  unknownDevices.insert(deviceAdvertisement.getName());
}
#endif // IOTSA_BLE_GENERIC

#ifdef IOTSA_WITH_WEB
void
IotsaBLEClientMod::handler() {
#ifdef IOTSA_BLE_GENERIC
  bool anyChanged = false;
  anyChanged |= formHandler_args(server, "", true);
  if (anyChanged) saveConfig();
  String message = "<html><head><title>BLE Devices</title></head><body><h1>BLE Devices</h1>";

  formHandler_fields(message, "BLE devices", "bledevice", true);

  message += "<form method='post'><input type='submit' name='refresh' value='Refresh'></form>";
  message += "</body></html>";
  server->send(200, "text/html", message);
#endif
}

void IotsaBLEClientMod::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
  // xxxjack fixed number of dimmers, so no need for "new" form.

  message += "<h2>Available Unknown/new " + text + " devices</h2>";
  message += "<form method='post'><input type='submit' name='scanUnknown' value='Scan for 20 seconds'></form>";
  message += "<form method='post'><input type='submit' name='refresh' value='Refresh'></form>";
  if (unknownDevices.size() == 0) {
    message += "<p>No unassigned BLE dimmer devices seen recently.</p>";
  } else {
    message += "<ul>";
    for (auto it: unknownDevices) {
      message += "<li>" + formHandler_field_perdevice(it.c_str()) + "</li>";
    }
    message += "</ul>";
  }
}

String IotsaBLEClientMod::formHandler_field_perdevice(const char *deviceName) {
  return String(deviceName);
}

bool IotsaBLEClientMod::formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) {
  if (server->hasArg("scanUnknown")) startScanUnknown();
  return false;
}

#endif // IOTSA_WITH_WEB

bool IotsaBLEClientMod::getHandler(const char *path, JsonObject& reply) {
  reply["scan_interval"] = scan_interval;
  reply["scan_window"] = scan_window;
  if (unknownDevices.size()) {
    JsonArray unknownReply = reply["unassigned"].as<JsonArray>();
    for (auto it : unknownDevices) {
      unknownReply.add((char *)it.c_str());
    }
  }
  reply["scanUnknown"] = (char *)NULL;
  return true;
}
bool IotsaBLEClientMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  bool _startScanUnknown = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (getFromRequest<int>(reqObj, "scan_interval", scan_interval)) {
    scan_interval = reqObj["scan_interval"];
    anyChanged = true;
  }
  if (getFromRequest<int>(reqObj, "scan_window", scan_window)) {
    scan_window = reqObj["scan_window"];
    anyChanged = true;
  }
  if (reqObj["scanUnknown"]|0) {
    _startScanUnknown = true;
  }
  if (anyChanged) {
    configSave();
    setupScanner();
  }
  if (_startScanUnknown) {
    startScanUnknown();
  }
  return anyChanged;
}

void IotsaBLEClientMod::startScanUnknown() {
  findUnknownDevices(true);
  scanUnknownUntilMillis = millis() + SCAN_UNKNOWN_DURATION_MS;
  iotsaConfig.postponeSleep(SCAN_UNKNOWN_DURATION_MS+1000);
}

void IotsaBLEClientMod::findUnknownDevices(bool on) {
  scanForUnknownClients = on;
  shouldUpdateScanAtMillis = millis();
}

bool IotsaBLEClientMod::isScanning() {
  return scanner != nullptr && scanner->isScanning();
}

unsigned int IotsaBLEClientMod::maxConnectionKeepOpen() {
  int rv = 30000; // Random large value
  if (shouldUpdateScanAtMillis != 0) {
    int rv2 = shouldUpdateScanAtMillis - millis();
    if (rv2 < 0) rv2 = 0;
    rv = rv2;
  }
  return rv;
}

void IotsaBLEClientMod::updateScanning() {
  // We should be scanning if scanForUnknownClients is true or any of
  // our known devices does not have an address associated with it yet.
  bool shouldScan = scanForUnknownClients;
  if (!shouldScan) {
    for (auto it: devices) {
      if (!it.second->available()) {
        //IotsaSerial.printf("BLE scan required for %s\n", it.second->getName().c_str());
        shouldScan = true;
        break;
      }
    }
  }
  if (shouldScan) {
    // if we should be scanning and we aren't we start scanning
    if (!isScanning()) {
      IFDEBUG {
        IotsaSerial.print("BLE scan for: ");
        if (scanForUnknownClients) { 
          IotsaSerial.print("(new/unknown) ");
        }
        for (auto it: devices) {
          if (!it.second->available()) {
            IotsaSerial.printf("%s ", it.second->getName().c_str());
          }
        }
        IotsaSerial.println();
      }
      startScanning();
    }
  } else {
    // if we should not be scanning but we are: we stop
    if (isScanning()) {
      stopScanning();
    }
  }
}

void IotsaBLEClientMod::startScanning() {
  if (isScanning()) {
    IotsaSerial.println("IotsaBLEClientMod.startScanning: already scanning...");
    return;
  }
  IFDEBUG IotsaSerial.println("IotsaBLEClientMod: BLE scan start");
#if 0
  // First close all connections. Scanning while connected has proved to result in issues.
  for (auto it : devices) {
    if (it.second && it.second->isConnected()) {
      if (!disconnectClientsForScan) {
        // Don't scan if any active clients. But next time around we will disconnect them
        IFDEBUG IotsaSerial.println("BLE scan aborted: active connection");
        shouldUpdateScan = true;
        dontUpdateScanBefore = millis() + PAUSE_BETWEEN_SCANS;
        disconnectClientsForScan = true;
        return;
      }
      IFDEBUG IotsaSerial.printf("BLE scan start: disconnect %s\n", it.second->name.c_str());
      it.second->disconnect();
    }
  }
#endif
  // Now start the scan
  scanner = BLEDevice::getScan();
  scanningMod = this;
  if (!scanner->start(11000)) {
    scanner = nullptr;
    IFDEBUG IotsaSerial.println("BLEClient: cannot start scan, retry in 1s");
    shouldUpdateScanAtMillis = millis() + SCAN_START_RETRY_MS;
    return;
  }
  scanningChanged();
#if 0
  // Do not sleep until scan is done
  iotsaConfig.pauseSleep();
#endif
}

void IotsaBLEClientMod::stopScanning() {
  if (false && !isScanning()) {
    IotsaSerial.println("IotsaBLEClientMod.stopScanning: not scanning...");
  } else {
    IFDEBUG IotsaSerial.println("IotsaBLEClientMod.stopScanning: BLE scan stop");
    scanner->stop();
    scanner = NULL;
    scanningMod = NULL;
    // We can sleep again, but give a bit of time to cient-connection objects to
    // react to scan results.
  #if 0
    iotsaConfig.resumeSleep();
    iotsaConfig.postponeSleep(100);
  #endif
    scanningChanged();
  }
  // Next time through loop, check whether we should scan again.
  shouldUpdateScanAtMillis = millis()+noScanMillis;
}

bool IotsaBLEClientMod::canConnect() {
#if 0
  // Connecting to a device while we are scanning has proved to result in issues.
  return scanner == NULL;
#else
  return true;
#endif
}

IotsaBLEClientMod* IotsaBLEClientMod::scanningMod = NULL;

void IotsaBLEClientMod::scanComplete(BLEScanResults results) {
    IFDEBUG IotsaSerial.println("IotsaBLEClientMod: BLE scan complete");
    iotsaConfig.resumeSleep();
    if (scanningMod) scanningMod->stopScanning();
}

void IotsaBLEClientMod::serverSetup() {
  api.setup("/api/bleclient", true, true, false);
  name = "bleclient";
}

void IotsaBLEClientMod::setUnknownDeviceFoundCallback(BleDeviceFoundCallback _callback) {
  unknownDeviceCallback = _callback;
}

void IotsaBLEClientMod::setKnownDeviceChangedCallback(BleDeviceFoundCallback _callback) {
  knownDeviceCallback = _callback;
}

void IotsaBLEClientMod::setDuplicateNameFilter(bool noDuplicateNames) {
  duplicateNameFilter = noDuplicateNames;
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
  if (scanner != nullptr && !scanner->isScanning()) {
    stopScanning();
  }
  if (scanUnknownUntilMillis != 0 && millis() > scanUnknownUntilMillis) {
    scanUnknownUntilMillis = 0;
    findUnknownDevices(false);
  }
  if (shouldUpdateScanAtMillis != 0 && millis() >= shouldUpdateScanAtMillis) {
    shouldUpdateScanAtMillis = 0;
    updateScanning();
  }
}

void IotsaBLEClientMod::onResult(const BLEAdvertisedDevice *advertisedDevice) {
#ifdef DEBUG_PRINT_ALL_CLIENTS
  IotsaSerial.printf("BLEClientMod::onResult(%s)\n", advertisedDevice->toString().c_str());
#endif
  // Is this an advertisement for a device we know, either by name or by address?
  std::string deviceName = advertisedDevice->getName();
  if (deviceName == "") return;
  auto it = devices.find(deviceName);
  if (it != devices.end()) {
    auto dev = it->second;
    if (dev == nullptr) {
      IotsaSerial.printf("BLEClientMod: device byName \"%s\" is NULL\n", deviceName.c_str());
      return;
    }
    bool changed = dev->receivedAdvertisement(*advertisedDevice);
    if (changed) {
      devicesByAddress[advertisedDevice->getAddress().toString()] = dev;
      IFDEBUG IotsaSerial.printf("BLEClientMod: advertisement update byname for %s\n", deviceName.c_str());
      knownDeviceCallback(*advertisedDevice);
    }
    shouldUpdateScanAtMillis = millis(); // We may have found what we were looking for
    return;
  }
  std::string addr = advertisedDevice->getAddress().toString();
  auto it2 = devicesByAddress.find(addr);
  if (it2 != devicesByAddress.end()) {
    auto dev = it->second;
    if (dev == nullptr) {
      IotsaSerial.printf("BLEClientMod: device byAddress \"%s\" is NULL\n", addr.c_str());
      return;
    }
    bool changed = dev->receivedAdvertisement(*advertisedDevice);
    if (changed) {
      devicesByAddress[advertisedDevice->getAddress().toString()] = dev;
      IFDEBUG IotsaSerial.printf("BLEClientMod: advertisement update byaddress for %s\n", deviceName.c_str());
      knownDeviceCallback(*advertisedDevice);
    }
    shouldUpdateScanAtMillis = millis(); // We may have found what we were looking for
    return;
  }
  // Do we want callbacks for unknown devices?
  if (unknownDeviceCallback == NULL) return;
  // Have we seen this unknown device before?
  if ( duplicateNameFilter && unknownDevices.find(advertisedDevice->getName()) != unknownDevices.end()) return;
  // Do we filter on services?
  if (serviceFilter != NULL) {
    if (!advertisedDevice->isAdvertisingService(*serviceFilter)) return;
  }
  // Do we filter on manufacturer data?
  if (hasManufacturerFilter) {
    std::string mfgData(advertisedDevice->getManufacturerData());
    if (mfgData.length() < 2) return;
    const uint16_t *mfg = (const uint16_t *)mfgData.c_str();
    if (*mfg != manufacturerFilter) return;
  }
  unknownDeviceCallback(*advertisedDevice);
}

IotsaBLEClientConnection* IotsaBLEClientMod::addDevice(std::string id) {
  auto it = devices.find(id);
  if (it == devices.end()) {
    // Device with this ID doesn't exist yet. Add it.
    IotsaBLEClientConnection* dev = new IotsaBLEClientConnection(id);
    devices[id] = dev;
#if 0
    // xxxjack bad idea to save straight away... 
    configSave();
#endif
    return dev;
  }
  shouldUpdateScanAtMillis = millis(); // We probably want to scan for the new device
  return it->second;
}

IotsaBLEClientConnection* IotsaBLEClientMod::getDevice(std::string id) {
  auto it = devices.find(id);
  if (it == devices.end()) {
    return NULL;
  }
  return it->second;
}

void IotsaBLEClientMod::deviceNotConnectable(std::string id) {
  IotsaBLEClientConnection *dev;
  dev = addDevice(id);
  if (dev == NULL) return;
  dev->clearDevice();
  shouldUpdateScanAtMillis = millis(); // We may want to start scanning again
}

void IotsaBLEClientMod::delDevice(std::string id) {
  shouldUpdateScanAtMillis = millis();  // We may be able to stop scanning
  int nDeleted = devices.erase(id);
#if 0
  // xxxjack bad idea to save config stright away
  if (nDeleted > 0) configSave();
#endif
}
#endif // IOTSA_WITH_BLE
