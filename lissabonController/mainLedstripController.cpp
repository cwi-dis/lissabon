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
// #define PIN_DISABLESLEEP 0
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
  void lateSetup() override;
  String info();
  void configLoad();
  void configSave();
  void loop();
  void selectDimmer(bool next, bool prev) override;
  float getTemperature() override;
  void setTemperature(float temperature) override;
  float getLevel() override;
  void setLevel(float level) override;
  void toggle() override;
  void sleepWakeupNotification(bool sleep) override;

protected:
  void _setupDisplay();
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  void unknownBLEDimmerFound(const NimBLEAdvertisedDevice& device);
  void knownBLEDimmerChanged(const NimBLEAdvertisedDevice& device);
  virtual String formHandler_field_perdevice(const char *deviceName) override;
  virtual void scanningChanged() override;
  virtual void showMessage(const char *message) override;
private:
  void dimmerOnOffChanged();
  void dimmerValueChanged();
  void dimmerAvailableChanged();
  void webHandler() override;
  void clearAllDimmersAndReboot();
  Buttons buttons;
  DimmerDynamicCollection::ItemType* getDimmerForCommand(int num);
  void updateDisplay(bool clear);
  void nudgeRefreshPriority(int index);
  typedef std::pair<std::string, NimBLEAddress> unknownDimmerInfo;
  DimmerDynamicCollection dimmers;
  DimmerDynamicCollection::ItemType* dimmerFactory(int num);
  int selectedDimmerIndex = 0; // currently selected dimmer on display
  int savedSelectedDimmerIndex = 0;
  bool selectedDimmerIsAvailable = false;
  int stayConnectedMillis = 3000; // deliberately not configurable yet, see cwi-dis/iotsa#144
  bool saveNeeded = false;
  int nextRefreshIndex = 0; // round-robin cursor, see loop()
};

void
IotsaLedstripControllerMod::nudgeRefreshPriority(int index) {
  // One-time priority nudge, not a persistent bias: makes the round-robin
  // refresh scheduler (loop()) try this dimmer next, without permanently
  // anchoring it there -- a dimmer that's stuck unreachable would otherwise
  // get retried every idle tick again, reintroducing the starvation bug
  // round-robin was added to fix (cwi-dis/lissabon#30). Called whenever the
  // user's own action makes one dimmer newly relevant: selecting it, booting
  // up with a persisted selection, or issuing a command that couldn't reach
  // it yet.
  if (index >= 0 && index < dimmers.size()) nextRefreshIndex = index;
}

void
IotsaLedstripControllerMod::selectDimmer(bool next, bool prev) {
  if (next) {
    selectedDimmerIndex++;
    selectedDimmerIsAvailable = false;
  }
  if (prev) {
    selectedDimmerIndex--;
    selectedDimmerIsAvailable = false;
  }
  if (selectedDimmerIndex < 0) {
    display->flash();
    selectedDimmerIndex = 0;
  }
  if (selectedDimmerIndex >= dimmers.size()) {
    display->flash();
    selectedDimmerIndex = dimmers.size()-1;
  }
  // Missing since this save mechanism was introduced (a535ca3, 2023): setLevel()/
  // toggle() both mark saveNeeded when they change selectedDimmerIndex's saved value,
  // but selectDimmer() itself -- the rocker switch, the actual way the selection
  // normally changes -- never did, so scrolling to a different strip without also
  // touching its level/on-off was silently never persisted.
  if (selectedDimmerIndex != savedSelectedDimmerIndex) saveNeeded = true;
  // Only on a genuine rocker-driven change, not the internal (false, false)
  // refresh-only calls dimmerAvailableChanged() makes on every state change --
  // those fire constantly during connect/fail/retry churn, and nudging on every
  // one of them turned the intended one-time nudge into a permanent, continuously
  // reasserted lock on whichever dimmer happened to be selected (observed live,
  // 2026-09-26: it starved every other dimmer for the whole boot).
  if (next || prev) nudgeRefreshPriority(selectedDimmerIndex);
  LOG_UI IotsaSerial.printf("LissabonController: now selectedDimmer=%d\n", selectedDimmerIndex);
  updateDisplay(false);
  buttons.refreshEncoder();
  // Same bug class as the nudge fix above: only on a genuine rocker-driven
  // change, not dimmerAvailableChanged()'s internal (false, false) calls --
  // those fire constantly during connect/fail/retry churn, and
  // postponeSleep(4000) adds activityExtraWakeDuration on top (see
  // IotsaSleepPolicy::postponeSleep()), so unconditionally calling it here
  // meant the sleep-inhibit deadline got pushed out by a full minute on
  // every single internal refresh -- confirmed live, 2026-09-26: control
  // hadn't slept in 7 hours, with postponeSleep sitting at ~64000ms
  // (4000 + activityExtraWakeDuration) essentially continuously.
  if (next || prev) iotsaController.postponeSleep(4000);
  if (selectedDimmerIndex < dimmers.size()) {
    auto d = dimmers.at(selectedDimmerIndex);
    bool availableNow = d->available();
    bool mustUpdate = availableNow && !selectedDimmerIsAvailable;
    selectedDimmerIsAvailable = availableNow;
    if (mustUpdate) {
      LOG_UI IotsaSerial.printf("LissabonController: refresh selectedDimmer=%d\n", selectedDimmerIndex);
      d->refresh();
    }
  }
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
    display->flash();
    nudgeRefreshPriority(selectedDimmerIndex);
    return;
  }
  float tempKelvin = DIMMER_MIN_TEMPERATURE + temperature * (DIMMER_MAX_TEMPERATURE-DIMMER_MIN_TEMPERATURE);
  if (tempKelvin < DIMMER_MIN_TEMPERATURE) tempKelvin = DIMMER_MIN_TEMPERATURE;
  if (tempKelvin > DIMMER_MAX_TEMPERATURE) tempKelvin = DIMMER_MAX_TEMPERATURE;
  d->temperature = tempKelvin;
  d->updateDimmer();
  updateDisplay(false);
  LOG_UI IotsaSerial.printf("LissabonController: updated dimmer %d temperature %f\n", selectedDimmerIndex, temperature);
  iotsaController.postponeSleep(4000);
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
    display->flash();
    nudgeRefreshPriority(selectedDimmerIndex);
    return;
  }
  d->level = level;
  d->updateDimmer();
  updateDisplay(false);
  LOG_UI IotsaSerial.printf("LissabonController: updated dimmer %d level %f\n", selectedDimmerIndex, level);
  if (selectedDimmerIndex != savedSelectedDimmerIndex) saveNeeded = true;
  iotsaController.postponeSleep(4000);
}

