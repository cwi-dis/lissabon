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
#endif

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
BLEUUID ledstripServiceUUID("153C0001-D28E-40B8-84EB-7F64B56D4E2E");

//
// LED Lighting control module. 
//

class IotsaBLEDimmerMod : public IotsaApiMod {
public:
  using IotsaApiMod::IotsaApiMod;
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
  void handler();
  bool touched1OnOff();
  bool level1Changed();
  void updateDimmer1();
  bool isOn1;
  float level1;
  float minLevel1;
  String nameDimmer1;
#ifdef WITH_SECOND_DIMMER
  bool touched2OnOff();
  bool level2Changed();
  void updateDimmer2();
  bool isOn2;
  float level2;
  float minLevel2;
  String nameDimmer2;
#endif
  std::set<std::string> unknownDimmers;
  uint32_t saveAtMillis = 0;
  uint32_t ledOffUntilMillis = 0;
  uint32_t lastButtonChangeMillis = 0;
  int buttonChangeCount = 0;
};

bool
IotsaBLEDimmerMod::touched1OnOff() {
  IFDEBUG IotsaSerial.printf("touched1OnOff: onOff=%d level=%f\n", isOn1, level1);
  buttonChanged();
  updateDimmer1();
  return true;
}

bool
IotsaBLEDimmerMod::level1Changed() {
  IFDEBUG IotsaSerial.printf("level1Changed: onOff=%d level=%f\n", isOn1, level1);
  updateDimmer1();
  return true;
}

void IotsaBLEDimmerMod::updateDimmer1() {
  // xxxjack send onOff and level to dimmer
  iotsaConfig.postponeSleep(2100);
  saveAtMillis = millis() + 2000;
}

#ifdef WITH_SECOND_DIMMER
bool
IotsaBLEDimmerMod::touched2OnOff() {
  IFDEBUG IotsaSerial.printf("touched2OnOff: onOff=%d level=%f\n", isOn2, level2);
  buttonChanged();
  updateDimmer2();
  return true;
}

bool
IotsaBLEDimmerMod::level2Changed() {
  IFDEBUG IotsaSerial.printf("level2Changed: onOff=%d level=%f\n", isOn2, level2);
  updateDimmer2();
  return true;
}

void IotsaBLEDimmerMod::updateDimmer2() {
  // xxxjack send onOff and level to dimmer
  iotsaConfig.postponeSleep(2100);
  saveAtMillis = millis() + 2000;
}
#endif // WITH_SECOND_DIMMER

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
  bool anyChanged = false;
  if (server->hasArg("dimmer1")) {
    String value = server->arg("dimmer1");
    if (value != nameDimmer1) {
      if (nameDimmer1) bleClientMod.delDevice(nameDimmer1);
      nameDimmer1 = value;
      if (value) bleClientMod.addDevice(nameDimmer1);
      anyChanged = true;
    }
  }
  if (server->hasArg("isOn1")) {
    isOn1 = server->arg("isOn1").toInt();
    anyChanged = true;
  }
  if (server->hasArg("level1")) {
    isOn1 = server->arg("level1").toFloat();
    anyChanged = true;
  }
  if (server->hasArg("minLevel1")) {
    isOn1 = server->arg("minLevel1").toFloat();
    anyChanged = true;
  }
  if (anyChanged) {
    configSave();
    // xxxjack send changes to dimmers
  }
  String message = "<html><head><title>BLE Dimmers</head><body><h1>BLE Dimmers</h1>";
}

String IotsaBLEDimmerMod::info() {
  return "";
}
#endif // IOTSA_WITH_WEB

bool IotsaBLEDimmerMod::getHandler(const char *path, JsonObject& reply) {
  reply["dimmer1"] = nameDimmer1;
  reply["isOn1"] = isOn1;
  reply["level1"] = level1;
  reply["minLevel1"] = minLevel1;
#ifdef WITH_SECOND_DIMMER
  reply["dimmer2"] = nameDimmer2;
  reply["isOn2"] = isOn2;
  reply["level2"] = level2;
  reply["minLevel2"] = minLevel2;
#endif
  return true;
}

bool IotsaBLEDimmerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("dimmer1")) {
    String value = reqObj["dimmer1"].as<String>();
    if (value != nameDimmer1) {
      if (nameDimmer1) bleClientMod.delDevice(nameDimmer1);
      nameDimmer1 = value;
      if (value) bleClientMod.addDevice(nameDimmer1);
      anyChanged = true;
    }
  }
  if (reqObj.containsKey("isOn1")) {
    isOn1 = reqObj["isOn1"];
    anyChanged = true;
  }
  if (reqObj.containsKey("level1")) {
    level1 = reqObj["level1"];
    anyChanged = true;
  }
  if (reqObj.containsKey("minLevel1")) {
    minLevel1 = reqObj["minLevel1"];
    anyChanged = true;
  }
