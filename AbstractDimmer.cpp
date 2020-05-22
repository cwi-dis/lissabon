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

String AbstractDimmer::info() {
  String message = "Dimmer";
  message += getUserVisibleName();
  if (!isOn) {
    message += " off";
  } else {
    message += " on (" + String(int(level*100)) + "%)";
  }
  message += "<br>";
  return message;
}

bool AbstractDimmer::handlerArgs(IotsaWebServer *server) {
  String n_dimmer = "dimmer" + String(num);
  String n_isOn = n_dimmer + ".isOn";
  String n_level = n_dimmer + ".level";
  if (server->hasArg(n_isOn)) {
    int val = server->arg(n_isOn).toInt();
    if (val != isOn) {
      isOn = val;
      updateDimmer();
    }
  }
  if (server->hasArg(n_level)) {
    float val = server->arg(n_level).toFloat();
    if (val != level) {
      level = val;
      updateDimmer();
    }
  }
  return true;
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
}

void AbstractDimmer::configSave(IotsaConfigFileSave& cf) {
  String n_name = "dimmer" + String(num);
  cf.put(n_name + ".name", name);
  cf.put(n_name + ".level", level);
  cf.put(n_name + ".isOn", isOn);
  cf.put(n_name + ".minLevel", minLevel);
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
  message += "<input type='submit'></form>";
  return message;
}

String AbstractDimmer::handlerConfigForm() {
  String s_num = String(num);
  String s_name = "dimmer" + s_num;
  String message = "<h2>Dimmer " + s_num + " configuration</h2><form method='get'>";
  message += "BLE name: <input name='" + s_name +".name' value='" + name + "'><br>";
  message += "Min Level (0.0..1.0): <input name='" + s_name +".minLevel' value='" + String(minLevel) + "'></br>";
  message += "<input type='submit'></form>";
  return message;
}

bool AbstractDimmer::getHandler(JsonObject& reply) {
  reply["name"] = name;
  reply["available"] = available();
  reply["isOn"] = isOn;
  reply["level"] = level;
  reply["minLevel"] = minLevel;
  return true;
}

bool AbstractDimmer::putHandler(const JsonVariant& request) {
  bool configChanged = false;
  bool dimmerChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (!reqObj) return false;
  if (reqObj.containsKey("name")) {
    String value = reqObj["name"].as<String>();
    if (setName(name)) configChanged = true;
  }
  if (reqObj.containsKey("isOn")) {
    isOn = reqObj["isOn"];
    dimmerChanged = true;
  }
  if (reqObj.containsKey("level")) {
    level = reqObj["level"];
    dimmerChanged = true;
  }
  if (reqObj.containsKey("minLevel")) {
    minLevel = reqObj["minLevel"];
    configChanged = true;
  }
  if (dimmerChanged) {
    updateDimmer();
  }
  return configChanged;
}

}