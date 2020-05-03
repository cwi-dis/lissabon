//
// A Neopixel strip server, which allows control over a strip of neopixels, intended
// to be used as lighting. Color can be set as fraction RGB or HSL, gamma can be changed,
// interval between lit pixels can be changed. Control is through a web UI or
// through REST calls (and/or, depending on Iotsa compile time options, COAP calls).
// The web interface can be disabled by building iotsa with IOTSA_WITHOUT_WEB.
//

#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaLed.h"
#include "iotsaConfigFile.h"
#include <set>

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.
#define LED_PIN 22  // Define to turn on the LED when powered and not sleeping.

//
// Device can be rebooted or configuration mode can be requested by quickly tapping any button.
// TAP_DURATION sets the maximum time between press-release-press-etc.
#define TAP_COUNT_MODE_CHANGE 4
#define TAP_COUNT_REBOOT 8
#define TAP_DURATION 1000

IotsaApplication application("Iotsa BLE Dimmer");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

#include "iotsaBattery.h"
#define PIN_DISABLESLEEP 0
#define PIN_VBAT 37
#define VBAT_100_PERCENT (12.0/11.0) // 100K and 1M resistors divide by 11, not 10...
IotsaBatteryMod batteryMod(application);

#include "iotsaInput.h"
Touchpad touch1down(12, true, true, true);
Touchpad touch1up(13, true, true, true);
UpDownButtons encoder1(touch1down, touch1up, true);
#ifdef WITH_SECOND_DIMMER
Touchpad touch2down(14, true, true, true);
Touchpad touch2up(15, true, true, true);
UpDownButtons encoder2(touch2down, touch2up, true);
#endif // WITH_SECOND_DIMMER

Input* inputs[] = {
  &encoder1,
#ifdef WITH_SECOND_DIMMER
  &encoder2,
#endif
};

IotsaInputMod touchMod(application, inputs, sizeof(inputs)/sizeof(inputs[0]));

#include "iotsaBLEClient.h"
IotsaBLEClientMod bleClientMod(application);

// UUID of service advertised by iotsaLedstrip and iotsaDimmer devices
static BLEUUID serviceUUID("F3390001-F793-4D0C-91BB-C91EEB92A1A4");
static BLEUUID isOnUUID("F3390002-F793-4D0C-91BB-C91EEB92A1A4");
//static constexpr UUIDstring identifyUUID = "F3390003-F793-4D0C-91BB-C91EEB92A1A4";
static BLEUUID brightnessUUID("F3390004-F793-4D0C-91BB-C91EEB92A1A4");
//static constexpr UUIDstring tempUUID = "F3390005-F793-4D0C-91BB-C91EEB92A1A4";
//static constexpr UUIDstring intervalUUID = "F3390006-F793-4D0C-91BB-C91EEB92A1A4";

//
// LED Lighting control module. 
//
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
  void loop();
};

class DimmerUI : public BLEDimmer {
public:
  //DimmerUI(int _num, DimmerCallbacks *_callbacks) : BLEDimmer(_num, _callbacks) {}
  using BLEDimmer::BLEDimmer;
  void setEncoder(UpDownButtons& encoder);
  bool touchedOnOff();
  bool levelChanged();
};

class IotsaBLEDimmerMod : public IotsaApiMod, public DimmerCallbacks {
public:
  IotsaBLEDimmerMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaRestApiMod(_app, _auth),
    dimmer1(1, this)
#ifdef WITH_SECOND_DIMMER
    , dimmer2(2, this)
#endif
  {
  }
  void setup();
  void serverSetup();
  String info();
  void configLoad();
  void configSave();
  void loop();

protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  void deviceFound(BLEAdvertisedDevice& device);
private:
  void buttonChanged();
  void needSave();
  void handler();
  DimmerUI dimmer1;
#ifdef WITH_SECOND_DIMMER
  DimmerUI dimmer2;
#endif
  std::set<std::string> unknownDimmers;
  uint32_t saveAtMillis = 0;
  uint32_t ledOffUntilMillis = 0;
  uint32_t lastButtonChangeMillis = 0;
  int buttonChangeCount = 0;
};

