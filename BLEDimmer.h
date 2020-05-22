#ifndef _BLEDIMMER_H_
#define _BLEDIMMER_H_
//
// LED Lighting control module. 
//
#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaBLEClient.h"
#include "AbstractDimmer.h"
#include "LissabonBLE.h"

#include <ArduinoJson.h>
using namespace ArduinoJson;

namespace Lissabon {

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