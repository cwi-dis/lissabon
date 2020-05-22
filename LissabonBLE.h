#ifndef _LISSABONBLE_H_
#define _LISSABONBLE_H_

#include <BLEDevice.h>

namespace Lissabon {
namespace Dimmer {
// UUID of service advertised by iotsaLedstrip and iotsaDimmer devices
extern BLEUUID serviceUUID;
extern BLEUUID isOnUUID;
extern BLEUUID identifyUUID;
extern BLEUUID brightnessUUID;
extern BLEUUID intervalUUID;
};
};
#endif // _LISSABONBLE_H_