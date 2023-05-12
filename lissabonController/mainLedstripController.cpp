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

#include "buttons.h"

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


IotsaInputMod touchMod(application, getInputs(), getInputCount());

#include "iotsaBLEClient.h"
#include "BLEDimmer.h"

//
// LED Lighting control module. 
//
using namespace Lissabon;


class IotsaLedstripControllerMod : public IotsaBLEClientMod, public DimmerCallbacks, public ButtonsCallbacks {
public:
  IotsaLedstripControllerMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : IotsaBLEClientMod(_app, _auth, early),
    buttons(this)
  {}
  void setup();
  void serverSetup();
  String info();
  void configLoad();
  void configSave();
  void loop();
  bool selectDimmer(bool next, bool prev) override;
  float getTemperature() override;
  void setTemperature(float temperature) override;
  float getLevel() override;
  void setLevel(float level) override;
  void toggle() override;

protected:
  void _setupDisplay();
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  void unknownBLEDimmerFound(BLEAdvertisedDevice& device);
  void knownBLEDimmerChanged(BLEAdvertisedDevice& device);
  virtual String formHandler_field_perdevice(const char *deviceName) override;
  virtual void scanningChanged() override;
  virtual void showMessage(const char *message) override;
private:
  void dimmerOnOffChanged();
  void dimmerValueChanged();
  void dimmerAvailableChanged();
  void handler();
  Buttons buttons;
  DimmerDynamicCollection::ItemType* getDimmerForCommand(int num);
  void updateDisplay(bool clear);
  typedef std::pair<std::string, BLEAddress> unknownDimmerInfo;
  DimmerDynamicCollection dimmers;
  DimmerDynamicCollection::ItemType* dimmerFactory(int num);
  int selectedDimmerIndex = 0; // currently selected dimmer on display
  bool selectedDimmerIsAvailable = false;
  int keepOpenMillis = 3000; // xxxjack should be configurable
};

bool
IotsaLedstripControllerMod::selectDimmer(bool next, bool prev) {
  bool rv = true;
  if (next) selectedDimmerIndex++;
  if (prev) selectedDimmerIndex--;
  if (selectedDimmerIndex < 0) {
    display->flash();
    selectedDimmerIndex = 0;
    rv = false;
  }
  if (selectedDimmerIndex >= dimmers.size()) {
    display->flash();
    selectedDimmerIndex = dimmers.size()-1;
    rv = false;
  }
  LOG_UI IotsaSerial.printf("LissabonController: now selectedDimmer=%d\n", selectedDimmerIndex);
  updateDisplay(false);
  iotsaConfig.postponeSleep(4000);
  if (rv) {
    auto d = dimmers.at(selectedDimmerIndex);
    selectedDimmerIsAvailable = d->available();
    d->refresh();
  }
  return rv;
}

float IotsaLedstripControllerMod::getTemperature() {
#ifdef DIMMER_WITH_TEMPERATURE
  auto d = getDimmerForCommand(selectedDimmerIndex);
  if (d) {
    float rv = (d->temperature - DIMMER_MIN_TEMPERATURE) / (DIMMER_MAX_TEMPERATURE - DIMMER_MIN_TEMPERATURE);
    if (rv < 0) rv = 0;
    if (rv > 1) rv = 1;
    return rv;
  }
#endif
  return 0;
}

void IotsaLedstripControllerMod::setTemperature(float temperature) {
#ifdef DIMMER_WITH_TEMPERATURE
  auto d = getDimmerForCommand(selectedDimmerIndex);
  if (d == nullptr) {
    return;
  }
  float tempKelvin = DIMMER_MIN_TEMPERATURE + temperature * (DIMMER_MAX_TEMPERATURE-DIMMER_MIN_TEMPERATURE);
  if (tempKelvin < DIMMER_MIN_TEMPERATURE) tempKelvin = DIMMER_MIN_TEMPERATURE;
  if (tempKelvin > DIMMER_MAX_TEMPERATURE) tempKelvin = DIMMER_MAX_TEMPERATURE;
  d->temperature = tempKelvin;
  d->updateDimmer();
  updateDisplay(false);
  LOG_UI IotsaSerial.printf("LissabonController: updated dimmer %d temperature %f\n", selectedDimmerIndex, temperature);
  iotsaConfig.postponeSleep(4000);
#else
  display->flash();
#endif
}

