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
typedef uint8_t Type_isOn;

extern BLEUUID identifyUUID;
extern const char* identifyUUIDstring;
extern const char* identifyUUID2901;
extern const uint8_t identifyUUID2904format;
extern const uint16_t identifyUUID2904unit;
typedef uint8_t Type_identify;

extern BLEUUID brightnessUUID;
extern const char* brightnessUUIDstring;
extern const char* brightnessUUID2901;
extern const uint8_t brightnessUUID2904format;
extern const uint16_t brightnessUUID2904unit;
typedef uint16_t Type_brightness;

extern BLEUUID temperatureUUID;
extern const char* temperatureUUIDstring;
extern const char* temperatureUUID2901;
extern const uint8_t temperatureUUID2904format;
extern const uint16_t temperatureUUID2904unit;
typedef uint16_t Type_temperature;

};
};
#endif // _LISSABONBLE_H_