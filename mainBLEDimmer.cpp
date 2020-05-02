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
  bool needTransmit1;
#ifdef WITH_SECOND_DIMMER
  bool touched2OnOff();
  bool level2Changed();
  void updateDimmer2();
  bool isOn2;
  float level2;
  float minLevel2;
  String nameDimmer2;
  bool needTransmit2;
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
  needTransmit1 = true;
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
  needTransmit2 = true;
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
  bool dimmerChanged = false;
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
    int val = server->arg("isOn1").toInt();
    if (val != isOn1) {
      isOn1 = val;
      dimmerChanged = true;
      anyChanged = true;
    }
  }
  if (server->hasArg("level1")) {
    float val = server->arg("level1").toFloat();
    if (val != level1) {
      level1 = val;
      dimmerChanged = true;
      anyChanged = true;
    }
  }
  if (server->hasArg("minLevel1")) {
    float val = server->arg("minLevel1").toFloat();
    if (val != minLevel1) {
      minLevel1 = val;
      dimmerChanged = true;
      anyChanged = true;
    }
  }
  if (dimmerChanged) {
    updateDimmer1();
  }
#ifdef WITH_SECOND_DIMMER
  dimmerChanged = false;
  if (server->hasArg("dimmer2")) {
    String value = server->arg("dimmer2");
    if (value != nameDimmer2) {
      if (nameDimmer2) bleClientMod.delDevice(nameDimmer2);
      nameDimmer2 = value;
      if (value) bleClientMod.addDevice(nameDimmer2);
      anyChanged = true;
    }
  }
  if (server->hasArg("isOn2")) {
    int val = server->arg("isOn2").toInt();
    if (val != isOn2) {
      isOn2 = val;
      dimmerChanged = true;
      anyChanged = true;
    }
  }
  if (server->hasArg("level2")) {
    float val = server->arg("level2").toFloat();
    if (val != level2) {
      level2 = val;
      dimmerChanged = true;
      anyChanged = true;
    }
  }
  if (server->hasArg("minLevel2")) {
    float val = server->arg("minLevel2").toFloat();
    if (val != minLevel2) {
      minLevel2 = val;
      dimmerChanged = true;
      anyChanged = true;
    }
  }
  if (dimmerChanged) {
    updateDimmer2();
  }
#endif // WITH_SECOND_DIMMER
  if (anyChanged) {
    configSave();
  }
  String message = "<html><head><title>BLE Dimmers</title></head><body><h1>BLE Dimmers</h1>";

  message += "<h2>Dimmer 1 (" + nameDimmer1 + ") operation</h2><form method='get'>";
  IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(nameDimmer1);
  if (dimmer == NULL || !dimmer->available()) message += "<em>(dimmer may be unavailable)</em><br>";
  String checkedOn = isOn1 ? "checked" : "";
  String checkedOff = !isOn1 ? "checked " : "";
  message += "<input type='radio' name='isOn1'" + checkedOff + " value='0'>Off <input type='radio' " + checkedOn + " name='isOn1' value='1'>On</br>";
  message += "Level (0.0..1.0): <input name='level1' value='" + String(level1) + "'></br>";
  message += "<input type='submit'></form>";
#ifdef WITH_SECOND_DIMMER
  message += "<h2>Dimmer 2 (" + nameDimmer2 + ") operation</h2><form method='get'>";
  dimmer = bleClientMod.getDevice(nameDimmer2);
  if (dimmer == NULL || !dimmer->available()) message += "<em>(dimmer may be unavailable)</em><br>";
  checkedOn = isOn2 ? "checked" : "";
  checkedOff = !isOn2 ? "checked" : "";
  message += "<input type='radio' name='isOn2'" + checkedOff + " value='0'>Off <input type='radio' " + checkedOn + " name='isOn2' value='1'>On</br>";
  message += "Level (0.0..1.0): <input name='level2' value='" + String(level2) + "'></br>";
  message += "<input type='submit'></form>";
#endif // WITH_SECOND_DIMMER
  message += "<h2>Dimmer 1 configuration</h2><form method='get'>";
  message += "BLE name: <input name='dimmer1' value='" + nameDimmer1 + "'><br>";
  message += "Min Level (0.0..1.0): <input name='minLevel1' value='" + String(minLevel1) + "'></br>";
  message += "<input type='submit'></form>";
