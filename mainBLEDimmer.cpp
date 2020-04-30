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


// CHANGE: Add application includes and declarations here

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.
#define LED_PIN 22  // Define to turn on the LED when powered and not sleeping.


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
Touchpad touch2down(14, true, false, true);
Touchpad touch2up(15, true, false, true);
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
  void handler();
  bool touched1OnOff();
  bool level1Changed();
  void updateDimmer1();
  bool isOn1;
  float level1;
  float minLevel1;
#ifdef WITH_SECOND_DIMMER
  bool touched2OnOff();
  bool level2Changed();
  void updateDimmer2();
  bool isOn2;
  float level2;
  float minLevel2;
#endif
  uint32_t saveAtMillis = 0;
};

bool
IotsaBLEDimmerMod::touched1OnOff() {
  IFDEBUG IotsaSerial.printf("touched1OnOff: onOff=%d level=%f\n", isOn1, level1);
  return true;
}

bool
IotsaBLEDimmerMod::level1Changed() {
  IFDEBUG IotsaSerial.printf("level1Changed: onOff=%d level=%f\n", isOn1, level1);
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
  return true;
}

bool
IotsaBLEDimmerMod::level2Changed() {
  IFDEBUG IotsaSerial.printf("level2Changed: onOff=%d level=%f\n", isOn2, level2);
  return true;
}

void IotsaBLEDimmerMod::updateDimmer2() {
  // xxxjack send onOff and level to dimmer
  iotsaConfig.postponeSleep(2100);
  saveAtMillis = millis() + 2000;
}
#endif // WITH_SECOND_DIMMER

#ifdef IOTSA_WITH_WEB
void
IotsaBLEDimmerMod::handler() {
}

String IotsaBLEDimmerMod::info() {
  return "";
}
#endif // IOTSA_WITH_WEB

bool IotsaBLEDimmerMod::getHandler(const char *path, JsonObject& reply) {
  return true;
}

bool IotsaBLEDimmerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  return true;
}

void IotsaBLEDimmerMod::serverSetup() {
}


void IotsaBLEDimmerMod::configLoad() {
  IotsaConfigFileLoad cf("/config/bledimmer.cfg");
  int value;
  cf.get("isOn1", value, 0);
  isOn1 = value;
  cf.get("level1", level1, 0.0);
  cf.get("minLevel1", minLevel1, 0.1);
#ifdef WITH_SECOND_DIMMER
  cf.get("isOn2", value, 0);
  isOn2 = value;
  cf.get("level2", level2, 0.0);
  cf.get("minLevel2", minLevel2, 0.1);
#endif // WITH_SECOND_DIMMER
}

void IotsaBLEDimmerMod::configSave() {
  IotsaConfigFileSave cf("/config/bledimmer.cfg");

  cf.put("level1", level1);
  cf.put("isOn1", isOn1);
  cf.put("minLevel1", minLevel1);
#ifdef WITH_SECOND_DIMMER
  cf.put("level2", level2);
  cf.put("isOn2", isOn2);
  cf.put("minLevel2", minLevel2);
#endif // WITH_SECOND_DIMMER
}

void IotsaBLEDimmerMod::setup() {
#ifdef LED_PIN
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
#endif
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  encoder1.setCallback(std::bind(&IotsaBLEDimmerMod::level1Changed, this));
  // Bind up/down buttons to variable illum, ranging from minLevel to 1.0 in 25 steps
  encoder1.bindVar(level1, minLevel1, 1.0, 0.02);
  encoder1.bindStateVar(isOn1);
  encoder1.setStateCallback(std::bind(&IotsaBLEDimmerMod::touched1OnOff, this));
#ifdef WITH_SECOND_DIMMER
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
  // Add the device, or update the connection information
  if (bleClientMod.addDevice(device.getName(), device)) {
  }
}

void IotsaBLEDimmerMod::loop() {
  // See if we have a value to save (because the user has been turning the dimmer)
  if (saveAtMillis > 0 && millis() > saveAtMillis) {
    saveAtMillis = 0;
    configSave();
  }

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
