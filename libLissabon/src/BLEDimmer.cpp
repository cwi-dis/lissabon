#ifdef IOTSA_WITH_BLE
#include "BLEDimmer.h"
#include "iotsaBLEClient.h"
#include "LissabonBLE.h"

namespace Lissabon {

// How long we keep trying to connect to a dimmer
#define IOTSA_BLEDIMMER_CONNECT_TIMEOUT 10000

// How long we keep open a ble connection (in case we have a quick new command)
// #define IOTSA_BLEDIMMER_KEEPOPEN_MILLIS 1000

bool BLEDimmer::available() {
  return _ensureConnection() && dimmer->available();
}

void BLEDimmer::updateDimmer() {
  IotsaSerial.printf("%s.updateDimmer() called\n", name.c_str());
  needSyncToDevice = true;
  needTransmitTimeoutAtMillis = millis() + IOTSA_BLEDIMMER_CONNECT_TIMEOUT;
  if (callbacks) callbacks->dimmerValueChanged();
}

bool BLEDimmer::setName(String value) {
  if (value == name) return false;
  if (name) bleClientMod.delDevice(name);
  if (dimmer) {
    dimmer->clearDevice();
    dimmer = nullptr;
  }
  name = value;
  if (value) bleClientMod.addDevice(name);
  return true;
}
void BLEDimmer::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
  AbstractDimmer::formHandler_fields(message, text, f_name, includeConfig);
  if (!_dataValid) {
    message += "<i>(data may be outdated or invalid)</i><br>";
  }
  if (includeConfig) {
    message += "BLE device name: <input name='" + f_name +".name' value='" + name + "'><br>";
    _ensureConnection();
    if (dimmer && dimmer->available()) {
      message += "BLE device address: " + String(dimmer->getAddress().c_str()) + "<br>";
    } else {
      message += "<em>BLE device not available</em><br>";
    }
  }
}

bool BLEDimmer::BLEDimmer::configLoad(IotsaConfigFileLoad& cf, const String& name) {
  // xxxjack to be done...
  return AbstractDimmer::configLoad(cf, name);
}

void BLEDimmer::configSave(IotsaConfigFileSave& cf, const String& n_name) {
//xxxjack  String s_num = String(num);
//xxxjack  String s_name = "dimmer" + s_num;
  _ensureConnection();
  if (dimmer) {
    String address = String(dimmer->getAddress().c_str());
    if (address != "") cf.put(n_name + ".address", address);
  }
  AbstractDimmer::configSave(cf, n_name);
}

void BLEDimmer::getHandler(JsonObject& reply) {
  if (_ensureConnection()) {
    std::string address = dimmer->getAddress();
    if (address != "") reply["address"] = String(address.c_str());
  }
  AbstractDimmer::getHandler(reply);
}

void BLEDimmer::setup() {
  if (listenForDeviceChanges) {
    needSyncFromDevice = listenForDeviceChanges;
    _dataValid = false;
    needTransmitTimeoutAtMillis = millis() + IOTSA_BLEDIMMER_CONNECT_TIMEOUT;
  } else {
    _dataValid = true;
  }
}

void BLEDimmer::refresh() {
  if (listenForDeviceChanges) {
    needSyncFromDevice = listenForDeviceChanges;   
  } else {
    _dataValid = true;
  }
}

void BLEDimmer::followDimmerChanges(bool follow) { 
  listenForDeviceChanges = follow; 
  if (listenForDeviceChanges) {
    needSyncFromDevice = listenForDeviceChanges;
    _dataValid = false;
    needTransmitTimeoutAtMillis = millis() + IOTSA_BLEDIMMER_CONNECT_TIMEOUT;
  } else {
    _dataValid = true;
  }
}

void BLEDimmer::identify() {
  needIdentify = true;
  updateDimmer();
}
 
void BLEDimmer::loop() {
  // If we don't have anything to transmit we bail out quickly...
  if (!needSyncToDevice && !needSyncFromDevice) {

    // But we first disconnect if we are connected-idle for long enough.
    if (disconnectAtMillis > 0 && millis() > disconnectAtMillis) {
      if (_ensureConnection()) dimmer->disconnect();
      callbacks->dimmerAvailableChanged(true, false);
      disconnectAtMillis = 0;
    }
    return;
  }
  // We have something to transmit/receive. Check whether our dimmer actually exists.
  if (!_ensureConnection()) {
    IFDEBUG IotsaSerial.printf("BLEDimmer: Skip connection to nonexistent dimmer %d %s\n", num, name.c_str());
    needSyncToDevice = false;
    needSyncFromDevice = false;
    return;
  }
  // If it exists, check that we have enough information to connect.
  if (!dimmer->available()) {
    //IotsaSerial.println("xxxjack dimmer not available");
    if (millis() > needTransmitTimeoutAtMillis) {
      IotsaSerial.printf("BLEDimmer: Giving up on connecting to %s\n", name.c_str());
      needSyncToDevice = false;
      needSyncFromDevice = false;
      callbacks->dimmerAvailableChanged(false, false);
      return;
    }
    // iotsaBLEClient should be listening for advertisements
    return;
  }
  // Now we can connect, unless we are already connected
  if (!dimmer->isConnected()) {
    // If we are scanning we don't try to connect
    if (!bleClientMod.canConnect()) {
      IotsaSerial.println("BLEDimmer: BLE busy, cannot connect");
      if (millis() > noWarningPrintBefore) {
        IotsaSerial.printf("BLEDimmer: BLE busy, cannot connect to %s\n", name.c_str());
        noWarningPrintBefore = millis() + 4000;
      }
      return;
    }
    noWarningPrintBefore = 0;
    // If all that is correct, try to connect.
    callbacks->dimmerAvailableChanged(true, true);
    if (!dimmer->connect()) {
      IotsaSerial.printf("BLEDimmer: connect to %s failed\n", dimmer->getName().c_str());
      bleClientMod.deviceNotConnectable(name);
      callbacks->dimmerAvailableChanged(false, false);
    }
    IFDEBUG IotsaSerial.printf("BLEDimmer: connected to %s\n", dimmer->getName().c_str());
    return; // Return: next time through the loop we will send/receive data.
  }
  
  if (needSyncFromDevice) {
    _syncFromDevice();
  }
  if (needSyncToDevice) {
    _syncToDevice();
  }
  disconnectAtMillis = millis() + keepOpenMillis;
  iotsaConfig.postponeSleep(keepOpenMillis+100);
  IFDEBUG IotsaSerial.println("BLEDimmer: keepopen");
}

