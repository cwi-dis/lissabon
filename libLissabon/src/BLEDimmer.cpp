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
  needTransmit = true;
  needTransmitTimeoutAtMillis = millis() + IOTSA_BLEDIMMER_CONNECT_TIMEOUT;
  callbacks->dimmerValueChanged();
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
  message += "BLE name: <input name='" + f_name +".name' value='" + name + "'><br>";
  IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(name);
  if (dimmer) message += "BLE public address: " + String(dimmer->getAddress().c_str()) + "<br>";
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
}

void BLEDimmer::loop() {
  // If we don't have anything to transmit we bail out quickly...
  if (!needTransmit) {
#ifdef IOTSA_BLEDIMMER_KEEPOPEN_MILLIS

    // But we first disconnect if we are connected-idle for long enough.
    if (disconnectAtMillis > 0 && millis() > disconnectAtMillis) {
      disconnectAtMillis = 0;
      IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(name);
      if (dimmer) {
        IFDEBUG IotsaSerial.println("xxxjack disconnecting");
        dimmer->disconnect();
      }
    }
  #endif
    return;
  }
  // We have something to transmit. Check whether our dimmer actually exists.
  IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(name);
  if (dimmer == NULL) {
    IFDEBUG IotsaSerial.printf("Skip xmit to nonexistent dimmer %d %s\n", num, name.c_str());
    needTransmit = false;
    return;
  }
  // If it exists, check that we have enough information to connect.
  if (!dimmer->available()) {
    //IotsaSerial.println("xxxjack dimmer not available");
    if (millis() > needTransmitTimeoutAtMillis) {
      IotsaSerial.println("Giving up on connecting to dimmer");
      needTransmit = false;
      return;
    }
    // iotsaBLEClient should be listening for advertisements
    return;
  }
  // If we are scanning we don't try to connect
  if (!bleClientMod.canConnect()) return;
  // If all that is correct, try to connect.
  if (!dimmer->connect()) {
    IotsaSerial.println("connect to dimmer failed");
    bleClientMod.deviceNotSeen(name);
    return;
  }
  bool ok;
#ifdef DIMMER_WITH_LEVEL
  // Connected to dimmer.
  int levelValue = level * ((1<<sizeof(Lissabon::Dimmer::Type_brightness)*8)-1);
  IFDEBUG IotsaSerial.printf("xxxjack Transmit brightness %d\n", levelValue);
  ok = dimmer->set(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::brightnessUUID, (Lissabon::Dimmer::Type_brightness)levelValue);
  if (!ok) {
    IFDEBUG IotsaSerial.println("BLE: set(brightness) failed");
  }
#endif
#ifdef DIMMER_WITH_TEMPERATURE
  int temperatureValue = temperature;
  IFDEBUG IotsaSerial.printf("xxxjack Transmit temperature %d\n", temperatureValue);
  ok = dimmer->set(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::temperatureUUID, (Lissabon::Dimmer::Type_temperature)temperatureValue);
  if (!ok) {
    IFDEBUG IotsaSerial.println("BLE: set(brightness) failed");
  }
#endif // DIMMER_WITH_TEMPERATURE
  IFDEBUG IotsaSerial.printf("xxxjack Transmit ison %d\n", (int)isOn);
  ok = dimmer->set(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::isOnUUID, (Lissabon::Dimmer::Type_isOn)isOn);
  if (!ok) {
    IFDEBUG IotsaSerial.println("BLE: set(isOn) failed");
  }
#ifdef IOTSA_BLEDIMMER_KEEPOPEN_MILLIS
  disconnectAtMillis = millis() + IOTSA_BLEDIMMER_KEEPOPEN_MILLIS;
  iotsaConfig.postponeSleep(IOTSA_BLEDIMMER_KEEPOPEN_MILLIS+100);
#else
  IFDEBUG IotsaSerial.println("xxxjack disconnecting");
  dimmer->disconnect();
#endif
  needTransmit = false;
}
}