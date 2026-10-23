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

#define DIMMER_MIN_TEMPERATURE 1500.0
#define DIMMER_MAX_TEMPERATURE 6500.0

namespace Lissabon {

class DimmerCallbacks {
public:
  ~DimmerCallbacks() {}
  virtual void dimmerOnOffChanged() = 0;
  virtual void dimmerValueChanged() = 0;
  virtual void dimmerAvailableChanged() = 0;
};

//
// A dimmer has values (isOn, level, temperature: the everyday part of its
// interface) and settings (name, minLevel, gamma, animation, pwmFrequency).
// The settings are always saved. The values are saved by a light, which must
// come back as it was after a reboot, but not by a dimmer that only mirrors
// another device (DimmerBLEClient): its values come from that device
// (cwi-dis/iotsa#183 downstream sweep). The subclass decides, with
// valuePersistence.
//
class AbstractDimmer : public IotsaApiModObject {
public:
  AbstractDimmer(int _num, DimmerCallbacks* _callbacks)
  : num(_num),
    callbacks(_callbacks)
  {}
  virtual ~AbstractDimmer() {}
  virtual void updateDimmer();
  void calcCurLevel();
  virtual bool available() = 0;
  // Whether the values are known to be the device's own: always for a light,
  // only after a read (or write) for a DimmerBLEClient.
  virtual bool dataValid() { return true; }
  virtual void refresh() {}
  String info();
  void getHandler(IotsaApiContext& ctx) override;
  void putHandler(IotsaApiContext& ctx) override;
  virtual bool configLoad(IotsaConfigFileLoad& cf, const String& name) override;
  virtual void configSave(IotsaConfigFileSave& cf, const String& name) override;
#ifdef IOTSA_WITH_WEB
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
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
  // Virtual, because some implementation may apply gamma differently.
  virtual float applyGamma(float level);
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
  IotsaContext::Persistence valuePersistence = IotsaContext::Saved;
  // The handlers in parts, for a subclass with a name of its own
  // (DimmerBLEClient): the values (with identify) and the settings, without
  // the name. The arg halves work on both contexts, and return whether
  // something changed; the caller calls updateDimmer().
  template<class Ctx> bool _valueArgs(Ctx& ctx);
  template<class Ctx> bool _settingArgs(Ctx& ctx);
  void _showValues(IotsaApiContext& ctx);
  void _showSettings(IotsaApiContext& ctx);
#ifdef IOTSA_WITH_WEB
  void _valueFields(IotsaWebContext& ctx, const String& heading);
  void _settingFields(IotsaWebContext& ctx, const String& heading, bool withName);
  // The heading of the dimmer's forms: its name, or its number.
  String _label();
#endif
private:
  // The whole put side, name included.
  template<class Ctx> void _args(Ctx& ctx);
};
};
#endif // _ABSTRACTDIMMER_H_