void BLEDimmer::_syncToDevice() {
  bool ok;
  if (!_ensureConnection()) return;
#ifdef DIMMER_WITH_LEVEL
  // Connected to dimmer.
  if (level < 0) level = 0;
  if (level > 1) level = 1;
  Lissabon::Dimmer::Type_brightness levelValue = level * ((1<<sizeof(Lissabon::Dimmer::Type_brightness)*8)-1);
  IFDEBUG IotsaSerial.printf("%s.syncToDevice: Transmit brightness %f (%d)\n", name.c_str(), level, levelValue);
  ok = dimmer->set(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::brightnessUUID, levelValue);
  if (ok) {
    _dataValid = true;
  } else {
    IFDEBUG IotsaSerial.println("BLEDimmer: set(brightness) failed");
    _dataValid = false;
  }
#endif
#ifdef DIMMER_WITH_TEMPERATURE
  Lissabon::Dimmer::Type_temperature temperatureValue = temperature;
  IFDEBUG IotsaSerial.printf("BLEDimmer: Transmit temperature %d\n", temperatureValue);
  ok = dimmer->set(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::temperatureUUID, (Lissabon::Dimmer::Type_temperature)temperatureValue);
  if (!ok) {
    IFDEBUG IotsaSerial.println("BLEDimmer: set(temperature) failed");
  }
#endif // DIMMER_WITH_TEMPERATURE
  IFDEBUG IotsaSerial.printf("%s.syncToDevice: Transmit ison %d\n", name.c_str(), (int)isOn);
  ok = dimmer->set(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::isOnUUID, (Lissabon::Dimmer::Type_isOn)isOn);
  if (!ok) {
    IFDEBUG IotsaSerial.println("BLEDimmer: set(isOn) failed");
  }
  if (needIdentify) {
    IFDEBUG IotsaSerial.printf("%s.syncToDevice: Transmit identify\n", name.c_str());
    ok = dimmer->set(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::identifyUUID, (Lissabon::Dimmer::Type_isOn)1);
    needIdentify = false;
    if (!ok) {
      IFDEBUG IotsaSerial.println("BLEDimmer: identify failed");
    }

  }
  needSyncToDevice = false;
}

bool BLEDimmer::_ensureConnection() {
  if (dimmer != nullptr) return true;
  dimmer = bleClientMod.getDevice(name);
  return dimmer != nullptr;
}

void BLEDimmer::_syncFromDevice() {
  if (!_ensureConnection()) return;
  bool ok;
#ifdef DIMMER_WITH_LEVEL
  // Connected to dimmer.
  Lissabon::Dimmer::Type_brightness levelValue;
  ok = dimmer->get(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::brightnessUUID, levelValue);
  if (ok) {
    level = (float)levelValue / (float)((1<<sizeof(Lissabon::Dimmer::Type_brightness)*8)-1);
    IFDEBUG IotsaSerial.printf("%s.syncFromDevice: Received brightness %f (%d)\n", name.c_str(), level, levelValue);
    _dataValid = true;
  } else {
    IFDEBUG IotsaSerial.println("BLEDimmer: get(brightness) failed");
    _dataValid = false;
  }
#endif
#ifdef DIMMER_WITH_TEMPERATURE
  Lissabon::Dimmer::Type_temperature temperatureValue;
  ok = dimmer->get(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::temperatureUUID, temperatureValue);
  if (ok) {
    temperature = (float)temperatureValue;
    IFDEBUG IotsaSerial.printf("%s.syncFromDevice: Received temperature %f (%d)\n", name.c_str(), temperature, temperatureValue);
  } else {
    IFDEBUG IotsaSerial.println("BLEDimmer: get(temperature) failed");
  }
#endif // DIMMER_WITH_TEMPERATURE
  uint8_t isOnValue;
  ok = dimmer->get(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::isOnUUID, isOnValue);
  if (ok) {
    IFDEBUG IotsaSerial.printf("%s.syncFromDevice: received isOn %d\n", name.c_str(), isOnValue);
    isOn = isOnValue;
  } else {
    IFDEBUG IotsaSerial.println("BLEDimmer: set(isOn) failed");
  }
  needSyncFromDevice = false;
  if (callbacks) callbacks->dimmerValueChanged();
}

}
#endif // IOTSA_WITH_BLE