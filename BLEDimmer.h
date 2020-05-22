#ifndef _BLEDIMMER_H_
#define _BLEDIMMER_H_
//
// LED Lighting control module. 
//
#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaBLEClient.h"
#include "AbstractDimmer.h"

#include <ArduinoJson.h>
using namespace ArduinoJson;

namespace Lissabon {

// UUID of service advertised by iotsaLedstrip and iotsaDimmer devices
static BLEUUID serviceUUID("F3390001-F793-4D0C-91BB-C91EEB92A1A4");
static BLEUUID isOnUUID("F3390002-F793-4D0C-91BB-C91EEB92A1A4");
//static constexpr UUIDstring identifyUUID = "F3390003-F793-4D0C-91BB-C91EEB92A1A4";
static BLEUUID brightnessUUID("F3390004-F793-4D0C-91BB-C91EEB92A1A4");
//static constexpr UUIDstring tempUUID = "F3390005-F793-4D0C-91BB-C91EEB92A1A4";
//static constexpr UUIDstring intervalUUID = "F3390006-F793-4D0C-91BB-C91EEB92A1A4";


class BLEDimmer : public AbstractDimmer {
public:
  BLEDimmer(int _num, IotsaBLEClientMod &_bleClientMod, DimmerCallbacks *_callbacks)
  : AbstractDimmer(num, _callbacks), 
    bleClientMod(_bleClientMod)
  {}
  void updateDimmer();
  bool available();
  bool setName(String value);

  IotsaBLEClientMod& bleClientMod;
  bool needTransmit;
  uint32_t needTransmitTimeoutAtMillis;
  uint32_t disconnectAtMillis;
  void loop();
};
};
#endif // _BLEDIMMER_H_