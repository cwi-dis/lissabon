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

//
// Device can be rebooted or configuration mode can be requested by quickly tapping any button.
// TAP_DURATION sets the maximum time between press-release-press-etc.
// Note that the controller records both press and release, so we need to double the count.
#define TAP_COUNT_MODE_CHANGE 6
#define TAP_COUNT_REBOOT 12
#define TAP_DURATION 1000

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

#define LOG_BLE if(0)
#define LOG_UI if(1)

IotsaApplication application("Iotsa LEDstrip Controller");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

#include "iotsaBLEServer.h"
#ifdef IOTSA_WITH_BLE
IotsaBLEServerMod bleserverMod(application);
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
RotaryEncoder encoder(14, 2);
#define ENCODER_STEPS 20
#ifdef DIMMER_WITH_TEMPERATURE
#define COLOR_TEMP_MIN 2200
#define COLOR_TEMP_MAX 6500
#endif
Button button(0, true, true, true);
#define SHORT_PRESS_DURATION 500
Button rockerUp(12, true, false, true);
Button rockerDown(13, true, false, true);

Input* inputs[] = {
  &button,
  &encoder,
  &rockerUp,
  &rockerDown,
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
  void _tap();
  void dimmerOnOffChanged();
  void dimmerValueChanged();
  void dimmerAvailableChanged(bool available, bool connected);
  void handler();
  bool uiRockerPressed();
  bool uiButtonPressed();
  bool uiEncoderChanged();
  DimmerDynamicCollection::ItemType* getDimmerForCommand(int num);
  void updateDisplay(bool clear);
  uint32_t scanUnknownUntilMillis = 0;
  typedef std::pair<std::string, BLEAddress> unknownDimmerInfo;
  DimmerDynamicCollection dimmers;
  DimmerDynamicCollection::ItemType* dimmerFactory(int num);
  int selectedDimmerIndex = 0; // currently selected dimmer on display
  Display::DisplayMode selectedMode = Display::DisplayMode::dm_select;
  uint32_t lastButtonChangeMillis = 0;
  int buttonChangeCount = 0;
};

void IotsaLedstripControllerMod::_tap() {
  // Called whenever any button changed state.
  // Used to give visual feedback (led turning off) on presses and releases,
  // and to enable config mod after 4 taps and reboot after 8 taps
  uint32_t now = millis();
  if (now >= lastButtonChangeMillis + TAP_DURATION) {
    // Either the first change, or too late. Reset.
    if (lastButtonChangeMillis > 0) {
      IotsaSerial.println("TapCount: reset");
    }
    lastButtonChangeMillis = millis();
    buttonChangeCount = 0;
  }
  if (lastButtonChangeMillis > 0 && now < lastButtonChangeMillis + TAP_DURATION) {
    // A button change that was quick enough for a tap
    lastButtonChangeMillis = now;
    buttonChangeCount++;
    IFDEBUG IotsaSerial.printf("TapCount: %d\n", buttonChangeCount);
    if (buttonChangeCount == TAP_COUNT_MODE_CHANGE) {
      IotsaSerial.println("TapCount: mode change");
      iotsaConfig.allowRequestedConfigurationMode();
    }
    if (buttonChangeCount == TAP_COUNT_REBOOT) {
      IotsaSerial.println("TapCount: reboot");
      iotsaConfig.requestReboot(1000);
    }
  }
}

void 
IotsaLedstripControllerMod::updateDisplay(bool clear) {
  LOG_BLE IotsaSerial.printf("LissabonController: %d strips:\n", dimmers.size());

  if (true || clear) display->clearStrips();
  display->selectMode(selectedMode);
  int index = 0;
  for (auto& elem : dimmers) {
    String name = elem->getUserVisibleName();
    LOG_BLE IotsaSerial.printf("  device %s, available=%d\n", name.c_str(), elem->available());
    display->addStrip(index, name, elem->available());
    index++;
  }
  display->selectStrip(selectedDimmerIndex);
  if (selectedDimmerIndex >= 0) {
    auto d = dimmers.at(selectedDimmerIndex);
    if (d && d->available() && d->dataValid()) {
      display->setLevel(d->level, d->isOn);
    } else {
      display->clearLevel();
    }
    // temperature not implemented yet
    display->clearTemp();
  }
  display->show();
}

DimmerDynamicCollection::ItemType* 
IotsaLedstripControllerMod::getDimmerForCommand(int num) {
  if (num < 0) {
    IotsaSerial.println("LissabonController: No dimmer selected");
    display->flash();
    return nullptr;
  }
  auto d = dimmers.at(num);  
  if (d == nullptr) {
    IotsaSerial.printf("LissabonController: Dimmer %d does not exist\n", num);
    display->flash();
    return nullptr;
  }
  if (!d->available()) {
    IotsaSerial.printf("LissabonController: Dimmer %d unavailable\n", num);
    display->flash();
    return nullptr;
  }
  if (!d->dataValid()) {
    IotsaSerial.printf("LissabonController: Dimmer %d current status unknown\n", num);
    display->flash();
    return nullptr;
  }
  return d;
}

