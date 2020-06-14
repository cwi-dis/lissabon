#ifndef _ABSTRACTDIMMER_H_
#define _ABSTRACTDIMMER_H_

#include "iotsa.h"
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
  virtual void updateDimmer();
  void calcCurLevel();
  virtual bool available() = 0;
  String info();
  virtual bool getHandler(JsonObject& reply);
  virtual bool putHandler(const JsonVariant& request);
  virtual bool putConfigHandler(const JsonVariant& request);
  virtual bool handlerArgs(IotsaWebServer *server);
  virtual bool handlerConfigArgs(IotsaWebServer *server);
  virtual void configLoad(IotsaConfigFileLoad& cf);
  virtual void configSave(IotsaConfigFileSave& cf);
  virtual String handlerForm();
  virtual String handlerConfigForm();
  virtual bool setName(String value);
  String getUserVisibleName();
  bool hasName();
  virtual void identify();
protected:
  virtual void extendHandlerConfigForm(String& message);
  virtual void extendHandlerConfigArgs(IotsaWebServer *server);
public:
  int num;
  DimmerCallbacks *callbacks;
  bool isOn;        // if true we want to show level, if false we want to be off.
#ifdef DIMMER_WITH_LEVEL
  float level;      // Requested light level
  float overrideLevel = -1; // If > 0 this is the _actual_ level we want to output, because a LED curve will cause it to be perceived as level.
  float curLevel;   // actual current light level (depends on level/isOn but also animation progress)
  float minLevel;   // minimum light level through UI
#endif
#ifdef DIMMER_WITH_GAMMA
  float gamma;
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
  float prevLevel;  // actual light level at millisAnimtationStart
  int animationDurationMillis;  // Maximum duration of an animation (0-100% or reverse)
  uint32_t millisAnimationStart;  // Time current animation started
  uint32_t millisAnimationEnd;  // Time current animation should end

#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_TEMPERATURE
  float temperature;
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
  float pwmFrequency;
#endif // DIMMER_WITH_PWMFREQUENCY
protected:
  String name;
};
};
#endif // _ABSTRACTDIMMER_H_