#ifdef WITH_SECOND_DIMMER
  if (reqObj.containsKey("dimmer2")) {
    String value = reqObj["dimmer2"].as<String>();
    if (value != nameDimmer2) {
      if (nameDimmer2) bleClientMod.delDevice(nameDimmer2);
      nameDimmer2 = value;
      if (value) bleClientMod.addDevice(nameDimmer2);
      anyChanged = true;
    }
  }
  if (reqObj.containsKey("isOn2")) {
    isOn2 = reqObj["isOn2"];
    anyChanged = true;
  }
  if (reqObj.containsKey("level2")) {
    level2 = reqObj["level2"];
    anyChanged = true;
  }
  if (reqObj.containsKey("minLevel2")) {
    minLevel2 = reqObj["minLevel2"];
    anyChanged = true;
  }
#endif
  if (anyChanged) {
    configSave();
    // xxxjack send changes to dimmers
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
  int value;
  cf.get("dimmer1", nameDimmer1, "");
  cf.get("isOn1", value, 0);
  isOn1 = value;
  cf.get("level1", level1, 0.0);
  cf.get("minLevel1", minLevel1, 0.1);
#ifdef WITH_SECOND_DIMMER
  cf.get("dimmer2", nameDimmer2, "");
  cf.get("isOn2", value, 0);
  isOn2 = value;
  cf.get("level2", level2, 0.0);
  cf.get("minLevel2", minLevel2, 0.1);
#endif // WITH_SECOND_DIMMER
}

void IotsaBLEDimmerMod::configSave() {
  IotsaConfigFileSave cf("/config/bledimmer.cfg");

  cf.put("dimmer1", nameDimmer1);
  cf.put("level1", level1);
  cf.put("isOn1", isOn1);
  cf.put("minLevel1", minLevel1);
#ifdef WITH_SECOND_DIMMER
  cf.put("dimmer2", nameDimmer2);
  cf.put("level2", level2);
  cf.put("isOn2", isOn2);
  cf.put("minLevel2", minLevel2);
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
  if (nameDimmer1) bleClientMod.addDevice(nameDimmer1);
  encoder1.setCallback(std::bind(&IotsaBLEDimmerMod::level1Changed, this));
  // Bind up/down buttons to variable illum, ranging from minLevel to 1.0 in 25 steps
  encoder1.bindVar(level1, minLevel1, 1.0, 0.02);
  encoder1.bindStateVar(isOn1);
  encoder1.setStateCallback(std::bind(&IotsaBLEDimmerMod::touched1OnOff, this));
#ifdef WITH_SECOND_DIMMER
  if (nameDimmer2) bleClientMod.addDevice(nameDimmer2);
  encoder2.setCallback(std::bind(&IotsaBLEDimmerMod::level2Changed, this));
  // Bind up/down buttons to variable illum, ranging from minLevel to 1.0 in 25 steps
  encoder2.bindVar(level2, minLevel2, 1.0, 0.02);
  encoder2.bindStateVar(isOn2);
  encoder2.setStateCallback(std::bind(&IotsaBLEDimmerMod::touched2OnOff, this));
#endif // WITH_SECOND_DIMMER

  auto callback = std::bind(&IotsaBLEDimmerMod::deviceFound, this, std::placeholders::_1);
  bleClientMod.setDeviceFoundCallback(callback);
  bleClientMod.setServiceFilter(ledstripServiceUUID);
}

void IotsaBLEDimmerMod::deviceFound(BLEAdvertisedDevice& device) {
  IFDEBUG IotsaSerial.printf("Found iotsaLedstrip/iotsaDimmer %s\n", device.getName().c_str());
  // update the connection information, or add to unknown dimmers if not known
  if (!bleClientMod.deviceSeen(device.getName(), device)) {
    unknownDimmers.insert(device.getName());
  }
}

void IotsaBLEDimmerMod::loop() {
  // See if we have a value to save (because the user has been turning the dimmer)
  if (saveAtMillis > 0 && millis() > saveAtMillis) {
    saveAtMillis = 0;
    configSave();
  }
#ifdef LED_PIN
  if (ledOffUntilMillis > 0 && millis() > ledOffUntilMillis) {
    digitalWrite(LED_PIN, LOW);
    ledOffUntilMillis = 0;
  }
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