void IotsaLedstripControllerMod::toggle() {
  auto d = getDimmerForCommand(selectedDimmerIndex);
  if (d != nullptr) {
    d->isOn = !d->isOn;
    d->updateDimmer();
    updateDisplay(false);
    if (selectedDimmerIndex != savedSelectedDimmerIndex) saveNeeded = true;
  } else {
    nudgeRefreshPriority(selectedDimmerIndex);
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
    StripStatus status = StripStatus::unavailable;
    if (elem->available()) status = StripStatus::seen;
    if (elem->dataValid()) status = StripStatus::synced;
    if (elem->isConnecting()) status = StripStatus::connecting;
    if (elem->isConnected()) status = StripStatus::connected;
    display->addStrip(index, name, status);
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
    return nullptr;
  }
  auto d = dimmers.at(num);  
  if (d == nullptr) {
    IotsaSerial.printf("LissabonController: Dimmer %d does not exist\n", num);
    return nullptr;
  }
  if (!d->available()) {
    IotsaSerial.printf("LissabonController: Dimmer %d unavailable\n", num);
    updateScanning();
    return nullptr;
  }
  if (!d->dataValid()) {
    IotsaSerial.printf("LissabonController: Dimmer %d current status unknown\n", num);
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
      iotsaController.allowRequestedConfigurationMode();
    }
    if (buttonChangeCount == TAP_COUNT_REBOOT) {
      IFDEBUG IotsaSerial.println("tap mode reboot");
      ledOffUntilMillis = now + 2000;
      iotsaController.requestReboot(1000);
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
  LOG_UI IotsaSerial.println("LissabonController: dimmerValueChanged()");
  updateDisplay(false);
}

DimmerDynamicCollection::ItemType *
IotsaLedstripControllerMod::dimmerFactory(int num) {
  BLEDimmer *newDimmer = new BLEDimmer(num, *this, this, stayConnectedMillis);
  newDimmer->followDimmerChanges(true);
  return newDimmer;
}

void
IotsaLedstripControllerMod::webHandler() {
  IotsaWebServer *server = api.webService->server;
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
    clearAllDimmersAndReboot();
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
  bool clearall;
  if (getFromRequest<bool>(request, "clearall", clearall) && clearall) {
    clearAllDimmersAndReboot();
    return true;
  }
  // This is another hack.
  String newDimmerName;
  if (getFromRequest<String>(request, "add", newDimmerName)) {
    if (dimmers.find(newDimmerName) == nullptr) {
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

void IotsaLedstripControllerMod::lateSetup() {
  name = "blecontroller";
  // /blecontroller (the module page, GET + POST) is auto-registered by this
  // get=true api.setup(); it dispatches to webHandler(). Deliberately does
  // not chain to IotsaBLEClientMod::lateSetup() -- this module exposes its
  // own /blecontroller endpoint instead of the base /bleclient one, and
  // getHandler()/putHandler() forward to the base explicitly.
  api.setup("blecontroller", true, true);
}

void IotsaLedstripControllerMod::configLoad() {
  IotsaConfigFileLoad cf("/config/blecontroller.cfg");
  cf.get("selectedDimmerIndex", selectedDimmerIndex, selectedDimmerIndex);
  savedSelectedDimmerIndex = selectedDimmerIndex;
  dimmers.configLoad(cf, "");
}

void IotsaLedstripControllerMod::configSave() {
  IotsaConfigFileSave cf("/config/blecontroller.cfg");
  IotsaSerial.println("LissabonController: save blecontroller config");
  cf.put("selectedDimmerIndex", selectedDimmerIndex);
  savedSelectedDimmerIndex = selectedDimmerIndex;
  dimmers.configSave(cf, "");
}

void IotsaLedstripControllerMod::clearAllDimmersAndReboot() {
  // Deliberately don't touch the live `dimmers` collection here. Deleting a
  // BLEDimmer whose connectionTask is still blocked inside a BLE connect
  // attempt calls vTaskDelete() on that task (BLEDimmer::~BLEDimmer()), which
  // can crash NimBLE's host task later when an async GAP event tries to
  // notify the now-deleted task (observed live: a stuck connect to a
  // just-added, flaky device, followed by "Remove All", panicked ~5s later
  // inside xTaskGenericNotify). Simplest safe fix: persist zero dimmers and
  // reboot -- a fresh boot never constructs them, so there's nothing to tear
  // down.
  IotsaConfigFileSave cf("/config/blecontroller.cfg");
  cf.put("selectedDimmerIndex", 0);
  cf.put("n_dimmer", 0);
  iotsaController.requestReboot(1000);
}

void IotsaLedstripControllerMod::setup() {
  //
  // Let our base class do its setup.
  //
  IotsaBLEClientMod::setup();
  dimmers.setFactory(std::bind(&IotsaLedstripControllerMod::dimmerFactory, this, std::placeholders::_1));
  // Allow switching to iotsa config mode over BLE
  batteryMod.allowBLEConfigModeSwitch();
  //
  // Load configuration
  //
  configLoad();
  nudgeRefreshPriority(selectedDimmerIndex); // prioritize the persisted selection on boot
 #if 0
  iotsaController.allowRCMDescription("tap any touchpad 4 times");
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


void IotsaLedstripControllerMod::unknownBLEDimmerFound(const NimBLEAdvertisedDevice& deviceAdvertisement) {
  // Nothing to do here -- IotsaBLEClientMod::onResult() already records this
  // device (name/address/rssi/lastSeen) in unknownDevices before calling us.
  LOG_BLE IotsaSerial.printf("LissabonController: unknownDeviceFound: device \"%s\"\n", deviceAdvertisement.getName().c_str());
}

void IotsaLedstripControllerMod::knownBLEDimmerChanged(const NimBLEAdvertisedDevice& deviceAdvertisement) {
  std::string name = deviceAdvertisement.getName();
  LOG_BLE IotsaSerial.printf("LissabonController: knownDeviceChanged: device \"%s\"\n", name.c_str());
  dimmerAvailableChanged();
}

void IotsaLedstripControllerMod::sleepWakeupNotification(bool sleep) 
{
  IotsaSerial.printf("LissabonController: sleep %d\n", (int)sleep);
  display->dim(sleep);
  if (!sleep) {
    buttons.justAwake();
  }
}


void IotsaLedstripControllerMod::loop() {
  //
  // Let our baseclass do its loop-y things
  //
  IotsaBLEClientMod::loop();

  //
  // Let the dimmers do any processing they need to do
  //
  for (auto d : dimmers) {
    d->loop();
  }
  //
  // If we are idle we may want to do a save, or load any available dimmer values.
  //
  bool isIdle = true;
  int n = dimmers.size();
  for (int i = 0; i < n; i++) {
    // Living dangerously: we don't have rtti so we can't use dynamic cast.
    // We know that is safe because we supplied the factory function.
    BLEDimmer* d_ble = reinterpret_cast<BLEDimmer*>(dimmers.at(i));
    if (d_ble->available() && (d_ble->isConnected() || d_ble->isConnecting())) {
      isIdle = false;
    }
  }
  if (isIdle) {
    if (saveNeeded) {
      IotsaSerial.println("LissabonController: save requested");
      saveNeeded = false;
      configSave();
    }
    // Round-robin starting at nextRefreshIndex, not always from the front: a
    // dimmer that keeps failing to connect must not starve the ones after it
    // in the list forever (cwi-dis/lissabon#30).
    for (int k = 0; k < n; k++) {
      int i = (nextRefreshIndex + k) % n;
      BLEDimmer* d_ble = reinterpret_cast<BLEDimmer*>(dimmers.at(i));
      if (d_ble->available() && !d_ble->dataValid()) {
        IotsaSerial.printf("LissabonController: refresh idle dimmer %d\n", d_ble->num);
        nextRefreshIndex = (i + 1) % n;
        d_ble->refresh();
        break;
      }
    }
  }
}

// Instantiate the Led module, and install it in the framework
IotsaLedstripControllerMod ledstripControllerMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}

