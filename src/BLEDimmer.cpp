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

void BLEDimmer::loop() {
  // If we don't have anything to transmit we bail out quickly...
  if (!needTransmit) {
    // But we first disconnect if we are connected-idle for long enough.
    if (disconnectAtMillis > 0 && millis() > disconnectAtMillis) {
      disconnectAtMillis = 0;
      IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(name);
      if (dimmer) {
        IFDEBUG IotsaSerial.println("xxxjack disconnecting");
        dimmer->disconnect();
        IFDEBUG IotsaSerial.println("xxxjack disconnected");
      }
    }
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
  // Connected to dimmer.
  IFDEBUG IotsaSerial.printf("xxxjack Transmit brightness %d\n", (uint8_t)(level*100));
  bool ok = dimmer->set(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::brightnessUUID, (uint8_t)(level*100));
  if (!ok) {
    IFDEBUG IotsaSerial.println("BLE: set(brightness) failed");
  }
#ifdef DIMMER_WITH_TEMPERATURE
  IFDEBUG IotsaSerial.printf("xxxjack Transmit temperature %d\n", (int16_t)(temperature));
  bool ok = dimmer->set(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::temperatureUUID, (int16_t)temperature);
  if (!ok) {
    IFDEBUG IotsaSerial.println("BLE: set(brightness) failed");
  }
#endif // DIMMER_WITH_TEMPERATURE
  IFDEBUG IotsaSerial.printf("xxxjack Transmit ison %d\n", (int)isOn);
  ok = dimmer->set(Lissabon::Dimmer::serviceUUID, Lissabon::Dimmer::isOnUUID, (uint8_t)isOn);
  if (!ok) {
    IFDEBUG IotsaSerial.println("BLE: set(isOn) failed");
  }
  disconnectAtMillis = millis() + IOTSA_BLEDIMMER_KEEPOPEN_MILLIS;
  iotsaConfig.postponeSleep(IOTSA_BLEDIMMER_KEEPOPEN_MILLIS+100);
  needTransmit = false;
}
}