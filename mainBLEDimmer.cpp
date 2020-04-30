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

#include "display.h"
Display *display;

// CHANGE: Add application includes and declarations here

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

IotsaApplication application("Iotsa LEDstrip Controller");
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
Button button(0, true, false, true);
RotaryEncoder encoder(4, 2);

Input* inputs[] = {
  &touchpad12,
  &touchpad13,
  &touchpad14,
  &touchpad15,
  &button,
  &encoder
};

IotsaInputMod touchMod(application, inputs, sizeof(inputs)/sizeof(inputs[0]));

#include "iotsaBLEClient.h"
IotsaBLEClientMod bleClientMod(application);

// UUID of service advertised by iotsaLedstrip devices
BLEUUID ledstripServiceUUID("153C0001-D28E-40B8-84EB-7F64B56D4E2E");

//
// LED Lighting control module. 
//

class IotsaLedstripControllerMod : public IotsaApiMod {
public:
  using IotsaApiMod::IotsaApiMod;
  void setup();
  void serverSetup();
  String info();
  void configLoad();
  void configSave();
  void loop();

protected:
  void _setupDisplay();
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  void deviceFound(BLEAdvertisedDevice& device);
private:
  void handler();
  bool touch12();
  bool touch13();
  bool touch14();
  bool touch15();
  bool buttonPress();
  bool encoderChanged();
  void updateDisplay();
};

void 
IotsaLedstripControllerMod::updateDisplay() {
  IotsaSerial.print(bleClientMod.devices.size());
  IotsaSerial.println(" strips:");

  display->clearStrips();
  int index = 0;
  for (auto& elem : bleClientMod.devices) {
    index++;
    std::string name = elem.first;
    IotsaBLEClientConnection* conn = elem.second;
    IotsaSerial.printf("device %s, available=%d\n", name.c_str(), conn->available());
    display->addStrip(index, name, conn->available());
  }
  display->show();
}

bool
IotsaLedstripControllerMod::touch12() {
  IFDEBUG IotsaSerial.println("touch12()");
  updateDisplay();
  return true;
}

bool
IotsaLedstripControllerMod::touch13() {
  IFDEBUG IotsaSerial.println("touch13()");
  updateDisplay();
  return true;
}

bool
IotsaLedstripControllerMod::touch14() {
  IFDEBUG IotsaSerial.println("touch14()");
  updateDisplay();
  return true;
}

bool
IotsaLedstripControllerMod::touch15() {
  IFDEBUG IotsaSerial.println("touch15()");
  updateDisplay();
  return true;
}

bool
IotsaLedstripControllerMod::buttonPress() {
  IFDEBUG IotsaSerial.println("buttonPress()");
  iotsaConfig.postponeSleep(4000);
  return true;
}

bool
IotsaLedstripControllerMod::encoderChanged() {
  IFDEBUG IotsaSerial.println("encoderChanged()");
  iotsaConfig.postponeSleep(4000);
  return true;
}

#ifdef IOTSA_WITH_WEB
void
IotsaLedstripControllerMod::handler() {
}

String IotsaLedstripControllerMod::info() {
  return "";
}
#endif // IOTSA_WITH_WEB

bool IotsaLedstripControllerMod::getHandler(const char *path, JsonObject& reply) {
  return true;
}

bool IotsaLedstripControllerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  return true;
}

void IotsaLedstripControllerMod::serverSetup() {
}


void IotsaLedstripControllerMod::configLoad() {
}

void IotsaLedstripControllerMod::configSave() {
}

void IotsaLedstripControllerMod::setup() {
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  _setupDisplay();
  touchpad12.setCallback(std::bind(&IotsaLedstripControllerMod::touch12, this));
  touchpad13.setCallback(std::bind(&IotsaLedstripControllerMod::touch13, this));
  touchpad14.setCallback(std::bind(&IotsaLedstripControllerMod::touch14, this));
  touchpad15.setCallback(std::bind(&IotsaLedstripControllerMod::touch15, this));
  button.setCallback(std::bind(&IotsaLedstripControllerMod::buttonPress, this));
  encoder.setCallback(std::bind(&IotsaLedstripControllerMod::encoderChanged, this));
  auto callback = std::bind(&IotsaLedstripControllerMod::deviceFound, this, std::placeholders::_1);
  bleClientMod.setDeviceFoundCallback(callback);
  bleClientMod.setServiceFilter(ledstripServiceUUID);
}

void IotsaLedstripControllerMod::_setupDisplay() {
  if (display == NULL) display = new Display();
  updateDisplay();
}

void IotsaLedstripControllerMod::deviceFound(BLEAdvertisedDevice& device) {
  IFDEBUG IotsaSerial.printf("Found iotsaLedstrip %s\n", device.getName().c_str());
  // Add the device, or update the connection information
  if (bleClientMod.addDevice(device.getName(), device)) {
    updateDisplay();
  }
}

void IotsaLedstripControllerMod::loop() {

}

// Instantiate the Led module, and install it in the framework
IotsaLedstripControllerMod ledstripControllerMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}

