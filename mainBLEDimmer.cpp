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
Touchpad touchpad12(12, true, false, true);
Touchpad touchpad13(13, true, false, true);
Touchpad touchpad14(14, true, false, true);
Touchpad touchpad15(15, true, false, true);

Input* inputs[] = {
  &touchpad12,
  &touchpad13,
  &touchpad14,
  &touchpad15
};

IotsaInputMod touchMod(application, inputs, sizeof(inputs)/sizeof(inputs[0]));

#include "iotsaBLEClient.h"
IotsaBLEClientMod bleClientMod(application);

// UUID of service advertised by iotsaLedstrip and iotsaDimmer devices
BLEUUID ledstripServiceUUID("153C0001-D28E-40B8-84EB-7F64B56D4E2E");

//
// LED Lighting control module. 
//

class iotsaBLEDimmerMod : public IotsaApiMod {
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
  bool touch12();
  bool touch13();
  bool touch14();
  bool touch15();
};

bool
iotsaBLEDimmerMod::touch12() {
  IFDEBUG IotsaSerial.println("touch12()");
  return true;
}

bool
iotsaBLEDimmerMod::touch13() {
  IFDEBUG IotsaSerial.println("touch13()");
  return true;
}

bool
iotsaBLEDimmerMod::touch14() {
  IFDEBUG IotsaSerial.println("touch14()");
  return true;
}

bool
iotsaBLEDimmerMod::touch15() {
  IFDEBUG IotsaSerial.println("touch15()");
  return true;
}

#ifdef IOTSA_WITH_WEB
void
iotsaBLEDimmerMod::handler() {
}

String iotsaBLEDimmerMod::info() {
  return "";
}
#endif // IOTSA_WITH_WEB

bool iotsaBLEDimmerMod::getHandler(const char *path, JsonObject& reply) {
  return true;
}

bool iotsaBLEDimmerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  return true;
}

void iotsaBLEDimmerMod::serverSetup() {
}


void iotsaBLEDimmerMod::configLoad() {
}

void iotsaBLEDimmerMod::configSave() {
}

void iotsaBLEDimmerMod::setup() {
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  touchpad12.setCallback(std::bind(&iotsaBLEDimmerMod::touch12, this));
  touchpad13.setCallback(std::bind(&iotsaBLEDimmerMod::touch13, this));
  touchpad14.setCallback(std::bind(&iotsaBLEDimmerMod::touch14, this));
  touchpad15.setCallback(std::bind(&iotsaBLEDimmerMod::touch15, this));
  auto callback = std::bind(&iotsaBLEDimmerMod::deviceFound, this, std::placeholders::_1);
  bleClientMod.setDeviceFoundCallback(callback);
  bleClientMod.setServiceFilter(ledstripServiceUUID);
}

void iotsaBLEDimmerMod::deviceFound(BLEAdvertisedDevice& device) {
  IFDEBUG IotsaSerial.printf("Found iotsaLedstrip/iotsaDimmer %s\n", device.getName().c_str());
  // Add the device, or update the connection information
  if (bleClientMod.addDevice(device.getName(), device)) {
  }
}

void iotsaBLEDimmerMod::loop() {

}

// Instantiate the Led module, and install it in the framework
iotsaBLEDimmerMod bleDimmerControllerMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
