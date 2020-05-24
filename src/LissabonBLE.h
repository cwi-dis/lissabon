#ifndef _LISSABONBLE_H_
#define _LISSABONBLE_H_

#include <BLEDevice.h>

namespace Lissabon {
namespace Dimmer {
// UUID of service advertised by iotsaLedstrip and iotsaDimmer devices
extern BLEUUID serviceUUID;
extern const char* serviceUUIDstring;
//extern const char* serviceUUID2901;
//extern const uint8_t serviceUUID2904format;
//extern const uint16_t serviceUUID2904unit;

extern BLEUUID isOnUUID;
extern const char* isOnUUIDstring;
extern const char* isOnUUID2901;
extern const uint8_t isOnUUID2904format;
extern const uint16_t isOnUUID2904unit;

extern BLEUUID identifyUUID;
extern const char* identifyUUIDstring;
extern const char* identifyUUID2901;
extern const uint8_t identifyUUID2904format;
extern const uint16_t identifyUUID2904unit;

extern BLEUUID brightnessUUID;
extern const char* brightnessUUIDstring;
extern const char* brightnessUUID2901;
extern const uint8_t brightnessUUID2904format;
extern const uint16_t brightnessUUID2904unit;

extern BLEUUID tempUUID;
extern const char* tempUUIDstring;
extern const char* tempUUID2901;
extern const uint8_t tempUUID2904format;
extern const uint16_t tempUUID2904unit;

extern BLEUUID intervalUUID;
extern const char* intervalUUIDstring;
extern const char* UUID2901;
extern const uint8_t UUID2904format;
extern const uint16_t UUID2904unit;

};
};
#endif // _LISSABONBLE_H_