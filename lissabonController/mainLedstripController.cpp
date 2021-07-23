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
// So, connect E and C to GND, D to GPIO0, A to GPIO4, B to GPIO2
Button button(0, true, false, true);
RotaryEncoder encoder(4, 2);

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
  void unknownDeviceFound(BLEAdvertisedDevice& device);
private:
  void uiButtonChanged();
  void dimmerValueChanged();
  void handler();
  bool buttonPress();
  bool encoderChanged();
  void updateDisplay();
  void startScanUnknown();
  uint32_t scanUnknownUntilMillis = 0;
  typedef std::pair<std::string, BLEAddress> unknownDimmerInfo;
  std::set<unknownDimmerInfo> unknownDimmers;
  DimmerDynamicCollection knownDimmers;
  DimmerDynamicCollection::ItemType* dimmerFactory(int num);
};

void 
IotsaLedstripControllerMod::updateDisplay() {
  IotsaSerial.print(knownDimmers.size());
  IotsaSerial.println(" strips:");

  display->clearStrips();
  int index = 0;
  for (auto& elem : knownDimmers) {
    index++;
    String name = elem->getUserVisibleName();
    IotsaSerial.printf("device %s, available=%d\n", name.c_str(), elem->available());
    display->addStrip(index, name, elem->available());
  }
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
  bool changed = false;
  String error;
  if (server->hasArg("scanUnknown")) startScanUnknown();
  if (server->hasArg("add")) {
    String newDimmerName = server->arg("add");
    if (newDimmerName != "" && knownDimmers.find(newDimmerName) == nullptr) {
      knownDimmers.push_back_new(newDimmerName);
      changed = true;
    } else {
      error = "Bad dimmer name";
    }
  }
  if (server->hasArg("set")) {
    for (auto it: knownDimmers) {
      String dimmerName(it->getUserVisibleName());
      changed |= it->formHandler_args(server, dimmerName, true);
    }
  }
  if (changed) configSave();
  
  String message = "<html><head><title>Lissabon Controller</title></head><body><h1>Lissabon Controller</h1>";
  if (error != "") {
    message += "<p><em>Error: " + error + "</em></p>";
  }
  for(auto it: knownDimmers) {
    String name(it->getUserVisibleName());
    message += "<h2>BLE dimmer " + name + "</h2>";
    message += "<form method='POST'>";
    it->formHandler_fields(message, name, name, true);
    message += "<input type='submit' name='set' value='Submit'></form>";
  }
  message += "<h2>Available Unknown/new BLE dimmer devices</h2>";
  message += "<form><input type='submit' name='scanUnknown' value='Scan for 20 seconds'></form>";
  message += "<form><input type='submit' name='refresh' value='Refresh'></form>";
  if (unknownDimmers.size() == 0) {
    message += "<p>No unassigned BLE dimmer devices seen recently.</p>";
  } else {
    message += "<ul>";
    for (auto it: unknownDimmers) {
      message += "<li>" + String(it.first.c_str()) + " " + it.second.toString().c_str();
      if (it.first != "") {
        message += "<form><input type='hidden' name='add' value='" + String(it.first.c_str()) + "'><input type='submit' value='Add'></form>";
      } else {
        message += "<em>nameless dimmer (cannot be added)</em>";
      }
      message += "</li>";
    }
    message += "</ul>";
  }
  message += "<h2>Add BLE dimmer manually</h2>";
  message += "<form>BLE name: <input name='add'><input type='submit' value='Add'></form>";
  message += "</body></html>";
  server->send(200, "text/html", message);
}

String IotsaLedstripControllerMod::info() {
  String message = "<p>See <a href='/blecontroller'>/blecontroller</a> to change settings or <a href='/api/blecontroller'>/api/blecontroller</a> for REST API.<br>";

  return message;
}
#endif // IOTSA_WITH_WEB

bool IotsaLedstripControllerMod::getHandler(const char *path, JsonObject& reply) {
  IotsaBLEClientMod::getHandler(path, reply);
  knownDimmers.getHandler(reply);
  return true;
}

bool IotsaLedstripControllerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  anyChanged = IotsaBLEClientMod::putHandler(path, request, reply);
  anyChanged |= knownDimmers.putHandler(request);
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
  knownDimmers.configLoad(cf, "");
}

void IotsaLedstripControllerMod::configSave() {
  IotsaConfigFileSave cf("/config/bledimmer.cfg");
  IotsaSerial.println("xxxjack save blecontroller config");
  knownDimmers.configSave(cf, "");
}

void IotsaLedstripControllerMod::setup() {
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  knownDimmers.setFactory(std::bind(&IotsaLedstripControllerMod::dimmerFactory, this, std::placeholders::_1));
  _setupDisplay();
  button.setCallback(std::bind(&IotsaLedstripControllerMod::buttonPress, this));
  encoder.setCallback(std::bind(&IotsaLedstripControllerMod::encoderChanged, this));
  auto callback = std::bind(&IotsaLedstripControllerMod::unknownDeviceFound, this, std::placeholders::_1);
  setUnknownDeviceFoundCallback(callback);
  setDuplicateNameFilter(true);
  setServiceFilter(Lissabon::Dimmer::serviceUUID);
}

void IotsaLedstripControllerMod::_setupDisplay() {
  if (display == NULL) display = new Display();
  updateDisplay();
}

void IotsaLedstripControllerMod::startScanUnknown() {
  findUnknownDevices(true);
  scanUnknownUntilMillis = millis() + 20000;
  iotsaConfig.postponeSleep(21000);
}


void IotsaLedstripControllerMod::unknownDeviceFound(BLEAdvertisedDevice& deviceAdvertisement) {
  IFDEBUG IotsaSerial.printf("unknownDeviceFound: iotsaLedstrip/iotsaDimmer \"%s\"\n", deviceAdvertisement.getName().c_str());
  deviceAdvertisement.getAddress();
  unknownDimmerInfo info(deviceAdvertisement.getName(), deviceAdvertisement.getAddress());
  unknownDimmers.insert(info);
}

void IotsaLedstripControllerMod::loop() {
  if (scanUnknownUntilMillis != 0 && millis() > scanUnknownUntilMillis) {
    findUnknownDevices(false);
    scanUnknownUntilMillis = 0;
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