bool
DimmerUI::touchedOnOff() {
  IFDEBUG IotsaSerial.printf("touchedOnOff %d: onOff=%d level=%f\n", num, isOn, level);
  callbacks->buttonChanged();
  updateDimmer();
  return true;
}

bool
DimmerUI::levelChanged() {
  IFDEBUG IotsaSerial.printf("levelChanged %d: onOff=%d level=%f\n", num, isOn, level);
  updateDimmer();
  return true;
}

void
DimmerUI::setEncoder(UpDownButtons& encoder) {
  encoder.setCallback(std::bind(&DimmerUI::levelChanged, this));
  // Bind up/down buttons to variable illum, ranging from minLevel to 1.0 in 25 steps
  encoder.bindVar(level, minLevel, 1.0, 0.02);
  encoder.bindStateVar(isOn);
  encoder.setStateCallback(std::bind(&DimmerUI::touchedOnOff, this));
}

bool BLEDimmer::available() {
  IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(name);
  return dimmer != NULL && dimmer->available();
}

String BLEDimmer::info() {
  String message = "Dimmer";
  message += String(num);
  message += ": ";
  message += name;
  if (!isOn) {
    message += " off";
  } else {
    message += " on (" + String(int(level*100)) + "%)";
  }
  message += "<br>";
  return message;
}

