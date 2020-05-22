#ifndef _ABSTRACTDIMMER_H_
#define _ABSTRACTDIMMER_H_

#include "iotsa.h"
#include "iotsaConfigFile.h"
#include <ArduinoJson.h>
using namespace ArduinoJson;

namespace Lissabon {

class DimmerCallbacks {
public:
  ~DimmerCallbacks() {}
  virtual void uiButtonChanged() = 0;
  virtual void dimmerValueChanged() = 0;
};

class AbstractDimmer {
public:
  AbstractDimmer(int _num, DimmerCallbacks* _callbacks)
  : num(_num),
    callbacks(_callbacks)
  {}
  virtual ~AbstractDimmer() {}
  virtual void updateDimmer() = 0;
  virtual bool available() = 0;
  String info();
  bool getHandler(JsonObject& reply);
  bool putHandler(const JsonVariant& request);
  bool handlerArgs(IotsaWebServer *server);
  bool handlerConfigArgs(IotsaWebServer *server);
  void configLoad(IotsaConfigFileLoad& cf);
  void configSave(IotsaConfigFileSave& cf);
  String handlerForm();
  String handlerConfigForm();
  virtual bool setName(String value);
  String getUserVisibleName();
  bool hasName();
public:
  int num;
  DimmerCallbacks *callbacks;
  bool isOn;
  float level;
  float minLevel;
protected:
  String name;
};
};
#endif // _ABSTRACTDIMMER_H_