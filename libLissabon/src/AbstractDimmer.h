#ifndef _ABSTRACTDIMMER_H_
#define _ABSTRACTDIMMER_H_

#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaConfigFile.h"
#include <ArduinoJson.h>
using namespace ArduinoJson;

#ifndef DIMMER_WITHOUT_LEVEL
#define DIMMER_WITH_LEVEL
#endif

namespace Lissabon {

class DimmerCallbacks {
public:
  ~DimmerCallbacks() {}
  virtual void dimmerOnOffChanged() = 0;
  virtual void dimmerValueChanged() = 0;
  virtual void dimmerAvailableChanged(bool available, bool connected) = 0;
};

class AbstractDimmer : IotsaApiModObject {
public:
  AbstractDimmer(int _num, DimmerCallbacks* _callbacks)
  : num(_num),
    callbacks(_callbacks)
  {}
  virtual ~AbstractDimmer() {}
  virtual void updateDimmer();
  void calcCurLevel();
  virtual bool available() = 0;
  virtual bool dataValid() { return true; }
  String info();
  virtual void getHandler(JsonObject& reply) override;
  virtual bool putHandler(const JsonVariant& request) override;
  virtual bool configLoad(IotsaConfigFileLoad& cf, const String& name) override;
  virtual void configSave(IotsaConfigFileSave& cf, const String& name) override;
  virtual bool formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) override;
//  virtual bool formArgHandler_config(IotsaWebServer *server, const String& f_name);
  virtual void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) override;
  virtual void formHandler_TD(String& message, bool includeConfig) override /* unimplemented */;
  virtual bool setName(String value);
  String getUserVisibleName();
  bool hasName();
  virtual void identify();
  virtual void setup() {};
  virtual void loop() {};
public:
  int num;
  DimmerCallbacks *callbacks;
  bool isOn = 0;        // if true we want to show level, if false we want to be off.
#ifdef DIMMER_WITH_LEVEL
  float level = 0;      // Requested light level
  float curLevel = 0;   // actual current light level (depends level, isOn, gamma, animation progress)
  float minLevel = 0;   // minimum light level through UI
#endif
#ifdef DIMMER_WITH_GAMMA
  float gamma = 1;
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
  float animationPrevLevel = 0;  // actual light level at millisAnimtationStart
  float animationCurLevel = 0; // actual current light level
  int animationDurationMillis = 0;  // Maximum duration of an animation (0-100% or reverse)
  uint32_t animationStartMillis = 0;  // Time current animation started
  uint32_t animationEndMillis = 0;  // Time current animation should end

#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_TEMPERATURE
  float temperature = 4000;
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
  float pwmFrequency = 50;
#endif // DIMMER_WITH_PWMFREQUENCY
protected:
  String name;
};
};
#endif // _ABSTRACTDIMMER_H_