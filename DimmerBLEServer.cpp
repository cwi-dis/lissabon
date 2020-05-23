#include "DimmerBLEServer.h"


#ifdef IOTSA_WITH_BLE
bool DimmerBLEServer::blePutHandler(UUIDstring charUUID) {
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
  return false;
}

bool DimmerBLEServer::bleGetHandler(UUIDstring charUUID) {
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
  return false;
}
#endif // IOTSA_WITH_BLE