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

void AbstractDimmer::identify() {}

String AbstractDimmer::info() {
  String message = "Dimmer";
  message += getUserVisibleName();
  if (!isOn) {
    message += " off";
  } else {
    message += " on (" + String(int(level*100)) + "%)";
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

bool AbstractDimmer::handlerArgs(IotsaWebServer *server) {
  bool anyChanged = false;
  String n_dimmer = "dimmer" + String(num);
  String n_isOn = n_dimmer + ".isOn";
  String n_level = n_dimmer + ".level";
  if (server->hasArg(n_isOn)) {
    int val = server->arg(n_isOn).toInt();
    if (val != isOn) {
      isOn = val;
      updateDimmer();
      anyChanged = true;
    }
  }
  if (server->hasArg(n_level)) {
    float val = server->arg(n_level).toFloat();
    if (val != level) {
      level = val;
      updateDimmer();
      anyChanged = true;
    }
  }
#ifdef DIMMER_WITH_GAMMA
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_TEMPERATURE
  String n_temperature = n_dimmer + ".temperature";
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
  return anyChanged;
}

bool AbstractDimmer::handlerConfigArgs(IotsaWebServer *server){
  bool anyChanged = false;
  String n_dimmer = "dimmer" + String(num);
  String n_name = n_dimmer + ".name";
  String n_minLevel = n_dimmer + ".minLevel";
  if (server->hasArg(n_name)) {
    // xxxjack check permission
    String value = server->arg(n_name);
    anyChanged = setName(value);
  }
  if (server->hasArg(n_minLevel)) {
    // xxxjack check permission
    float val = server->arg(n_minLevel).toFloat();
    if (val != minLevel) {
      minLevel = val;
      anyChanged = true;
    }
  }
#ifdef DIMMER_WITH_GAMMA
  String n_gamma = n_dimmer + ".gamma";
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
  String n_animation = n_dimmer + ".animation";
  if (server->hasArg(n_animation)) {
    // xxxjack check permission
    int val = server->arg(n_animation).toInt();
    if (val != animationDirationMillis) {
      animationDirationMillis = val;
      anyChanged = true;
    }
  }
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_TEMPERATURE
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
  String n_pwmfrequency = n_dimmer + ".pwmFrequency";
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
void AbstractDimmer::configLoad(IotsaConfigFileLoad& cf) {
  int value;
  String n_name = "dimmer" + String(num);
  String strval;
  cf.get(n_name + ".name", strval, "");
  setName(strval);
  cf.get(n_name + ".isOn", value, 0);
  isOn = value;
  cf.get(n_name + ".level", level, 0.0);
  cf.get(n_name + ".minLevel", minLevel, 0.1);
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
}

void AbstractDimmer::configSave(IotsaConfigFileSave& cf) {
  String n_name = "dimmer" + String(num);
  cf.put(n_name + ".name", name);
  cf.put(n_name + ".level", level);
  cf.put(n_name + ".isOn", isOn);
  cf.put(n_name + ".minLevel", minLevel);
#ifdef DIMMER_WITH_GAMMA
  cf.put(n_name + ".gamma", gamma);
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
  cf.put(n_name + ".animation", animation);
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_TEMPERATURE
  cf.put(n_name + ".temperature", temperature);
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
  cf.put(n_name + ".pwmFrequency", pwmFrequency);
#endif // DIMMER_WITH_PWMFREQUENCY
}

String AbstractDimmer::handlerForm() {
  String s_num = String(num);
  String s_name = "dimmer" + s_num;

  String message = "<h2>Dimmer " + s_num + " (" + name + ") operation</h2><form method='get'>";
  if (!available()) message += "<em>(dimmer may be unavailable)</em><br>";
  String checkedOn = isOn ? "checked" : "";
  String checkedOff = !isOn ? "checked " : "";
  message += "<input type='radio' name='" + s_name +".isOn'" + checkedOff + " value='0'>Off <input type='radio' " + checkedOn + " name='" + s_name + ".isOn' value='1'>On</br>";
  message += "Level (0.0..1.0): <input name='" + s_name +".level' value='" + String(level) + "'></br>";
#ifdef DIMMER_WITH_GAMMA
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_TEMPERATURE
  message += "Color Temperature: <input name='" + s_name +".temperature' value='" + String(temperature) + "'></br>";
  message += "(3000.0 is warm, 4000.0 is neutral, 6000.0 is cool)<br>";
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
#endif // DIMMER_WITH_PWMFREQUENCY
  message += "<input type='submit'></form>";
  return message;
}

String AbstractDimmer::handlerConfigForm() {
  String s_num = String(num);
  String s_name = "dimmer" + s_num;
  String message = "<h2>Dimmer " + s_num + " configuration</h2><form method='get'>";
  message += "BLE name: <input name='" + s_name +".name' value='" + name + "'><br>";
  message += "Min Level (0.0..1.0): <input name='" + s_name +".minLevel' value='" + String(minLevel) + "'></br>";
#ifdef DIMMER_WITH_GAMMA
  message += "Gamma: <input name='" + s_name +".minLevel' value='" + String(minLevel) + "'></br>";
  message += "(1.0 is linear, 2.2 is common for direct PWM LEDs)<br>";
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
  message += "Animation duration (milliseconds): <input name='" + s_name +".animation' value='" + String(animationDurationMillis) + "'></br>";
#endif // DIMMER_WITH_ANIMATION
#ifdef DIMMER_WITH_TEMPERATURE
#endif // DIMMER_WITH_TEMPERATURE
#ifdef DIMMER_WITH_PWMFREQUENCY
  message += "PWM Frequency: <input name='" + s_name +".minLevel' value='" + String(minLevel) + "'></br>";
  message += "(Adapt when dimmed device flashes. 100 may be fine for dimmable LED lamps, 5000 for incandescent)<br>";
#endif // DIMMER_WITH_PWMFREQUENCY
  message += "<input type='submit'></form>";
  return message;
}

bool AbstractDimmer::getHandler(JsonObject& reply) {
  reply["name"] = name;
  reply["available"] = available();
  reply["isOn"] = isOn;
  reply["level"] = level;
  reply["minLevel"] = minLevel;
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
  reply["pwmFrequency"]
#endif // DIMMER_WITH_PWMFREQUENCY
  return true;
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
  if (reqObj.containsKey("level")) {
    level = reqObj["level"];
    dimmerChanged = true;
  }
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
  return anyChanged;
}
bool AbstractDimmer::putConfigHandler(const JsonVariant& request) {
  bool configChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (!reqObj) return false;
  // xxxjack check permission
  if (reqObj.containsKey("name")) {
    String value = reqObj["name"].as<String>();
    if (setName(name)) configChanged = true;
  }
  if (reqObj.containsKey("minLevel")) {
    minLevel = reqObj["minLevel"];
    configChanged = true;
  }
#ifdef DIMMER_WITH_GAMMA
  if (reqObj.containsKey("gamma")) {
    gamma = reqObj["gamma"];
    configChanged = true;
  }
#endif // DIMMER_WITH_GAMMA
#ifdef DIMMER_WITH_ANIMATION
  if (reqObj.containsKey("animation")) {
    animation = reqObj["animation"];
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
  return configChanged;
}

}