bool BLEDimmer::handlerArgs(IotsaWebServer *server) {
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

bool BLEDimmer::handlerConfigArgs(IotsaWebServer *server){
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
void BLEDimmer::configLoad(IotsaConfigFileLoad& cf) {
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

void BLEDimmer::configSave(IotsaConfigFileSave& cf) {
  String n_name = "dimmer" + String(num);
  cf.put(n_name + ".name", name);
  cf.put(n_name + ".level", level);
  cf.put(n_name + ".isOn", isOn);
  cf.put(n_name + ".minLevel", minLevel);
}

String BLEDimmer::handlerForm() {
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

String BLEDimmer::handlerConfigForm() {
  String s_num = String(num);
  String s_name = "dimmer" + s_num;
  String message = "<h2>Dimmer " + s_num + " configuration</h2><form method='get'>";
  message += "BLE name: <input name='" + s_name +".name' value='" + name + "'><br>";
  message += "Min Level (0.0..1.0): <input name='" + s_name +".minLevel' value='" + String(minLevel) + "'></br>";
  message += "<input type='submit'></form>";
  return message;
}

bool BLEDimmer::getHandler(JsonObject& reply) {
  reply["name"] = name;
  reply["available"] = available();
  reply["isOn"] = isOn;
  reply["level"] = level;
  reply["minLevel"] = minLevel;
  return true;
}

bool BLEDimmer::putHandler(const JsonVariant& request) {
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

void BLEDimmer::updateDimmer() {
  needTransmit = true;
  callbacks->needSave();
}

bool BLEDimmer::setName(String value) {
  if (value == name) return false;
  if (name) bleClientMod.delDevice(name);
  name = value;
  if (value) bleClientMod.addDevice(name);
  return true;
}

void BLEDimmer::loop() {
    // See whether we have some values to transmit to Dimmer1
  if (needTransmit) {
    IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(name);
    if (dimmer == NULL) {
      IFDEBUG IotsaSerial.printf("Ignoring transmission to dimmer %d\n", num);
      needTransmit = false;
    } else if (dimmer->available() && dimmer->connect()) {
      IFDEBUG IotsaSerial.println("xxxjack Transmit brightness");
      bool ok = dimmer->set(serviceUUID, brightnessUUID, (uint8_t)(level*100));
      if (!ok) {
        IFDEBUG IotsaSerial.println("BLE: set(brightness) failed");
      }
      IFDEBUG IotsaSerial.println("xxxjack Transmit ison");
      ok = dimmer->set(serviceUUID, isOnUUID, (uint8_t)isOn);
      if (!ok) {
        IFDEBUG IotsaSerial.println("BLE: set(isOn) failed");
      }
      IFDEBUG IotsaSerial.println("xxxjack disconnecting");
      dimmer->disconnect();
      IFDEBUG IotsaSerial.println("xxxjack disconnected");
      needTransmit = false;
    }
  }
}

void IotsaBLEDimmerMod::buttonChanged() {
  // Called whenever any button changed state.
  // Used to give visual feedback (led turning off) on presses and releases,
  // and to enable config mod after 4 taps and reboot after 8 taps
  uint32_t now = millis();
#ifdef LED_PIN
  digitalWrite(LED_PIN, HIGH);
  ledOffUntilMillis = now + 100;
#endif
  if (lastButtonChangeMillis > 0 && now < lastButtonChangeMillis + TAP_DURATION) {
    // A button change that was quick enough for a tap
    lastButtonChangeMillis = now;
    buttonChangeCount++;
    if (buttonChangeCount == TAP_COUNT_MODE_CHANGE) {
      IFDEBUG IotsaSerial.println("tap mode change");
      iotsaConfig.allowRequestedConfigurationMode();
    }
    if (buttonChangeCount == TAP_COUNT_REBOOT) {
      IFDEBUG IotsaSerial.println("tap mode reboot");
      ledOffUntilMillis = now + 2000;
      iotsaConfig.requestReboot(1000);
    }
  } else {
    // Either the first change, or too late. Reset.
    lastButtonChangeMillis = millis();
    buttonChangeCount = 0;
  }
}
#ifdef IOTSA_WITH_WEB
void
IotsaBLEDimmerMod::handler() {
  bool anyChanged;
  anyChanged = dimmer1.handlerConfigArgs(server);
  dimmer1.handlerArgs(server);
#ifdef WITH_SECOND_DIMMER
  anyChanged = dimmer2.handlerConfigArgs(server);
  dimmer2.handlerArgs(server);
#endif // WITH_SECOND_DIMMER
  if (anyChanged) {
    configSave();
  }

  String message = "<html><head><title>BLE Dimmers</title></head><body><h1>BLE Dimmers</h1>";
  message += dimmer1.handlerForm();

#ifdef WITH_SECOND_DIMMER
  message += dimmer2.handlerForm();
#endif // WITH_SECOND_DIMMER
  message += dimmer1.handlerConfigForm();
#ifdef WITH_SECOND_DIMMER
  message += dimmer2.handlerConfigForm();
#endif // WITH_SECOND_DIMMER
  message += "<h2>Available BLE dimmer devices</h2>";
  if (unknownDimmers.size() == 0) {
    message += "<p>No unassigned BLE dimmer devices seen recently.</p>";
  } else {
    message += "<ul>";
    for (auto it: unknownDimmers) {
      message += "<li>" + String(it.c_str()) + "</li>";
    }
    message += "</ul>";
  }
  message += "</body></html>";
  server->send(200, "text/html", message);
}

String IotsaBLEDimmerMod::info() {

  String message = "Built with BLE Dimmer module. See <a href='/bledimmer'>/bledimmer</a> to change settings or <a href='/api/bledimmer'>/api/bledimmer</a> for REST API.<br>";
  message += dimmer1.info();
#ifdef WITH_SECOND_DIMMER
  message += dimmer2.info();
#endif // WITH_SECOND_DIMMER
  return message;
}
#endif // IOTSA_WITH_WEB

bool IotsaBLEDimmerMod::getHandler(const char *path, JsonObject& reply) {
  // xxxjack need to distinguish between config and operational parameters
  JsonObject dimmer1Reply = reply.createNestedObject("dimmer1");
  dimmer1.getHandler(dimmer1Reply);
#ifdef WITH_SECOND_DIMMER
  JsonObject dimmer2Reply = reply.createNestedObject("dimmer2");
  dimmer2.getHandler(dimmer2Reply);
#endif
  if (unknownDimmers.size()) {
    JsonArray unknownReply = reply.createNestedArray("unassigned");
    for (auto it : unknownDimmers) {
      unknownReply.add(it.c_str());
    }
  }
  return true;
}

bool IotsaBLEDimmerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  // xxxjack need to distinguish between config and operational parameters
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  JsonVariant dimmer1Request = reqObj["dimmer1"];
  if (dimmer1Request) {
    if (dimmer1.putHandler(dimmer1Request)) anyChanged = true;
  }
#ifdef WITH_SECOND_DIMMER
  JsonVariant dimmer2Request = reqObj["dimmer2"];
  if (dimmer2Request) {
    if (dimmer2.putHandler(dimmer2Request)) anyChanged = true;
  }
#endif
  if (anyChanged) {
    configSave();
  }
  return anyChanged;
}

void IotsaBLEDimmerMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/bledimmer", std::bind(&IotsaBLEDimmerMod::handler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/bledimmer", true, true);
  name = "bledimmer";
#endif
}


void IotsaBLEDimmerMod::configLoad() {
  IotsaConfigFileLoad cf("/config/bledimmer.cfg");
  dimmer1.configLoad(cf);
#ifdef WITH_SECOND_DIMMER
  dimmer2.configLoad(cf);
#endif // WITH_SECOND_DIMMER
}

void IotsaBLEDimmerMod::configSave() {
  IotsaConfigFileSave cf("/config/bledimmer.cfg");
  dimmer1.configSave(cf);
#ifdef WITH_SECOND_DIMMER
  dimmer2.configSave(cf);
#endif // WITH_SECOND_DIMMER
}

void IotsaBLEDimmerMod::setup() {
  configLoad();
  iotsaConfig.allowRCMDescription("tap any touchpad 4 times");
#ifdef LED_PIN
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
#endif
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  dimmer1.setEncoder(encoder1);
#ifdef WITH_SECOND_DIMMER
  dimmer2.setEncoder(encoder2);
#endif // WITH_SECOND_DIMMER

  auto callback = std::bind(&IotsaBLEDimmerMod::deviceFound, this, std::placeholders::_1);
  bleClientMod.setDeviceFoundCallback(callback);
  bleClientMod.setServiceFilter(serviceUUID);
}

void IotsaBLEDimmerMod::deviceFound(BLEAdvertisedDevice& device) {
  IFDEBUG IotsaSerial.printf("deviceFound: iotsaLedstrip/iotsaDimmer \"%s\"\n", device.getName().c_str());
  // update the connection information, or add to unknown dimmers if not known
  if (!bleClientMod.deviceSeen(device.getName(), device)) {
    IFDEBUG IotsaSerial.printf("deviceFound: unknown dimmer \"%s\"\n", device.getName().c_str());
    unknownDimmers.insert(device.getName());
  }
}

void IotsaBLEDimmerMod::needSave() {
  saveAtMillis = millis() + 1000;
}

void IotsaBLEDimmerMod::loop() {
  // xxxjack debug: { static uint32_t last; if (millis() > last+1000) { IotsaSerial.println("xxxjack loop"); last = millis(); }}
  // See whether we have a value to save (because the user has been turning the dimmer)
  if (saveAtMillis > 0 && millis() > saveAtMillis) {
    saveAtMillis = 0;
    configSave();
  }
#ifdef LED_PIN
  // See whether we have to turn on the LED again (after a short off-period as feedback for user interaction)
  if (ledOffUntilMillis > 0 && millis() > ledOffUntilMillis) {
    digitalWrite(LED_PIN, LOW);
    ledOffUntilMillis = 0;
  }
#endif
  dimmer1.loop();
#ifdef WITH_SECOND_DIMMER
  dimmer2.loop();
#endif
}

// Instantiate the Led module, and install it in the framework
IotsaBLEDimmerMod bleDimmerControllerMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
