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
  // xxxjack need to cater for curLevel
  int thisDuration = int(animationDurationMillis * fabs(newLevel-prevLevel));
  millisAnimationStart = millis();
  millisAnimationEnd = millis() + thisDuration;
  iotsaConfig.postponeSleep(thisDuration+100);
#endif
}

void AbstractDimmer::calcCurLevel() {
#ifdef DIMMER_WITH_LEVEL
  float wantedLevel = level;
  if (overrideLevel > 0) wantedLevel = overrideLevel;
  if (!isOn) wantedLevel = 0;

#ifdef DIMMER_WITH_ANIMATION
    // Determine how far along the animation we are, and terminate the animation when done (or if it looks preposterous)
  uint32_t thisDur = millisAnimationEnd - millisAnimationStart;
  if (thisDur == 0) thisDur = 1;
  float progress = float(millis() - millisAnimationStart) / float(thisDur);
  if (progress < 0) progress = 0;
  if (progress >= 1) {
    // We are done with the animation
    progress = 1;
    millisAnimationStart = 0;
    millisAnimationEnd = 0;
    prevLevel = wantedLevel;

    IFDEBUG IotsaSerial.printf("IotsaDimmer: wantedLevel=%f level=%f\n", wantedLevel, level);
    callbacks->dimmerValueChanged();
  }
  curLevel = wantedLevel*progress + prevLevel*(1-progress);
#else
  curLevel = wantedLevel;
#endif // DIMMER_WITH_ANIMATION
  
  if (curLevel < 0) curLevel = 0;
  if (curLevel > 1) curLevel = 1;

#ifdef DIMMER_WITH_GAMMA
  if (gamma && gamma != 1.0) curLevel = powf(curLevel, gamma);
#endif // DIMMER_WITH_GAMMA
#endif // DIMMER_WITH_LEVEL
}

void AbstractDimmer::identify() {}

String AbstractDimmer::info() {
  String message = "Dimmer";
  message += getUserVisibleName();
  if (!isOn) {
    message += " off";
  } else {
    message += " on";
#ifdef DIMMER_WITH_LEVEL
    message += "(" + String(int(level*100)) + "%)";
#endif
#ifdef DIMMER_WITH_TEMPERATURE
    message += String(temperature) + "K";
#endif // DIMMER_WITH_TEMPERATURE
  }
#ifdef DIMMER_WITH_GAMMA
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_PWMFREQUENCY
#endif // DIMMER_WITH_PWMFREQUENCY
  message += "<br>";
  return message;
}

bool AbstractDimmer::formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) {
  bool anyChanged = false;
  String n_isOn = f_name + ".isOn";
  String n_level = f_name + ".level";
  String n_identify = f_name + ".identify";
  if (server->hasArg(n_identify) && server->arg(n_identify).toInt()) {
    identify();
  }
  if (server->hasArg(n_isOn)) {
    int val = server->arg(n_isOn).toInt();
    if (val != isOn) {
      isOn = val;
      updateDimmer();
      anyChanged = true;
    }
  }
#ifdef DIMMER_WITH_LEVEL
  if (server->hasArg(n_level)) {
    float val = server->arg(n_level).toFloat();
    if (val < minLevel) val = minLevel;
    if (val > 1) val = 1;
    if (val != level) {
      level = val;
      updateDimmer();
      anyChanged = true;
    }
  }
#endif
#ifdef DIMMER_WITH_GAMMA
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_TEMPERATURE
  String n_temperature = f_name + ".temperature";
  if (server->hasArg(n_temperature)) {
    float val = server->arg(n_temperature).toFloat();
    if (val != temperature) {
      temperature = val;
      updateDimmer();
      anyChanged = true;
    }
  }
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
#endif // DIMMER_WITH_PWMFREQUENCY
  if (!includeConfig) return anyChanged;
  //
  // Examine configuration parameters
  //
  String n_name = f_name + ".name";
  if (server->hasArg(n_name)) {
    // xxxjack check permission
    String value = server->arg(n_name);
    anyChanged |= setName(value);
  }
#ifdef DIMMER_WITH_LEVEL
  String n_minLevel = f_name + ".minLevel";
  if (server->hasArg(n_minLevel)) {
    // xxxjack check permission
    float val = server->arg(n_minLevel).toFloat();
    if (val != minLevel) {
      minLevel = val;
      anyChanged = true;
    }
    if (level < minLevel) level = minLevel;
  }
#endif
#ifdef DIMMER_WITH_GAMMA
  String n_gamma = f_name + ".gamma";
  if (server->hasArg(n_gamma)) {
    // xxxjack check permission
    float val = server->arg(n_gamma).toFloat();
    if (val != gamma) {
      gamma = val;
      anyChanged = true;
    }
  }
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
  String n_animation = f_name + ".animation";
  if (server->hasArg(n_animation)) {
    // xxxjack check permission
    int val = server->arg(n_animation).toInt();
    if (val != animationDurationMillis) {
      animationDurationMillis = val;
      anyChanged = true;
    }
  }
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_TEMPERATURE
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
  String n_pwmfrequency = f_name + ".pwmFrequency";
  if (server->hasArg(n_pwmfrequency)) {
    // xxxjack check permission
    float val = server->arg(n_pwmfrequency).toFloat();
    if (val != pwmFrequency) {
      pwmFrequency = val;
      anyChanged = true;
    }
  }
#endif // DIMMER_WITH_PWMFREQUENCY
  return anyChanged;
}

