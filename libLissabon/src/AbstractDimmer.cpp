#include "AbstractDimmer.h"

namespace Lissabon {

bool AbstractDimmer::setName(String value) {
  if (name == value) return false;
  name = value;
  return true;
}

String AbstractDimmer::getUserVisibleName() {
  if (name != "") return name;
  return String(num);
}

bool AbstractDimmer::hasName() {
  return name != "";
}

void AbstractDimmer::updateDimmer() {
#ifdef DIMMER_WITH_ANIMATION
  float newLevel = isOn ? level : 0;
  if (animationEndMillis > 0) {
    // Need to cater for curLevel for animation that is in progress
    animationPrevLevel = animationCurLevel;
  }
  int thisDuration = int(animationDurationMillis * fabs(newLevel-animationPrevLevel));
  animationStartMillis = millis()-1;
  animationEndMillis = millis() + thisDuration;
  IOTSA_LOG_DEBUG("AbstractDimmer", "dimmer %d: level %f->%f in %d ms", num, animationPrevLevel, newLevel, thisDuration);
  iotsaController.postponeSleep(thisDuration+100);
#endif
}

void AbstractDimmer::calcCurLevel() {
#ifdef DIMMER_WITH_LEVEL
  float wantedLevel = level;
  if (!isOn) wantedLevel = 0;

#ifdef DIMMER_WITH_ANIMATION
    // Determine how far along the animation we are, and terminate the animation when done (or if it looks preposterous)
  uint32_t thisDur = animationEndMillis - animationStartMillis;
  if (thisDur == 0) thisDur = 1;
  float progress = float(millis() - animationStartMillis) / float(thisDur);
  if (progress < 0) progress = 0;
  if (progress > 1) progress = 1;
  animationCurLevel = wantedLevel*progress + animationPrevLevel*(1-progress);
  if (progress < 1) {
    curLevel = animationCurLevel;
  } else {
    // We are done with the animation
    progress = 1;
    animationStartMillis = 0;
    animationEndMillis = 0;
    curLevel = animationPrevLevel = animationCurLevel = wantedLevel;

    IOTSA_LOG_DEBUG("AbstractDimmer", "dimmer %d: animation done, wantedLevel=%f level=%f", num, wantedLevel, level);
    callbacks->dimmerValueChanged();
  }
#else
  curLevel = wantedLevel;
  callbacks->dimmerValueChanged();
#endif // DIMMER_WITH_ANIMATION
  
  if (curLevel < 0) curLevel = 0;
  if (curLevel > 1) curLevel = 1;

#ifdef DIMMER_WITH_GAMMA
  curLevel = applyGamma(curLevel);
#endif // DIMMER_WITH_GAMMA
#endif // DIMMER_WITH_LEVEL
}

#ifdef DIMMER_WITH_GAMMA

float AbstractDimmer::applyGamma(float level) {
  if (gamma && gamma != 1.0) level = powf(level, gamma);
  return level;
}
#endif

void AbstractDimmer::identify() {}

String AbstractDimmer::info() {
  String message = "Dimmer " + IotsaBaseModule::htmlEncode(getUserVisibleName());
  if (!isOn) {
    message += ": off";
  } else {
    message += ": on";
#ifdef DIMMER_WITH_LEVEL
    message += " (" + String(int(level*100)) + "%)";
#endif
#ifdef DIMMER_WITH_TEMPERATURE
    message += ", " + String(int(temperature)) + "K";
#endif // DIMMER_WITH_TEMPERATURE
  }
  message += "<br>";
  return message;
}

//
// The put side, for both contexts (cwi-dis/iotsa#183, #296).
//

template<class Ctx> bool AbstractDimmer::_valueArgs(Ctx& ctx) {
  bool changed = false;
  changed |= ctx.arg("isOn", isOn, valuePersistence);
#ifdef DIMMER_WITH_LEVEL
  if (ctx.arg("level", level, valuePersistence)) {
    if (level < minLevel) level = minLevel;
    if (level > 1) level = 1;
    changed = true;
  }
#endif
#ifdef DIMMER_WITH_TEMPERATURE
  changed |= ctx.arg("temperature", temperature, valuePersistence);
#endif // DIMMER_WITH_TEMPERATURE
  bool doIdentify = ctx.argAction("identify", IotsaContext::Transient);
  if (doIdentify && !ctx.failed()) identify();
  return changed;
}

// Tuning a light (how it dims, its curve, the PWM whine) needs the right
// "lighttuning"; switching it is open to everyone.
template<class Ctx> bool AbstractDimmer::_settingArgs(Ctx& ctx) {
  IotsaContext::RightsScope rights(ctx);
  ctx.argsNeedRight("lighttuning");
  bool changed = false;
#ifdef DIMMER_WITH_LEVEL
  if (ctx.arg("minLevel", minLevel, IotsaContext::Saved)) {
    if (level < minLevel) level = minLevel;
    changed = true;
  }
#endif
#ifdef DIMMER_WITH_GAMMA
  changed |= ctx.arg("gamma", gamma, IotsaContext::Saved);
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
  changed |= ctx.arg("animation", animationDurationMillis, IotsaContext::Saved);
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_PWMFREQUENCY
  changed |= ctx.arg("pwmFrequency", pwmFrequency, IotsaContext::Saved);
#endif // DIMMER_WITH_PWMFREQUENCY
  return changed;
}

// Whatever was assigned before an error is shown, so updateDimmer() is
// called even then.
template<class Ctx> void AbstractDimmer::_args(Ctx& ctx) {
  String newName = name;
  bool rename;
  {
    IotsaContext::RightsScope rights(ctx);
    ctx.argsNeedRight("lighttuning");
    rename = ctx.arg("name", newName, IotsaContext::Saved);
  }
  bool changed = _settingArgs(ctx);
  changed |= _valueArgs(ctx);
  if (rename) setName(newName);
  if (changed) updateDimmer();
}

template bool AbstractDimmer::_valueArgs<IotsaApiContext>(IotsaApiContext& ctx);
template bool AbstractDimmer::_settingArgs<IotsaApiContext>(IotsaApiContext& ctx);
#ifdef IOTSA_WITH_WEB
template bool AbstractDimmer::_valueArgs<IotsaWebContext>(IotsaWebContext& ctx);
template bool AbstractDimmer::_settingArgs<IotsaWebContext>(IotsaWebContext& ctx);
#endif

void AbstractDimmer::putHandler(IotsaApiContext& ctx) {
  _args(ctx);
}

//
// The get side.
//

void AbstractDimmer::_showValues(IotsaApiContext& ctx) {
  ctx.show("isOn", isOn);
#ifdef DIMMER_WITH_LEVEL
  ctx.show("level", level);
#endif // DIMMER_WITH_LEVEL
#ifdef DIMMER_WITH_TEMPERATURE
  ctx.show("temperature", temperature);
#endif // DIMMER_WITH_TEMPERATURE
}

void AbstractDimmer::_showSettings(IotsaApiContext& ctx) {
#ifdef DIMMER_WITH_LEVEL
  ctx.show("minLevel", minLevel);
#endif // DIMMER_WITH_LEVEL
#ifdef DIMMER_WITH_GAMMA
  ctx.show("gamma", gamma);
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
  ctx.show("animation", animationDurationMillis);
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_PWMFREQUENCY
  ctx.show("pwmFrequency", pwmFrequency);
#endif // DIMMER_WITH_PWMFREQUENCY
}

void AbstractDimmer::getHandler(IotsaApiContext& ctx) {
  ctx.show("name", name);
  ctx.show("available", available());
  _showValues(ctx);
  _showSettings(ctx);
}

#ifdef IOTSA_WITH_WEB
String AbstractDimmer::_label() {
  if (name != "") return name;
  return "Dimmer " + String(num);
}

void AbstractDimmer::formHandler_args(IotsaWebContext& ctx) {
  _args(ctx);
}

void AbstractDimmer::_valueFields(IotsaWebContext& ctx, const String& heading) {
  {
    IotsaWebContext::Form form(ctx, "Set", heading);
    ctx.showCheckbox("On", "isOn", isOn, valuePersistence);
#ifdef DIMMER_WITH_LEVEL
    ctx.showField("Level", "level", level, valuePersistence, "0.0 to 1.0");
#endif
#ifdef DIMMER_WITH_TEMPERATURE
    ctx.showField("Color temperature", "temperature", temperature, valuePersistence, "3000 is warm, 4000 is neutral, 6000 is cool");
#endif // DIMMER_WITH_TEMPERATURE
  }
  ctx.showButton("identify", "Identify (two rapid flashes)", IotsaContext::Transient);
}

void AbstractDimmer::_settingFields(IotsaWebContext& ctx, const String& heading, bool withName) {
  IotsaContext::RightsScope rights(ctx);
  ctx.argsNeedRight("lighttuning");
  IotsaWebContext::Form form(ctx, "Save", heading);
  if (withName) ctx.showField("Name", "name", name, IotsaContext::Saved);
#ifdef DIMMER_WITH_LEVEL
  ctx.showField("Minimum level", "minLevel", minLevel, IotsaContext::Saved, "0.0 to 1.0, the lowest the buttons go");
#endif
#ifdef DIMMER_WITH_GAMMA
  ctx.showField("Gamma", "gamma", gamma, IotsaContext::Saved, "1.0 is linear, 2.2 is common for direct PWM LEDs");
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
  ctx.showField("Animation duration", "animation", animationDurationMillis, IotsaContext::Saved, "milliseconds, for 0 to 100%");
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_PWMFREQUENCY
  ctx.showField("PWM frequency", "pwmFrequency", pwmFrequency, IotsaContext::Saved, "Hz. Change it when the light flickers: 100 may be fine for dimmable LED lamps, 5000 for incandescent");
#endif // DIMMER_WITH_PWMFREQUENCY
}

void AbstractDimmer::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.includesEveryday()) _valueFields(ctx, _label());
  if (ctx.includesSettings()) _settingFields(ctx, _label(), true);
}
#endif // IOTSA_WITH_WEB