float IotsaLedstripControllerMod::getLevel() {
  auto d = getDimmerForCommand(selectedDimmerIndex);
  if (d) return d->level;
  return 0;
}

void IotsaLedstripControllerMod::setLevel(float level) {
  auto d = getDimmerForCommand(selectedDimmerIndex);
  if (d == nullptr) {
    return;
    d->level = level;
    return;
  }
  d->level = level;
  d->updateDimmer();
  updateDisplay(false);
  LOG_UI IotsaSerial.printf("LissabonController: updated dimmer %d level %f\n", selectedDimmerIndex, level);
  iotsaConfig.postponeSleep(4000);
}

void IotsaLedstripControllerMod::toggle() {
  auto d = getDimmerForCommand(selectedDimmerIndex);
  if (d != nullptr) {
    d->isOn = !d->isOn;
    d->updateDimmer();
    updateDisplay(false);
  }
}

void 
IotsaLedstripControllerMod::updateDisplay(bool clear) {
  LOG_BLE IotsaSerial.printf("LissabonController: %d strips:\n", dimmers.size());

  if (true || clear) display->clearStrips();
  int index = 0;
  for (auto& _elem : dimmers) {
    // Living dangerously: we don't have rtti so we can't use dynamic cast.
    // We know that is safe because we supplied the factory function.
    BLEDimmer* elem = reinterpret_cast<BLEDimmer *>(_elem);
    String name = elem->getUserVisibleName();
    LOG_BLE IotsaSerial.printf("  device %s, available=%d connected=%d\n", name.c_str(), elem->available(), elem->isConnected());
    display->addStrip(index, name, elem->available(), elem->isConnected());
    index++;
  }
  display->selectStrip(selectedDimmerIndex);
  if (selectedDimmerIndex >= 0) {
    auto d = dimmers.at(selectedDimmerIndex);
    if (d && d->available() && d->dataValid()) {
      display->setLevel(d->level, d->isOn);
      display->setTemp(getTemperature());
    } else {
      display->clearLevel();
      display->clearTemp();
    }
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


void IotsaLedstripControllerMod::dimmerAvailableChanged() {
  LOG_UI IotsaSerial.println("LissabonController: dimmerAvailableChanged()");
  bool availableChanged = true;
  display->showScanning(isScanning());
  updateDisplay(false);
  if (availableChanged) {
    // if this happens to be the current dimmer we want to refresh its status
    selectDimmer(false, false);
  }
}

void IotsaLedstripControllerMod::showMessage(const char *message) {
  display->showActivity(message);
}

void IotsaLedstripControllerMod::scanningChanged() {
  display->showScanning(isScanning());
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
  BLEDimmer *newDimmer = new BLEDimmer(num, *this, this, keepOpenMillis);
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
  // This is a hack. We don't implement DELETE so we add a funny value
  if (request.containsKey("clearall") && request["clearall"]) {
    dimmers.clear();
    anyChanged = true;
  }
  // This is another hack.
  if (request.containsKey("add")) {
    String newDimmerName = request["add"];
    if (newDimmerName != "" && dimmers.find(newDimmerName) == nullptr) {
      dimmers.push_back_new(newDimmerName);
      dimmers.setup();
      anyChanged = true;
    } else {
      IotsaSerial.println("IotsaLedstripControllerMod::putHandler: Bad add= value ");
    }
  }
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
  buttons.setup();

  auto unknownCallback = std::bind(&IotsaLedstripControllerMod::unknownBLEDimmerFound, this, std::placeholders::_1);
  setUnknownDeviceFoundCallback(unknownCallback);
  auto knownCallback = std::bind(&IotsaLedstripControllerMod::knownBLEDimmerChanged, this, std::placeholders::_1);
  setKnownDeviceChangedCallback(knownCallback);
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


void IotsaLedstripControllerMod::unknownBLEDimmerFound(BLEAdvertisedDevice& deviceAdvertisement) {
  LOG_BLE IotsaSerial.printf("LissabonController: unknownDeviceFound: device \"%s\"\n", deviceAdvertisement.getName().c_str());
  unknownDevices.insert(deviceAdvertisement.getName());
}

void IotsaLedstripControllerMod::knownBLEDimmerChanged(BLEAdvertisedDevice& deviceAdvertisement) {
  std::string name = deviceAdvertisement.getName();
  LOG_BLE IotsaSerial.printf("LissabonController: knownDeviceChanged: device \"%s\"\n", name.c_str());
  dimmerAvailableChanged();
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

