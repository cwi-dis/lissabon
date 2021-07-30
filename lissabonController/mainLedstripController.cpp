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

#include "DimmerDynamicCollection.h"
using namespace Lissabon;

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
// When using an Alps EC12D rotary encoder with pushbutton here is the pinout:
// When viewed from the top there are pins at northwest, north, northeast, southwest, southeast.
// These pins are named (in Alps terminology) A, E, B, C, D.
// A and B (northwest, northeast) are the rotary encoder pins,
// C is the corresponding ground,
// D and E are the pushbutton pins.
// So, connect E and C to GND, D to GPIO0, A to GPI14, B to GPIO2
Button button(0, true, false, true);
RotaryEncoder encoder(14, 2);

Input* inputs[] = {
  &button,
  &encoder
};

IotsaInputMod touchMod(application, inputs, sizeof(inputs)/sizeof(inputs[0]));

#include "iotsaBLEClient.h"
#include "BLEDimmer.h"

//
// LED Lighting control module. 
//
using namespace Lissabon;

class IotsaLedstripControllerMod : public IotsaBLEClientMod, public DimmerCallbacks {
public:
  using IotsaBLEClientMod::IotsaBLEClientMod;
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
  void unknownBLEDimmerFound(BLEAdvertisedDevice& device);
  virtual String formHandler_field_perdevice(const char *deviceName) override;
private:
  void uiButtonChanged();
  void dimmerValueChanged();
  void handler();
  bool buttonPress();
  bool encoderChanged();
  void updateDisplay(bool clear);
//xxxjack   void startScanUnknown();
  uint32_t scanUnknownUntilMillis = 0;
  typedef std::pair<std::string, BLEAddress> unknownDimmerInfo;
  DimmerDynamicCollection dimmers;
  DimmerDynamicCollection::ItemType* dimmerFactory(int num);
  int selectedIndex = -1; // currently selected dimmer on display
};

void 
IotsaLedstripControllerMod::updateDisplay(bool clear) {
  IotsaSerial.print(dimmers.size());
  IotsaSerial.println(" strips:");

  if (true || clear) display->clearStrips();
  int index = 0;
  for (auto& elem : dimmers) {
    String name = elem->getUserVisibleName();
    IotsaSerial.printf("device %s, available=%d\n", name.c_str(), elem->available());
    display->addStrip(index, name, elem->available());
    index++;
  }
  if (selectedIndex >= dimmers.size()) selectedIndex = dimmers.size()-1;
  if (selectedIndex < 0) selectedIndex = 0;
  encoder.value = selectedIndex;
  display->selectStrip(selectedIndex);
  display->show();
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
  selectedIndex = encoder.value;
  updateDisplay(false);
  iotsaConfig.postponeSleep(4000);
  return true;
}


void IotsaLedstripControllerMod::uiButtonChanged() {
#if 1
  IotsaSerial.println("xxxjack IOButtonChanged()");
#else
  // Called whenever any button changed state.
  // Used to give visual feedback (led turning off) on presses and releases,
  // and to enable config mod after 4 taps and reboot after 8 taps
  uint32_t now = millis();
  ledOn();
  if (lastButtonChangeMillis > 0 && now < lastButtonChangeMillis + TAP_DURATION) {
    // A button change that was quick enough for a tap
    lastButtonChangeMillis = now;
    buttonChangeCount++;
    IFDEBUG IotsaSerial.printf("tap mode count=%d\n", buttonChangeCount);
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
    IFDEBUG IotsaSerial.println("tap mode 0");
  }
#endif
}


void IotsaLedstripControllerMod::dimmerValueChanged() {
#if 1
  IotsaSerial.println("xxxjack dimmerValueChanged()");
  updateDisplay(false);
#else
  saveAtMillis = millis() + 1000;
  ledOn();
#endif
}

DimmerDynamicCollection::ItemType *
IotsaLedstripControllerMod::dimmerFactory(int num) {
  BLEDimmer *newDimmer = new BLEDimmer(num, *this, this);
  newDimmer->followDimmerChanges(true);
  return newDimmer;
}

#ifdef IOTSA_WITH_WEB
void
IotsaLedstripControllerMod::handler() {
  // xxxjack update settings for remotes?
  bool anyChanged = false;
  String error;
  anyChanged |= dimmers.formHandler_args(server, "", true);
  anyChanged |= IotsaBLEClientMod::formHandler_args(server, "", true);
#if 0
  if (server->hasArg("scanUnknown")) startScanUnknown();
#endif
  if (server->hasArg("add")) {
    String newDimmerName = server->arg("add");
    if (newDimmerName != "" && dimmers.find(newDimmerName) == nullptr) {
      dimmers.push_back_new(newDimmerName);
      dimmers.setup();
      anyChanged = true;
    } else {
      error = "Bad dimmer name";
    }
  }
  if (server->hasArg("clearall") && server->arg("iamsure") == "iamsure") {
    dimmers.clear();
    anyChanged = true;
  }
  if (anyChanged) configSave();
  
  String message = "<html><head><title>Lissabon Controller</title></head><body><h1>Lissabon Controller</h1>";
  if (error != "") {
    message += "<p><em>Error: " + error + "</em></p>";
  }
  message += "<h2>Dimmer Settings</h2><form method='post'>";
  dimmers.formHandler_fields(message, "", "", false);
  message += "<input type='submit' name='set' value='Set Dimmers'></form><br>";

  message += "<h2>Dimmer Configurations</h2><form method='post'>";
  dimmers.formHandler_fields(message, "", "", true);
  message += "<input type='submit' name='config' value='Configure Dimmers'></form><br>";

  message += "<br><form method='post'>Remove all: <input type='checkbox' name='iamsure' value='iamsure'>I am sure <input type='submit' name='clearall' value='Remove All'></form><br>";
  message += "<br><form method='post'>Add by name: <input name='add'><input type='submit' name='addbyname' value='Add'></form><br>";

  IotsaBLEClientMod::formHandler_fields(message, "BLE Dimmer", "dimmer", true);

  message += "</body></html>";
  server->send(200, "text/html", message);
}

