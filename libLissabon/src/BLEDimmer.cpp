#include "BLEDimmer.h"
#include "iotsaBLEClient.h"
#include "LissabonBLE.h"

namespace Lissabon {

// How long we keep trying to connect to a dimmer
#define IOTSA_BLEDIMMER_CONNECT_TIMEOUT 10000

// How long we keep open a ble connection (in case we have a quick new command)
#define IOTSA_BLEDIMMER_KEEPOPEN_MILLIS 1000

bool BLEDimmer::available() {
  IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(name);
  return dimmer != NULL && dimmer->available();
}

void BLEDimmer::updateDimmer() {
  needSyncToDevice = true;
  needTransmitTimeoutAtMillis = millis() + IOTSA_BLEDIMMER_CONNECT_TIMEOUT;
  if (callbacks) callbacks->dimmerValueChanged();
}

bool BLEDimmer::setName(String value) {
  if (value == name) return false;
  if (name) bleClientMod.delDevice(name);
  name = value;
  if (value) bleClientMod.addDevice(name);
  return true;
}
void BLEDimmer::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
  AbstractDimmer::formHandler_fields(message, text, f_name, includeConfig);
  if (includeConfig) {
    message += "BLE device name: <input name='" + f_name +".name' value='" + name + "'><br>";
    IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(name);
    if (dimmer) {
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
  IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(name);
  if (dimmer) {
    String address = String(dimmer->getAddress().c_str());
    if (address != "") cf.put(n_name + ".address", address);
  }
  AbstractDimmer::configSave(cf, n_name);
}

void BLEDimmer::getHandler(JsonObject& reply) {
  IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(name);
  if (dimmer) {
    std::string address = dimmer->getAddress();
    if (address != "") reply["address"] = String(address.c_str());
  }
  AbstractDimmer::getHandler(reply);
}

void BLEDimmer::setup() {
  if (listenForDeviceChanges) {
    needSyncFromDevice = listenForDeviceChanges;
    needTransmitTimeoutAtMillis = millis() + IOTSA_BLEDIMMER_CONNECT_TIMEOUT;
  }
}

void BLEDimmer::followDimmerChanges(bool follow) { 
  listenForDeviceChanges = follow; 
  if (listenForDeviceChanges) {
    needSyncFromDevice = listenForDeviceChanges;
    needTransmitTimeoutAtMillis = millis() + IOTSA_BLEDIMMER_CONNECT_TIMEOUT;
  }
}

void BLEDimmer::identify() {
  needIdentify = true;
  updateDimmer();
}
 
void BLEDimmer::loop() {
  // If we don't have anything to transmit we bail out quickly...
  if (!needSyncToDevice && !needSyncFromDevice) {
#ifdef IOTSA_BLEDIMMER_KEEPOPEN_MILLIS

    // But we first disconnect if we are connected-idle for long enough.
    if (disconnectAtMillis > 0 && millis() > disconnectAtMillis) {
      disconnectAtMillis = 0;
      IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(name);
      if (dimmer) {
        IFDEBUG IotsaSerial.printf("BLEDimmer: delayed disconnect %s\n", dimmer->getName().c_str());
        dimmer->disconnect();
      }
    }
  #endif
    return;
  }
  // We have something to transmit/receive. Check whether our dimmer actually exists.
  IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(name);
  if (dimmer == NULL) {
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
      return;
    }
    // iotsaBLEClient should be listening for advertisements
    return;
  }
  // If we are scanning we don't try to connect
  if (!bleClientMod.canConnect()) {
    //xxxjack IotsaSerial.println("BLEDimmer: BLE busy, cannot connect");
    if (millis() > noWarningPrintBefore) {
      IotsaSerial.printf("BLEDimmer: BLE busy, cannot connect to %s\n", name.c_str());
      noWarningPrintBefore = millis() + 4000;
    }
    return;
  }
  noWarningPrintBefore = 0;
  // If all that is correct, try to connect.
  if (!dimmer->connect()) {
    IotsaSerial.printf("BLEDimmer: connect to %s failed\n", dimmer->getName().c_str());
    bleClientMod.deviceNotConnectable(name);
    return;
  }
  IFDEBUG IotsaSerial.printf("BLEDimmer: connected to %s\n", dimmer->getName().c_str());
  
  if (needSyncFromDevice) {
    syncFromDevice(dimmer);
  }
  if (needSyncToDevice) {
    syncToDevice(dimmer);
#ifdef IOTSA_BLEDIMMER_KEEPOPEN_MILLIS
    disconnectAtMillis = millis() + IOTSA_BLEDIMMER_KEEPOPEN_MILLIS;
    iotsaConfig.postponeSleep(IOTSA_BLEDIMMER_KEEPOPEN_MILLIS+100);
    IFDEBUG IotsaSerial.println("BLEDimmer: keepopen");
    return;
#endif
  }

  IFDEBUG IotsaSerial.printf("BLEDimmer: disconnecting %s", dimmer->getName().c_str());
  dimmer->disconnect();
}

void BLEDimmer::syncToDevice(IotsaBLEClientConnection *dimmer) {
  bool ok;
#ifdef DIMMER_WITH_LEVEL
  // Connected to dimmer.
  Lissabon::Dimmer::Type_brightness levelValue = level * ((1<<sizeof(Lissabon::Dimmer::Type_brightness)*8)-1);
  IFDEBUG IotsaSerial.printf("BLEDimmer: Transmit brightness %d\n", levelValue);
  ok = dimmer->set(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::brightnessUUID, (Lissabon::Dimmer::Type_brightness)levelValue);
  if (!ok) {
    IFDEBUG IotsaSerial.println("BLEDimmer: set(brightness) failed");
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
  IFDEBUG IotsaSerial.printf("BLEDimmer: Transmit ison %d\n", (int)isOn);
  ok = dimmer->set(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::isOnUUID, (Lissabon::Dimmer::Type_isOn)isOn);
  if (!ok) {
    IFDEBUG IotsaSerial.println("BLEDimmer: set(isOn) failed");
  }
  if (needIdentify) {
    IFDEBUG IotsaSerial.printf("BLEDimmer: Transmit identify\n");
    ok = dimmer->set(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::identifyUUID, (Lissabon::Dimmer::Type_isOn)1);
    needIdentify = false;
    if (!ok) {
      IFDEBUG IotsaSerial.println("BLEDimmer: identify failed");
    }

  }
  needSyncToDevice = false;
}

void BLEDimmer::syncFromDevice(IotsaBLEClientConnection *dimmer) {
  bool ok;
#ifdef DIMMER_WITH_LEVEL
  // Connected to dimmer.
  Lissabon::Dimmer::Type_brightness levelValue;
  ok = dimmer->get(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::brightnessUUID, levelValue);
  if (ok) {
    IFDEBUG IotsaSerial.printf("BLEDimmer: Received brightness %d\n", levelValue);
    level = (float)levelValue / (float)((1<<sizeof(Lissabon::Dimmer::Type_brightness)*8)-1);
  } else {
    IFDEBUG IotsaSerial.println("BLEDimmer: get(brightness) failed");
  }
#endif
#ifdef DIMMER_WITH_TEMPERATURE
  Lissabon::Dimmer::Type_temperature temperatureValue;
  ok = dimmer->get(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::temperatureUUID, temperatureValue);
  if (ok) {
    IFDEBUG IotsaSerial.printf("BLEDimmer: Received temperature %d\n", temperatureValue);
    temperature = (float)temperatureValue;
  } else {
    IFDEBUG IotsaSerial.println("BLEDimmer: get(temperature) failed");
  }
#endif // DIMMER_WITH_TEMPERATURE
  uint8_t isOnValue;
  ok = dimmer->get(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::isOnUUID, isOnValue);
  if (ok) {
    IFDEBUG IotsaSerial.printf("BLEDimmer received isOn %d\n", isOnValue);
    isOn = isOnValue;
  } else {
    IFDEBUG IotsaSerial.println("BLEDimmer: set(isOn) failed");
  }
  needSyncFromDevice = false;
}

}