#ifdef WITH_SECOND_DIMMER
  message += "<h2>Dimmer 2 configuration</h2><form method='get'>";
  message += "BLE name: <input name='dimmer2' value='" + nameDimmer2 + "'><br>";
  message += "Min Level (0.0..1.0): <input name='minLevel2' value='" + String(minLevel2) + "'></br>";
  message += "<input type='submit'></form>";
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
  message += "Dimmer1: ";
  message += nameDimmer1;
  if (!isOn1) {
    message += " off";
  } else {
    message += " on (" + String(int(level1*100)) + "%)";
  }
  message += "<br>";
#ifdef WITH_SECOND_DIMMER
  message += "Dimmer2: ";
  message += nameDimmer2;
  if (!isOn2) {
    message += " off";
  } else {
    message += " on (" + String(int(level2*100)) + "%)";
  }
  message += "<br>";
#endif // WITH_SECOND_DIMMER
  return message;
}
#endif // IOTSA_WITH_WEB

bool IotsaBLEDimmerMod::getHandler(const char *path, JsonObject& reply) {
  // xxxjack need to distinguish between config and operational parameters
  reply["dimmer1"] = nameDimmer1;
  IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(nameDimmer1);
  reply["available1"] = (bool)(dimmer != NULL && dimmer->available());
  reply["isOn1"] = isOn1;
  reply["level1"] = level1;
  reply["minLevel1"] = minLevel1;
#ifdef WITH_SECOND_DIMMER
  reply["dimmer2"] = nameDimmer2;
  dimmer = bleClientMod.getDevice(nameDimmer2);
  reply["available2"] = (bool)(dimmer != NULL && dimmer->available());
  reply["isOn2"] = isOn2;
  reply["level2"] = level2;
  reply["minLevel2"] = minLevel2;
#endif
  // xxxjack should we return unassigned dimmers?
  return true;
}

bool IotsaBLEDimmerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  // xxxjack need to distinguish between config and operational parameters
  bool anyChanged = false;
  bool dimmerChanged = false;
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
    dimmerChanged = true;
  }
  if (reqObj.containsKey("level1")) {
    level1 = reqObj["level1"];
    anyChanged = true;
    dimmerChanged = true;
  }
  if (reqObj.containsKey("minLevel1")) {
    minLevel1 = reqObj["minLevel1"];
    anyChanged = true;
  }
  if (dimmerChanged) {
    updateDimmer1();
  }
#ifdef WITH_SECOND_DIMMER
  dimmerChanged = false;
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
  if (dimmerChanged) {
    updateDimmer2();
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
  // See whether we have some values to transmit to Dimmer1
  if (needTransmit1) {
    IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(nameDimmer1);
    if (dimmer == NULL) {
      needTransmit1 = false;
    } else if (dimmer->available() && dimmer->connect()) {
      IFDEBUG IotsaSerial.println("Transmit brightness to dimmer1");
      bool ok = dimmer->set(serviceUUID, brightnessUUID, (uint8_t)(level1*100));
      if (!ok) {
        IFDEBUG IotsaSerial.println("BLE: set(brightness) failed");
      }
      IFDEBUG IotsaSerial.println("Transmit ison to dimmer1");
      ok = dimmer->set(serviceUUID, isOnUUID, (uint8_t)isOn1);
      if (!ok) {
        IFDEBUG IotsaSerial.println("BLE: set(isOn) failed");
      }
      IFDEBUG IotsaSerial.println("disconnecting to dimmer1");
      dimmer->disconnect();
      IFDEBUG IotsaSerial.println("disconnected to dimmer1");
      needTransmit1 = false;
    }
  }
#ifdef WITH_SECOND_DIMMER
  // See whether we have some values to transmit to Dimmer2
  if (needTransmit2) {
    IotsaBLEClientConnection *dimmer = bleClientMod.getDevice(nameDimmer2);
    if (dimmer == NULL) {
      needTransmit2 = false;
    } else if (dimmer->available() && dimmer->connect()) {
      IFDEBUG IotsaSerial.println("Transmit brightness to dimmer2");
      bool ok = dimmer->set(serviceUUID, brightnessUUID, (uint8_t)(level2*100));
      if (!ok) {
        IFDEBUG IotsaSerial.println("BLE: set(brightness) failed");
      }
      IFDEBUG IotsaSerial.println("Transmit ison to dimmer2");
      ok = dimmer->set(serviceUUID, isOnUUID, (uint8_t)isOn2);
      if (!ok) {
        IFDEBUG IotsaSerial.println("BLE: set(isOn) failed");
      }
      IFDEBUG IotsaSerial.println("disconnecting to dimmer2");
      dimmer->disconnect();
      IFDEBUG IotsaSerial.println("disconnected to dimmer2");
      needTransmit2 = false;
    }
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
