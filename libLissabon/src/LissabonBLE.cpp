#ifdef IOTSA_WITH_BLE
#include "LissabonBLE.h"

namespace Lissabon {
namespace Dimmer {
// UUID of service advertised by iotsaLedstrip and iotsaDimmer devices
const char* serviceUUIDstring = "6B2F0001-38BC-4204-A506-1D3546AD3688";
BLEUUID serviceUUID(serviceUUIDstring);
//const char* serviceUUID2901 = "Lissabon Dimmer Service";
//const uint8_t serviceUUID2904format = ;
//const uint16_t serviceUUID2904unit = ;

const char* isOnUUIDstring = "6B2F0002-38BC-4204-A506-1D3546AD3688";
BLEUUID isOnUUID(isOnUUIDstring);
const char* isOnUUID2901 = "On/Off";
const uint8_t isOnUUID2904format = BLE2904::FORMAT_UINT8;
const uint16_t isOnUUID2904unit = 0x2700;

const char* identifyUUIDstring = "6B2F0003-38BC-4204-A506-1D3546AD3688";
BLEUUID identifyUUID(identifyUUIDstring);
const char* identifyUUID2901 = "Request device identify";
const uint8_t identifyUUID2904format = BLE2904::FORMAT_UINT8;
const uint16_t identifyUUID2904unit = 0x2700;

const char* brightnessUUIDstring = "6B2F0004-38BC-4204-A506-1D3546AD3688";
BLEUUID brightnessUUID(brightnessUUIDstring);
const char* brightnessUUID2901 = "Brightness";
const uint8_t brightnessUUID2904format = BLE2904::FORMAT_UINT16;
const uint16_t brightnessUUID2904unit = 0x27ad;

const char* temperatureUUIDstring = "6B2F0005-38BC-4204-A506-1D3546AD3688";
BLEUUID temperatureUUID(temperatureUUIDstring);
const char* temperatureUUID2901 = "Color Temperature";
const uint8_t temperatureUUID2904format = BLE2904::FORMAT_UINT16;
const uint16_t temperatureUUID2904unit = 0x2700;

};
};
#endif // IOTSA_WITH_BLE