bool AbstractDimmer::configLoad(IotsaConfigFileLoad& cf, const String& n_name) {
  int value;
  String strval;
  cf.get(n_name + ".name", strval, "");
  setName(strval);
  bool savesValues = valuePersistence != IotsaContext::Transient;
  if (savesValues) {
    cf.get(n_name + ".isOn", value, 0);
    isOn = value;
  }
#ifdef DIMMER_WITH_LEVEL
  if (savesValues) cf.get(n_name + ".level", level, 0.0);
  cf.get(n_name + ".minLevel", minLevel, 0.1);
  if (level < minLevel) level = minLevel;
  if (level > 1) level = 1;
#endif
#ifdef DIMMER_WITH_GAMMA
  cf.get(n_name + ".gamma", gamma, 1.0);
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
  cf.get(n_name + ".animation", animationDurationMillis, 500);
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_TEMPERATURE
  if (savesValues) cf.get(n_name + ".temperature", temperature, 4000.0);
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
  cf.get(n_name + ".pwmFrequency", pwmFrequency, 5000.0);
#endif // DIMMER_WITH_PWMFREQUENCY
  return name != "";
}

void AbstractDimmer::configSave(IotsaConfigFileSave& cf, const String& n_name) {
  cf.put(n_name + ".name", name);
  bool savesValues = valuePersistence != IotsaContext::Transient;
  if (savesValues) cf.put(n_name + ".isOn", isOn);
#ifdef DIMMER_WITH_LEVEL
  if (savesValues) cf.put(n_name + ".level", level);
  cf.put(n_name + ".minLevel", minLevel);
#endif // DIMMER_WITH_LEVEL
#ifdef DIMMER_WITH_GAMMA
  cf.put(n_name + ".gamma", gamma);
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
  cf.put(n_name + ".animation", animationDurationMillis);
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_TEMPERATURE
  if (savesValues) cf.put(n_name + ".temperature", temperature);
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
  cf.put(n_name + ".pwmFrequency", pwmFrequency);
#endif // DIMMER_WITH_PWMFREQUENCY
}

}
