#ifdef IOTSA_WITH_BLE
#include "LissabonBLE.h"

namespace Lissabon {
// UUID of service advertised by iotsaLedstrip and iotsaDimmer devices
const char* serviceUUIDstring = "6B2F0001-38BC-4204-A506-1D3546AD3688";
NimBLEUUID serviceUUID(serviceUUIDstring);
//const char* serviceUUID2901 = "Lissabon Dimmer Service";
//const uint8_t serviceUUID2904format = ;
//const uint16_t serviceUUID2904unit = ;

const char* isOnUUIDstring = "6B2F0002-38BC-4204-A506-1D3546AD3688";
NimBLEUUID isOnUUID(isOnUUIDstring);
const char* isOnUUID2901 = "On/Off";
const uint8_t isOnUUID2904format = NimBLE2904::FORMAT_UINT8;
const uint16_t isOnUUID2904unit = 0x2700;

// 6B2F0003 (was identifyUUID here) is superseded by the generic core runmode
// identify characteristic (IotsaRunmodeBLE::identifyUUID, iotsaBLE.h) --
// DimmerBLEClient/DimmerBLEServer no longer use it. Kept reachable as
// v2_identify in the Python CLI's bleIotsaUUIDs.py for already-deployed
// devices running older firmware that still exposes it.

const char* brightnessUUIDstring = "6B2F0004-38BC-4204-A506-1D3546AD3688";
NimBLEUUID brightnessUUID(brightnessUUIDstring);
const char* brightnessUUID2901 = "Brightness";
const uint8_t brightnessUUID2904format = NimBLE2904::FORMAT_UINT16;
const uint16_t brightnessUUID2904unit = 0x27ad;

const char* temperatureUUIDstring = "6B2F0005-38BC-4204-A506-1D3546AD3688";
NimBLEUUID temperatureUUID(temperatureUUIDstring);
const char* temperatureUUID2901 = "Color Temperature";
const uint8_t temperatureUUID2904format = NimBLE2904::FORMAT_UINT16;
const uint16_t temperatureUUID2904unit = 0x2700;

};
#endif // IOTSA_WITH_BLE