bool AbstractDimmer::configLoad(IotsaConfigFileLoad& cf, const String& n_name) {
  int value;
  String strval;
  cf.get(n_name + ".name", strval, "");
  setName(strval);
  cf.get(n_name + ".isOn", value, 0);
  isOn = value;
#ifdef DIMMER_WITH_LEVEL
  cf.get(n_name + ".level", level, 0.0);
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
  cf.get(n_name + ".temperature", temperature, 4000.0);
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
  cf.get(n_name + ".pwmFrequency", pwmFrequency, 5000.0);
#endif // DIMMER_WITH_PWMFREQUENCY
  return name != "";
}

void AbstractDimmer::configSave(IotsaConfigFileSave& cf, const String& n_name) {
  cf.put(n_name + ".name", name);
  cf.put(n_name + ".isOn", isOn);
#ifdef DIMMER_WITH_LEVEL
  cf.put(n_name + ".level", level);
  cf.put(n_name + ".minLevel", minLevel);
#endif // DIMMER_WITH_LEVEL
#ifdef DIMMER_WITH_GAMMA
  cf.put(n_name + ".gamma", gamma);
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
  cf.put(n_name + ".animation", animationDurationMillis);
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_TEMPERATURE
  cf.put(n_name + ".temperature", temperature);
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
  cf.put(n_name + ".pwmFrequency", pwmFrequency);
#endif // DIMMER_WITH_PWMFREQUENCY
}

void AbstractDimmer::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
  message += "<br>Dimmer: ";
  message += text;
  if (!available()) message += " <em>(dimmer may be unavailable)</em>";
  message += "<br>";
  if (!includeConfig) {
    message += "<input type='checkbox' name='" + f_name + ".identify' value='1'>Identify (two rapid flashes)<br>";
    String checkedOn = isOn ? "checked" : "";
    String checkedOff = !isOn ? "checked " : "";
    message += "<input type='radio' name='" + f_name +".isOn'" + checkedOff + " value='0'>Off <input type='radio' " + checkedOn + " name='" + f_name + ".isOn' value='1'>On</br>";
#ifdef DIMMER_WITH_LEVEL
    message += "Level (0.0..1.0): <input name='" + f_name +".level' value='" + String(level) + "'></br>";
#endif
#ifdef DIMMER_WITH_GAMMA
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_TEMPERATURE
    message += "Color Temperature: <input name='" + f_name +".temperature' value='" + String(temperature) + "'>";
    message += "(3000.0 is warm, 4000.0 is neutral, 6000.0 is cool)<br>";
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
#endif // DIMMER_WITH_PWMFREQUENCY
  } else {
    //
    // Configuration form fields
    //
#ifdef DIMMER_WITH_LEVEL
    message += "Min Level (0.0..1.0): <input name='" + f_name +".minLevel' value='" + String(minLevel) + "'></br>";
#endif
#ifdef DIMMER_WITH_GAMMA
    message += "Gamma: <input name='" + f_name +".gamma' value='" + String(gamma) + "'>";
    message += "(1.0 is linear, 2.2 is common for direct PWM LEDs)<br>";
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
    message += "Animation duration (milliseconds): <input name='" + f_name +".animation' value='" + String(animationDurationMillis) + "'></br>";
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_TEMPERATURE
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
    message += "PWM Frequency: <input name='" + f_name +".pwmFrequency' value='" + String(pwmFrequency) + "'>";
    message += "(Adapt when dimmed device flashes. 100 may be fine for dimmable LED lamps, 5000 for incandescent)<br>";
#endif // DIMMER_WITH_PWMFREQUENCY
  }
}