String IotsaLedstripControllerMod::formHandler_field_perdevice(const char *_deviceName) {
  String deviceName(_deviceName);
  String rv(deviceName);
  rv += "<form><input type='hidden' name='add' value='" + deviceName + "'><input type='submit' value='Add'></form>";
  return rv;
}

String IotsaLedstripControllerMod::info() {
  String message = "<p>See <a href='/blecontroller'>/blecontroller</a> to change settings or <a href='/api/blecontroller'>/api/blecontroller</a> for REST API.<br>";

  return message;
}
#endif // IOTSA_WITH_WEB

bool IotsaLedstripControllerMod::getHandler(const char *path, JsonObject& reply) {
  IotsaBLEClientMod::getHandler(path, reply);
  dimmers.getHandler(reply);
  return true;
}

bool IotsaLedstripControllerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  anyChanged = IotsaBLEClientMod::putHandler(path, request, reply);
  anyChanged |= dimmers.putHandler(request);
  if (anyChanged) {
    configSave();
  }
  return true;
}

void IotsaLedstripControllerMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/blecontroller", std::bind(&IotsaLedstripControllerMod::handler, this));
  server->on("/blecontroller", HTTP_POST, std::bind(&IotsaLedstripControllerMod::handler, this));
#endif
  api.setup("/api/blecontroller", true, true);
  name = "blecontroller";
}

void IotsaLedstripControllerMod::configLoad() {
  IotsaConfigFileLoad cf("/config/blecontroller.cfg");
  dimmers.configLoad(cf, "");
}

void IotsaLedstripControllerMod::configSave() {
  IotsaConfigFileSave cf("/config/blecontroller.cfg");
  IotsaSerial.println("xxxjack save blecontroller config");
  dimmers.configSave(cf, "");
}

void IotsaLedstripControllerMod::setup() {
  //
  // Let our base class do its setup.
  //
  IotsaBLEClientMod::setup();
  dimmers.setFactory(std::bind(&IotsaLedstripControllerMod::dimmerFactory, this, std::placeholders::_1));
  //
  // Load configuration
  //
  configLoad();
 #if 0
  iotsaConfig.allowRCMDescription("tap any touchpad 4 times");
#endif
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  _setupDisplay();
  button.setCallback(std::bind(&IotsaLedstripControllerMod::buttonPress, this));
  encoder.setCallback(std::bind(&IotsaLedstripControllerMod::encoderChanged, this));
  auto callback = std::bind(&IotsaLedstripControllerMod::unknownBLEDimmerFound, this, std::placeholders::_1);
  setUnknownDeviceFoundCallback(callback);
  setDuplicateNameFilter(true);
  setServiceFilter(Lissabon::Dimmer::serviceUUID);
  //
  // Setup dimmers by getting current settings from BLE devices
  // xxxjack move to DimmerCollection
  //
  for (auto d : dimmers) {
    d->setup();
  }
}

void IotsaLedstripControllerMod::_setupDisplay() {
  if (display == NULL) display = new Display();
  updateDisplay(true);
}

#if 0
void IotsaLedstripControllerMod::startScanUnknown() {
  findUnknownDevices(true);
  scanUnknownUntilMillis = millis() + 20000;
  iotsaConfig.postponeSleep(21000);
}
#endif

void IotsaLedstripControllerMod::unknownBLEDimmerFound(BLEAdvertisedDevice& deviceAdvertisement) {
  IFDEBUG IotsaSerial.printf("unknownDeviceFound: iotsaLedstrip/iotsaDimmer \"%s\"\n", deviceAdvertisement.getName().c_str());
  unknownDevices.insert(deviceAdvertisement.getName());
}

void IotsaLedstripControllerMod::loop() {
   //
  // Let our baseclass do its loop-y things
  //
  IotsaBLEClientMod::loop();
#if 0
  //
  // See whether we have a value to save (because the user has been turning the dimmer)
  //
  if (saveAtMillis > 0 && millis() > saveAtMillis) {
    saveAtMillis = 0;
    configSave();
    ledOff();
  }
#endif
  //
  // Let the dimmers do any processing they need to do
  //
  for (auto d : dimmers) {
    d->loop();
  }
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

