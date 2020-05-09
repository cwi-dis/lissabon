#ifndef _BLEDIMMER_H_
#define _BLEDIMMER_H_
//
// LED Lighting control module. 
//
#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaBLEClient.h"

#include <ArduinoJson.h>

// UUID of service advertised by iotsaLedstrip and iotsaDimmer devices
static BLEUUID serviceUUID("F3390001-F793-4D0C-91BB-C91EEB92A1A4");
static BLEUUID isOnUUID("F3390002-F793-4D0C-91BB-C91EEB92A1A4");
//static constexpr UUIDstring identifyUUID = "F3390003-F793-4D0C-91BB-C91EEB92A1A4";
static BLEUUID brightnessUUID("F3390004-F793-4D0C-91BB-C91EEB92A1A4");
//static constexpr UUIDstring tempUUID = "F3390005-F793-4D0C-91BB-C91EEB92A1A4";
//static constexpr UUIDstring intervalUUID = "F3390006-F793-4D0C-91BB-C91EEB92A1A4";

using namespace ArduinoJson;

class DimmerCallbacks {
public:
  ~DimmerCallbacks() {}
  virtual void buttonChanged() = 0;
  virtual void needSave() = 0;
};

class BLEDimmer {
public:
  BLEDimmer(int _num, DimmerCallbacks *_callbacks) : num(_num), callbacks(_callbacks) {}
  void updateDimmer();
  bool setName(String value);
  bool available();
  String info();
  bool getHandler(JsonObject& reply);
  bool putHandler(const JsonVariant& request);
  bool handlerArgs(IotsaWebServer *server);
  bool handlerConfigArgs(IotsaWebServer *server);
  void configLoad(IotsaConfigFileLoad& cf);
  void configSave(IotsaConfigFileSave& cf);
  String handlerForm();
  String handlerConfigForm();

  int num;
  DimmerCallbacks *callbacks;
  bool isOn;
  float level;
  float minLevel;
  String name;
  bool needTransmit;
  uint32_t needTransmitTimeoutAtMillis;
  uint32_t disconnectAtMillis;
  void loop();
};
#endif // _BLEDIMMER_H_