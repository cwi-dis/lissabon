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
  void followDimmerChanges(bool follow);
  void updateDimmer();
  bool available();
  bool dataValid() override { return _dataValid; }
  bool setName(String value);
  void setup() override;
  void loop() override;
  void identify() override;
  virtual bool configLoad(IotsaConfigFileLoad& cf, const String& name) override;
  virtual void configSave(IotsaConfigFileSave& cf, const String& name) override;
  virtual void getHandler(JsonObject& reply) override;
  virtual void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) override;
protected:
  void syncToDevice(IotsaBLEClientConnection *dimmer);
  void syncFromDevice(IotsaBLEClientConnection *dimmer);
  IotsaBLEClientMod& bleClientMod;
  bool listenForDeviceChanges = false;
  bool needSyncToDevice = false;
  bool needSyncFromDevice = false;
  bool _dataValid = false;
  bool needIdentify = false;
  uint32_t needTransmitTimeoutAtMillis = 0;
  uint32_t disconnectAtMillis = 0;
  uint32_t noWarningPrintBefore = 0;
};
};
#endif // _BLEDIMMER_H_