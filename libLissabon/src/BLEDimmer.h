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

namespace Lissabon {

class BLEDimmer : public AbstractDimmer {
public:
  BLEDimmer(int _num, IotsaBLEClientMod &_bleClientMod, DimmerCallbacks *_callbacks)
  : AbstractDimmer(_num, _callbacks), 
    bleClientMod(_bleClientMod)
  {}
  void updateDimmer();
  bool available();
  bool setName(String value);
  void setup();
  void loop();
  virtual bool configLoad(IotsaConfigFileLoad& cf, const String& name) override;
  virtual void configSave(IotsaConfigFileSave& cf, const String& name) override;
  virtual void getHandler(JsonObject& reply) override;
  virtual void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) override;
protected:

  IotsaBLEClientMod& bleClientMod;
  bool needTransmit;
  uint32_t needTransmitTimeoutAtMillis;
  uint32_t disconnectAtMillis;
};
};
#endif // _BLEDIMMER_H_