void AbstractDimmer::formHandler_TD(String& message, bool includeConfig) {
  IotsaSerial.println("AbstractDimmer: formHandler_TD not implemented");
}

void AbstractDimmer::getHandler(JsonObject& reply) {
  reply["name"] = name;
  reply["available"] = available();
  reply["isOn"] = isOn;
#ifdef DIMMER_WITH_LEVEL
  reply["level"] = level;
  reply["minLevel"] = minLevel;
#endif // DIMMER_WITH_LEVEL
#ifdef DIMMER_WITH_GAMMA
  reply["gamma"] = gamma;
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
  reply["animation"] = animationDurationMillis;
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_TEMPERATURE
  reply["temperature"] = temperature;
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
  reply["pwmFrequency"] = pwmFrequency;
#endif // DIMMER_WITH_PWMFREQUENCY
}

bool AbstractDimmer::putHandler(const JsonVariant& request) {
  bool anyChanged = false;
  bool dimmerChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (!reqObj) return false;
  if (reqObj["identify"]|0) {
    identify();
    anyChanged = true;
  }
  if (reqObj.containsKey("isOn")) {
    isOn = reqObj["isOn"];
    dimmerChanged = true;
  }
#ifdef DIMMER_WITH_LEVEL
  if (reqObj.containsKey("level")) {
    level = reqObj["level"];
    dimmerChanged = true;
  }
#endif // DIMMER_WITH_LEVEL
#ifdef DIMMER_WITH_GAMMA
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_TEMPERATURE
  if (reqObj.containsKey("temperature")) {
    temperature = reqObj["temperature"];
    dimmerChanged = true;
  }
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
#endif // DIMMER_WITH_PWMFREQUENCY
  if (dimmerChanged) {
    updateDimmer();
    anyChanged = true;
  }
  if (true) {
    // xxxjack the following should only be changed when in configuration mode
    bool configChanged = false;
    // xxxjack check permission
    if (reqObj.containsKey("name")) {
      String value = reqObj["name"].as<String>();
      if (setName(name)) configChanged = true;
    }
#ifdef DIMMER_WITH_LEVEL
    if (reqObj.containsKey("minLevel")) {
      minLevel = reqObj["minLevel"];
      configChanged = true;
    }
#endif // DIMMER_WITH_LEVEL
#ifdef DIMMER_WITH_GAMMA
    if (reqObj.containsKey("gamma")) {
      gamma = reqObj["gamma"];
      configChanged = true;
    }
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
    if (reqObj.containsKey("animation")) {
      animationDurationMillis = reqObj["animation"];
      configChanged = true;
    }
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_TEMPERATURE
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
    if (reqObj.containsKey("pwmFrequency")) {
      pwmFrequency = reqObj["pwmFrequency"];
      configChanged = true;
    }
#endif // DIMMER_WITH_PWMFREQUENCY
    // Ensure level is a reasonable value
    if (level < minLevel) level = minLevel;
    if (level > 1) level = 1;
    // xxxjack if configChanged we should save
    anyChanged |= configChanged;

  }
  return anyChanged;
}


}