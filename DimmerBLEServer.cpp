#include "DimmerBLEServer.h"


void DimmerBLEServer::setup() {
#ifdef IOTSA_WITH_BLE
  // Set default advertising interval to be between 200ms and 600ms
  IotsaBLEServerMod::setAdvertisingInterval(300, 900);

  bleApi.setup(serviceUUID, this);
  static BLE2904 isOn2904;
  isOn2904.setFormat(BLE2904::FORMAT_UINT8);
  isOn2904.setUnit(0x2700);
  static BLE2901 isOn2901("On/Off");
  bleApi.addCharacteristic(isOnUUID, BLE_READ|BLE_WRITE, &isOn2904, &isOn2901);

  static BLE2904 illum2904;
  illum2904.setFormat(BLE2904::FORMAT_UINT8);
  illum2904.setUnit(0x27AD);
  static BLE2901 illum2901("Illumination");
  bleApi.addCharacteristic(illumUUID, BLE_READ|BLE_WRITE, &illum2904, &illum2901);

  static BLE2904 identify2904;
  identify2904.setFormat(BLE2904::FORMAT_UINT8);
  identify2904.setUnit(0x2700);
  static BLE2901 identify2901("Identify");
  bleApi.addCharacteristic(identifyUUID, BLE_WRITE, &identify2904, &identify2901);
#endif
}

bool DimmerBLEServer::blePutHandler(UUIDstring charUUID) {
#ifdef IOTSA_WITH_BLE
  bool anyChanged = false;
  if (charUUID == illumUUID) {
      int level = bleApi.getAsInt(illumUUID);
      dimmer.level = float(level)/100.0;
      IFDEBUG IotsaSerial.printf("xxxjack ble: wrote illum %s value %d %f\n", illumUUID, level, dimmer.level);
      anyChanged = true;
  }
  if (charUUID == isOnUUID) {
    int value = bleApi.getAsInt(isOnUUID);
    dimmer.isOn = (bool)value;
    anyChanged = true;
  }
  if (charUUID == identifyUUID) {
    int value = bleApi.getAsInt(identifyUUID);
    if (value) dimmer.identify();
  }
  if (anyChanged) {
    configSave();
    dimmer.updateDimmer();
    return true;
  }
  IotsaSerial.println("IotsaDimmerMod: ble: write unknown uuid");
#endif
  return false;
}

bool DimmerBLEServer::bleGetHandler(UUIDstring charUUID) {
#ifdef IOTSA_WITH_BLE
  if (charUUID == illumUUID) {
      int level = int(dimmer.level*100);
      IFDEBUG IotsaSerial.printf("xxxjack ble: read level %s value %d\n", charUUID, level);
      bleApi.set(illumUUID, (uint8_t)level);
      return true;
  }
  if (charUUID == isOnUUID) {
      IFDEBUG IotsaSerial.printf("xxxjack ble: read isOn %s value %d\n", charUUID, isOn);
      bleApi.set(isOnUUID, (uint8_t)dimmer.isOn);
      return true;
  }
  IotsaSerial.println("IotsaDimmerMod: ble: read unknown uuid");
#endif
  return false;
}
