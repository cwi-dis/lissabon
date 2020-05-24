#include "DimmerBLEServer.h"

namespace Lissabon {

void DimmerBLEServer::setup() {
#ifdef IOTSA_WITH_BLE
  // Set default advertising interval to be between 200ms and 600ms
  IotsaBLEServerMod::setAdvertisingInterval(300, 900);

  bleApi.setup(Lissabon::Dimmer::serviceUUIDstring, this);

  static BLE2904 isOn2904;
  isOn2904.setFormat(Lissabon::Dimmer::isOnUUID2904format);
  isOn2904.setUnit(Lissabon::Dimmer::isOnUUID2904unit);
  static BLE2901 isOn2901(Lissabon::Dimmer::isOnUUID2901);
  bleApi.addCharacteristic(Lissabon::Dimmer::isOnUUIDstring, BLE_READ|BLE_WRITE, &isOn2904, &isOn2901);

  static BLE2904 brightness2904;
  brightness2904.setFormat(Lissabon::Dimmer::brightnessUUID2904format);
  brightness2904.setUnit(Lissabon::Dimmer::brightnessUUID2904unit);
  static BLE2901 brightness2901(Lissabon::Dimmer::brightnessUUID2901);
  bleApi.addCharacteristic(Lissabon::Dimmer::brightnessUUIDstring, BLE_READ|BLE_WRITE, &brightness2904, &brightness2901);

  static BLE2904 identify2904;
  identify2904.setFormat(Lissabon::Dimmer::identifyUUID2904format);
  identify2904.setUnit(Lissabon::Dimmer::identifyUUID2904unit);
  static BLE2901 identify2901(Lissabon::Dimmer::identifyUUID2901);
  bleApi.addCharacteristic(Lissabon::Dimmer::identifyUUIDstring, BLE_WRITE, &identify2904, &identify2901);
#endif
}

bool DimmerBLEServer::blePutHandler(UUIDstring charUUID) {
#ifdef IOTSA_WITH_BLE
  bool anyChanged = false;
  if (charUUID == Lissabon::Dimmer::brightnessUUIDstring) {
      int level = bleApi.getAsInt(Lissabon::Dimmer::brightnessUUIDstring);
      dimmer.level = float(level)/100.0;
      IFDEBUG IotsaSerial.printf("xxxjack ble: wrote brightness %s value %d %f\n", Lissabon::Dimmer::brightnessUUIDstring, level, dimmer.level);
      anyChanged = true;
  }
  if (charUUID == Lissabon::Dimmer::isOnUUIDstring) {
    int value = bleApi.getAsInt(Lissabon::Dimmer::isOnUUIDstring);
    dimmer.isOn = (bool)value;
    anyChanged = true;
  }
  if (charUUID == Lissabon::Dimmer::identifyUUIDstring) {
    int value = bleApi.getAsInt(Lissabon::Dimmer::identifyUUIDstring);
    if (value) dimmer.identify();
  }
  if (anyChanged) {
    dimmer.updateDimmer();
    return true;
  }
  IotsaSerial.println("IotsaDimmerMod: ble: write unknown uuid");
#endif
  return false;
}

bool DimmerBLEServer::bleGetHandler(UUIDstring charUUID) {
#ifdef IOTSA_WITH_BLE
  if (charUUID == Lissabon::Dimmer::brightnessUUIDstring) {
      int level = int(dimmer.level*100);
      IFDEBUG IotsaSerial.printf("xxxjack ble: read level %s value %d\n", Lissabon::Dimmer::brightnessUUIDstring, level);
      bleApi.set(Lissabon::Dimmer::brightnessUUIDstring, (uint8_t)level);
      return true;
  }
  if (charUUID == Lissabon::Dimmer::isOnUUIDstring) {
      IFDEBUG IotsaSerial.printf("xxxjack ble: read isOn %s value %d\n", Lissabon::Dimmer::isOnUUIDstring, dimmer.isOn);
      bleApi.set(Lissabon::Dimmer::isOnUUIDstring, (uint8_t)dimmer.isOn);
      return true;
  }
  IotsaSerial.println("IotsaDimmerMod: ble: read unknown uuid");
#endif
  return false;
}
}