bool
IotsaLedstripControllerMod::uiRockerPressed() {
  LOG_UI IotsaSerial.printf("LissabonController: uiRockerPressed: up: state=%d repeatCount=%d duration=%d\n", rockerUp.pressed, rockerUp.repeatCount, rockerUp.duration);
  LOG_UI IotsaSerial.printf("LissabonController: uiRockerPressed: down: state=%d repeatCount=%d duration=%d\n", rockerDown.pressed, rockerDown.repeatCount, rockerDown.duration);
  // The encoder controls the selected dimmer
  if (rockerUp.pressed) selectedDimmerIndex++;
  if (rockerDown.pressed) selectedDimmerIndex--;
  if (selectedDimmerIndex < 0) {
    display->flash();
    selectedDimmerIndex = 0;
  }
  if (selectedDimmerIndex >= dimmers.size()) {
    display->flash();
    selectedDimmerIndex = dimmers.size()-1;
  }
  LOG_UI IotsaSerial.printf("LissabonController: now selectedDimmer=%d\n", selectedDimmerIndex);
  updateDisplay(false);
  iotsaConfig.postponeSleep(4000);
  return true;
}

bool
IotsaLedstripControllerMod::uiButtonPressed() {
  LOG_UI IotsaSerial.printf("LissabonController: uiButtonPressed: state=%d repeatCount=%d duration=%d\n", button.pressed, button.repeatCount, button.duration);
  _tap();
  //iotsaConfig.postponeSleep(4000);
  auto d = dimmers.at(selectedDimmerIndex);
  if (d == nullptr) return false;
#ifdef DIMMER_WITH_TEMPERATURE
  if (button.pressed) {
    float t_value = (d->temperature - COLOR_TEMP_MIN) / (COLOR_TEMP_MAX - TCOLOR_EMP_MIN);
    if (t_value < 0) t_value = 0;
    if (t_value > 1) t_value = 1;
    encoder.value = (int)(f_value * ENCODER_STEPS);
  } else
#endif // DIMMER_WITH_TEMPERATURE
  {
    float f_value = d == nullptr ? 0 : d->level;
    encoder.value = (int)(f_value * ENCODER_STEPS);
  }
  if (button.duration < SHORT_PRESS_DURATION) {
    // Short press: turn current dimmer on or off
    LOG_UI IotsaSerial.println("LissabonCOntroller: uiButtonPressed: dimmer on/off");
    auto d = getDimmerForCommand(selectedDimmerIndex);
    if (d != nullptr) {
      d->isOn = !d->isOn;
      d->updateDimmer();
      updateDisplay(false);
    }
  } else {
    // Long press: assume rotary will be used for selecting temperature
  }
  return true;
}

bool
IotsaLedstripControllerMod::uiEncoderChanged() {
  LOG_UI IotsaSerial.printf("LissabonController: uiEncoderChanged()\n");
  auto d = getDimmerForCommand(selectedDimmerIndex);
  if (d == nullptr) {
    // getDimmerForCommand has already given the error message
    return true;
  }
#ifdef DIMMER_WITH_TEMPERATURE
  if (button.pressed) {
    if (encoder.value < 0) encoder.value = 0;
    if (encoder.value >= ENCODER_STEPS) encoder.value = ENCODER_STEPS;
    float f_value = (float)encoder.value / ENCODER_STEPS;
    if (f_value > 1.0) {
      f_value = 1.0;
      encoder.value = ENCODER_STEPS;
    }
    float t_value = COLOR_TEMP_MIN + f_value * (COLOR_TEMP_MAX - COLOR_TEMP_MIN);
    LOG_UI IotsaSerial.printf("LissabonController: now temperature=%f (int %d)\n", t_value, encoder.value);
    d->temperature = t_value;
  } else 
#endif
  {
    // Button not pressed, encoder controls level.
    if (encoder.value < 0) encoder.value = 0;
    if (encoder.value >= ENCODER_STEPS) encoder.value = ENCODER_STEPS;
    float f_value = (float)encoder.value / ENCODER_STEPS;
    if (f_value > 1.0) {
      f_value = 1.0;
      encoder.value = ENCODER_STEPS;
    }
    LOG_UI IotsaSerial.printf("LissabonController: now level=%f (int %d)\n", f_value, encoder.value);
    d->level = f_value;
  }
  d->updateDimmer();
  LOG_UI IotsaSerial.printf("LissabonController: updated dimmer %d\n", selectedDimmerIndex);
  iotsaConfig.postponeSleep(4000);
  return true;
}


void IotsaLedstripControllerMod::dimmerAvailableChanged(bool available, bool connected) {
  LOG_UI IotsaSerial.println("LissabonController: dimmerAvailableChanged()");
  updateDisplay(false);
  display->showActivity(connected);
}

void IotsaLedstripControllerMod::dimmerOnOffChanged() {
#if 1
  LOG_UI IotsaSerial.println("LissabonController: dimmerOnOfChanged()");
  updateDisplay(false);
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
  LOG_UI IotsaSerial.println("LissabonController: dimmerValueChanged()");
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
  rv += "<form method='post'><input type='hidden' name='add' value='" + deviceName + "'><input type='submit' value='Add'></form>";
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
  IotsaSerial.println("LissabonController: save blecontroller config");
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
  button.setCallback(std::bind(&IotsaLedstripControllerMod::uiButtonPressed, this));
  encoder.setCallback(std::bind(&IotsaLedstripControllerMod::uiEncoderChanged, this));

  rockerUp.setCallback(std::bind(&IotsaLedstripControllerMod::uiRockerPressed, this));
  rockerDown.setCallback(std::bind(&IotsaLedstripControllerMod::uiRockerPressed, this));
  rockerUp.setRepeat(500,200);
  rockerDown.setRepeat(500,200);
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
  LOG_BLE IotsaSerial.printf("LissabonController: unknownDeviceFound: device \"%s\"\n", deviceAdvertisement.getName().c_str());
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

