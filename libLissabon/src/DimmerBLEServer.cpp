#include "DimmerBLEServer.h"
#ifdef IOTSA_WITH_BLE

namespace Lissabon {

void DimmerBLEServer::setup() {
  bleApi.setup(Lissabon::serviceUUIDstring, this);

  bleApi.addCharacteristic(
    Lissabon::isOnUUIDstring,
    bleApi.BLE_READ|bleApi.BLE_WRITE,
    Lissabon::isOnUUID2904format, 
    Lissabon::isOnUUID2904unit, 
    Lissabon::isOnUUID2901
    );
#ifdef DIMMER_WITH_LEVEL
  bleApi.addCharacteristic(
    Lissabon::brightnessUUIDstring, 
    bleApi.BLE_READ|bleApi.BLE_WRITE,
    Lissabon::brightnessUUID2904format,
    Lissabon::brightnessUUID2904unit,
    Lissabon::brightnessUUID2901
    );
#endif
#ifdef DIMMER_WITH_TEMPERATURE
  bleApi.addCharacteristic(
    Lissabon::temperatureUUIDstring, 
    bleApi.BLE_READ|bleApi.BLE_WRITE,
    Lissabon::temperatureUUID2904format,
    Lissabon::temperatureUUID2904unit,
    Lissabon::temperatureUUID2901
    );
#endif
}

bool DimmerBLEServer::blePutHandler(UUIDstring charUUID) {
  bool anyChanged = false;
#ifdef DIMMER_WITH_LEVEL
  if (charUUID == Lissabon::brightnessUUIDstring) {
    int i_level = bleApi.getAsInt(Lissabon::brightnessUUIDstring);
    float maxLevel = (float)(1<<sizeof(Lissabon::Type_brightness)*8)-1; // Depends on max #bits in brightness type
    float level = float(i_level)/maxLevel;
    if (level < dimmer.minLevel) level = dimmer.minLevel;
    if (level > 1) level = 1;
    dimmer.level = level;
    IOTSA_LOG_DEBUG("DimmerBLEServer", "wrote brightness %s value %d %f", Lissabon::brightnessUUIDstring, i_level, dimmer.level);
    anyChanged = true;
  }
#endif
#ifdef DIMMER_WITH_TEMPERATURE
  if (charUUID == Lissabon::temperatureUUIDstring) {
    int temperature = bleApi.getAsInt(Lissabon::temperatureUUIDstring);
    dimmer.temperature = temperature;
    IOTSA_LOG_DEBUG("DimmerBLEServer", "wrote temperature %s value %d", Lissabon::temperatureUUIDstring, dimmer.temperature);
    anyChanged = true;
  }
#endif
  if (charUUID == Lissabon::isOnUUIDstring) {
    int value = bleApi.getAsInt(Lissabon::isOnUUIDstring);
    dimmer.isOn = (bool)value;
    IOTSA_LOG_DEBUG("DimmerBLEServer", "wrote isOn %s value %d", Lissabon::isOnUUIDstring, dimmer.isOn);
    if (!value && auxDimmer != nullptr) {
      auxDimmer->isOn = false;
      auxDimmer->updateDimmer();
      IOTSA_LOG_DEBUG("DimmerBLEServer", "also turned off auxdimmer");
    } 
    anyChanged = true;
  }
  if (anyChanged) {
    dimmer.updateDimmer();
    return true;
  }
  IOTSA_LOG("DimmerBLEServer", "write unknown uuid %s", charUUID);
  return false;
}

bool DimmerBLEServer::bleGetHandler(UUIDstring charUUID) {
#ifdef DIMMER_WITH_LEVEL
  if (charUUID == Lissabon::brightnessUUIDstring) {
      unsigned int maxLevel = (1<<sizeof(Lissabon::Type_brightness)*8)-1; // Depends on max #bits in brightness. Assume <= sizeof(int)
      unsigned int level = dimmer.level*maxLevel;
      IOTSA_LOG_DEBUG("DimmerBLEServer", "read level %s value %d", Lissabon::brightnessUUIDstring, level);
      bleApi.set(Lissabon::brightnessUUIDstring, (Lissabon::Type_brightness)level);
      return true;
  }
#endif
#ifdef DIMMER_WITH_TEMPERATURE
  if (charUUID == Lissabon::temperatureUUIDstring) {
      int temperature = dimmer.temperature;
      IOTSA_LOG_DEBUG("DimmerBLEServer", "read temperature %s value %d", Lissabon::isOnUUIDstring, temperature);
      bleApi.set(Lissabon::temperatureUUIDstring, (Lissabon::Type_temperature)temperature);
      return true;
  }
#endif // DIMMER_WITH_TEMPERATURE
  if (charUUID == Lissabon::isOnUUIDstring) {
      IOTSA_LOG_DEBUG("DimmerBLEServer", "read isOn %s value %d", Lissabon::isOnUUIDstring, dimmer.isOn);
      bleApi.set(Lissabon::isOnUUIDstring, (Lissabon::Type_isOn)dimmer.isOn);
      return true;
  }
  IOTSA_LOG("DimmerBLEServer", "read unknown uuid %s", charUUID);
  return false;
}
}
#endif // IOTSA_WITH_BLE