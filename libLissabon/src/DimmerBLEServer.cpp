#include "DimmerBLEServer.h"
#ifdef IOTSA_WITH_BLE

namespace Lissabon {

void DimmerBLEServer::setup() {
#ifdef IOTSA_WITH_BLE
  // Set default advertising interval to be between 200ms and 600ms
  
  bleApi.setup(Lissabon::Dimmer::serviceUUIDstring, this);

  bleApi.addCharacteristic(
    Lissabon::Dimmer::isOnUUIDstring, 
    BLE_READ|BLE_WRITE, 
    Lissabon::Dimmer::isOnUUID2904format, 
    Lissabon::Dimmer::isOnUUID2904unit, 
    Lissabon::Dimmer::isOnUUID2901
    );

  bleApi.addCharacteristic(
    Lissabon::Dimmer::brightnessUUIDstring, 
    BLE_READ|BLE_WRITE, 
    Lissabon::Dimmer::brightnessUUID2904format,
    Lissabon::Dimmer::brightnessUUID2904unit,
    Lissabon::Dimmer::brightnessUUID2901
    );
#ifdef DIMMER_WITH_TEMPERATURE
  bleApi.addCharacteristic(
    Lissabon::Dimmer::temperatureUUIDstring, 
    BLE_READ|BLE_WRITE, 
    Lissabon::Dimmer::temperatureUUID2904format,
    Lissabon::Dimmer::temperatureUUID2904unit,
    Lissabon::Dimmer::temperatureUUID2901
    );
#endif
  bleApi.addCharacteristic(
    Lissabon::Dimmer::identifyUUIDstring, 
    BLE_WRITE, 
    Lissabon::Dimmer::identifyUUID2904format,
    Lissabon::Dimmer::identifyUUID2904unit,
    Lissabon::Dimmer::identifyUUID2901
    );
#endif
}

bool DimmerBLEServer::blePutHandler(UUIDstring charUUID) {
  bool anyChanged = false;
  if (charUUID == Lissabon::Dimmer::brightnessUUIDstring) {
    int i_level = bleApi.getAsInt(Lissabon::Dimmer::brightnessUUIDstring);
    float maxLevel = (float)(1<<sizeof(Lissabon::Dimmer::Type_brightness)*8)-1; // Depends on max #bits in brightness type
    float level = float(i_level)/maxLevel;
    if (level < dimmer.minLevel) level = dimmer.minLevel;
    if (level > 1) level = 1;
    dimmer.level = level;
    IFDEBUG IotsaSerial.printf("xxxjack ble: wrote brightness %s value %d %f\n", Lissabon::Dimmer::brightnessUUIDstring, i_level, dimmer.level);
    anyChanged = true;
  }
#ifdef DIMMER_WITH_TEMPERATURE
  if (charUUID == Lissabon::Dimmer::temperatureUUIDstring) {
    int temperature = bleApi.getAsInt(Lissabon::Dimmer::temperatureUUIDstring);
    dimmer.temperature = temperature;
    IFDEBUG IotsaSerial.printf("xxxjack ble: wrote temperature %s value %d\n", Lissabon::Dimmer::temperatureUUIDstring, dimmer.temperature);
    anyChanged = true;
  }
#endif
  if (charUUID == Lissabon::Dimmer::isOnUUIDstring) {
    int value = bleApi.getAsInt(Lissabon::Dimmer::isOnUUIDstring);
    dimmer.isOn = (bool)value;
    IFDEBUG IotsaSerial.printf("xxxjack ble: wrote isOn %s value %d\n", Lissabon::Dimmer::isOnUUIDstring, dimmer.isOn);
    if (!value && auxDimmer != nullptr) {
      auxDimmer->isOn = false;
      auxDimmer->updateDimmer();
      IFDEBUG IotsaSerial.printf("xxxjack ble: also turned off auxdimmer\n");
    } 
    anyChanged = true;
  }
  if (charUUID == Lissabon::Dimmer::identifyUUIDstring) {
    int value = bleApi.getAsInt(Lissabon::Dimmer::identifyUUIDstring);
    if (value) dimmer.identify();
    IFDEBUG IotsaSerial.printf("xxxjack ble: identify %s value %d\n", Lissabon::Dimmer::identifyUUIDstring, value);
    return true;
  }
  if (anyChanged) {
    dimmer.updateDimmer();
    return true;
  }
  IotsaSerial.printf("IotsaDimmerMod: ble: write unknown uuid %s\n", charUUID);
  return false;
}

bool DimmerBLEServer::bleGetHandler(UUIDstring charUUID) {
  if (charUUID == Lissabon::Dimmer::brightnessUUIDstring) {
      unsigned int maxLevel = (1<<sizeof(Lissabon::Dimmer::Type_brightness)*8)-1; // Depends on max #bits in brightness. Assume <= sizeof(int)
      unsigned int level = dimmer.level*maxLevel;
      IFDEBUG IotsaSerial.printf("xxxjack ble: read level %s value %d\n", Lissabon::Dimmer::brightnessUUIDstring, level);
      bleApi.set(Lissabon::Dimmer::brightnessUUIDstring, (Lissabon::Dimmer::Type_brightness)level);
      return true;
  }
#ifdef DIMMER_WITH_TEMPERATURE
  if (charUUID == Lissabon::Dimmer::temperatureUUIDstring) {
      int temperature = dimmer.temperature;
      IFDEBUG IotsaSerial.printf("xxxjack ble: read temperature %s value %d\n", Lissabon::Dimmer::isOnUUIDstring, temperature);
      bleApi.set(Lissabon::Dimmer::temperatureUUIDstring, (Lissabon::Dimmer::Type_temperature)temperature);
      return true;
  }
#endif // DIMMER_WITH_TEMPERATURE
  if (charUUID == Lissabon::Dimmer::isOnUUIDstring) {
      IFDEBUG IotsaSerial.printf("xxxjack ble: read isOn %s value %d\n", Lissabon::Dimmer::isOnUUIDstring, dimmer.isOn);
      bleApi.set(Lissabon::Dimmer::isOnUUIDstring, (Lissabon::Dimmer::Type_isOn)dimmer.isOn);
      return true;
  }
  IotsaSerial.printf("IotsaDimmerMod: ble: read unknown uuid %s\n", charUUID);
  return false;
}
}
#endif // IOTSA_WITH_BLE