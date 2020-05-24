#include "LissabonBLE.h"
#include <BLE2904.h>

namespace Lissabon {
namespace Dimmer {
// UUID of service advertised by iotsaLedstrip and iotsaDimmer devices
const char* serviceUUIDstring = "F3390001-F793-4D0C-91BB-C91EEB92A1A4";
BLEUUID serviceUUID(serviceUUIDstring);
//const char* serviceUUID2901 = "Lissabon Dimmer Service";
//const uint8_t serviceUUID2904format = ;
//const uint16_t serviceUUID2904unit = ;

const char* isOnUUIDstring = "F3390002-F793-4D0C-91BB-C91EEB92A1A4";
BLEUUID isOnUUID(isOnUUIDstring);
const char* isOnUUID2901 = "On/Off";
const uint8_t isOnUUID2904format = BLE2904::FORMAT_UINT8;
const uint16_t isOnUUID2904unit = 0x2700;

const char* identifyUUIDstring = "F3390003-F793-4D0C-91BB-C91EEB92A1A4";
BLEUUID identifyUUID(identifyUUIDstring);
const char* identifyUUID2901 = "Request device identify";
const uint8_t identifyUUID2904format = BLE2904::FORMAT_UINT8;
const uint16_t identifyUUID2904unit = 0x2700;

const char* brightnessUUIDstring = "F3390004-F793-4D0C-91BB-C91EEB92A1A4";
BLEUUID brightnessUUID(brightnessUUIDstring);
const char* brightnessUUID2901 = "Brightness";
const uint8_t brightnessUUID2904format = BLE2904::FORMAT_UINT8;
const uint16_t brightnessUUID2904unit = 0x27ad;

const char* tempUUIDstring = "F3390005-F793-4D0C-91BB-C91EEB92A1A4";
BLEUUID tempUUID(tempUUIDstring);
const char* tempUUID2901 = "Color Temperature";
const uint8_t tempUUID2904format = BLE2904::FORMAT_UINT16;
const uint16_t tempUUID2904unit = 0x2700;

const char* intervalUUIDstring = "F3390006-F793-4D0C-91BB-C91EEB92A1A4";
BLEUUID intervalUUID(intervalUUIDstring);
const char* intervalUUID2901 = "Lit pixel interval";
const uint8_t intervalUUID2904format = BLE2904::FORMAT_UINT8;
const uint16_t intervalUUID2904unit = 0x2700;

};
};