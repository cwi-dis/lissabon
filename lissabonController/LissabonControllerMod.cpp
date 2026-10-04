#include "LissabonControllerMod.h"

using namespace Lissabon;

Display *display;

#define LOG_BLE if(0)
#define LOG_UI if(1)

void
LissabonControllerMod::setDimmerFollowed(int index, bool follow) {
  // Only the selected dimmer ever gets a live, maintained BLE connection
  // (cwi-dis/lissabon#31) -- background-syncing all of them at once is what
  // made the shared NIMBLE_MAX_CONNECTIONS pool genuinely scarce. Living
  // dangerously: no RTTI, but the factory only ever creates DimmerBLEClients.
  if (index < 0 || index >= dimmers.size()) return;
  DimmerBLEClient* d = reinterpret_cast<DimmerBLEClient*>(dimmers.at(index));
  d->followDimmerChanges(follow);
}

void
LissabonControllerMod::selectDimmer(bool next, bool prev) {
  int oldSelectedDimmerIndex = selectedDimmerIndex;
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
  // Move the live connection to the new selection -- only on a genuine change,
  // not the internal (false, false) refresh-only calls dimmerAvailableChanged()
  // makes on every state change (those fire constantly during connect/fail/retry
  // churn). Gating on the index actually changing, rather than next||prev,
  // also handles the "next at the last dimmer" clamp case correctly -- next
  // can be true with no real change, and toggling follow off-then-on on the
  // same dimmer would force a pointless resync (same bug class as the
  // nudge/sleep-postpone fixes below: observed live, 2026-09-26, that an
  // unconditional per-call side effect here turns into a permanent lock or a
  // constant reset).
  if (selectedDimmerIndex != oldSelectedDimmerIndex) {
    setDimmerFollowed(oldSelectedDimmerIndex, false);
    setDimmerFollowed(selectedDimmerIndex, true);
  }
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

float LissabonControllerMod::getTemperature() {
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

void LissabonControllerMod::setTemperature(float temperature) {
#ifdef DIMMER_WITH_TEMPERATURE
  auto d = getDimmerForCommand(selectedDimmerIndex);
  if (d == nullptr) {
    display->flash();
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

float LissabonControllerMod::getLevel() {
  auto d = getDimmerForCommand(selectedDimmerIndex);
  if (d) return d->level;
  return 0;
}

void LissabonControllerMod::setLevel(float level) {
  auto d = getDimmerForCommand(selectedDimmerIndex);
  if (d == nullptr) {
    display->flash();
    return;
  }
  d->level = level;
  d->updateDimmer();
  updateDisplay(false);
  LOG_UI IotsaSerial.printf("LissabonController: updated dimmer %d level %f\n", selectedDimmerIndex, level);
  if (selectedDimmerIndex != savedSelectedDimmerIndex) saveNeeded = true;
  iotsaController.postponeSleep(4000);
}

void LissabonControllerMod::toggle() {
  auto d = getDimmerForCommand(selectedDimmerIndex);
  if (d != nullptr) {
    d->isOn = !d->isOn;
    d->updateDimmer();
    updateDisplay(false);
    if (selectedDimmerIndex != savedSelectedDimmerIndex) saveNeeded = true;
  }
}

void
LissabonControllerMod::updateDisplay(bool clear) {
  LOG_BLE IotsaSerial.printf("LissabonController: %d strips:\n", dimmers.size());

  if (true || clear) display->clearStrips();
  int index = 0;
  for (auto& _elem : dimmers) {
    // Living dangerously: we don't have rtti so we can't use dynamic cast.
    // We know that is safe because we supplied the factory function.
    DimmerBLEClient* elem = reinterpret_cast<DimmerBLEClient *>(_elem);
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
LissabonControllerMod::getDimmerForCommand(int num) {
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
    // Ask for it: iotsa only scans for devices with work pending (cwi-dis/iotsa#263).
    d->refresh();
    return nullptr;
  }
  if (!d->dataValid()) {
    IotsaSerial.printf("LissabonController: Dimmer %d current status unknown\n", num);
    return nullptr;
  }
  return d;
}


void LissabonControllerMod::dimmerAvailableChanged() {
  LOG_UI IotsaSerial.println("LissabonController: dimmerAvailableChanged()");
  bool availableChanged = true;
  display->showScanning(isScanning());
  updateDisplay(false);
  if (availableChanged) {
    // if this happens to be the current dimmer we want to refresh its status
    selectDimmer(false, false);
  }
}

void LissabonControllerMod::showMessage(const char *message) {
  display->showActivity(message);
}

void LissabonControllerMod::scanningChanged() {
  display->showScanning(isScanning());
}

void LissabonControllerMod::dimmerOnOffChanged() {
  LOG_UI IotsaSerial.println("LissabonController: dimmerOnOfChanged()");
  updateDisplay(false);
}


void LissabonControllerMod::dimmerValueChanged() {
  LOG_UI IotsaSerial.println("LissabonController: dimmerValueChanged()");
  updateDisplay(false);
}

DimmerDynamicCollection::ItemType *
LissabonControllerMod::dimmerFactory(int num) {
  DimmerBLEClient *newDimmer = new DimmerBLEClient(num, *this, this, stayConnectedMillis);
  // Not followed by default (cwi-dis/lissabon#31) -- setup()/selectDimmer()
  // turn following on for whichever dimmer is actually selected.
  return newDimmer;
}

bool LissabonControllerMod::addDeviceByName(const std::string& _name) {
  String name(_name.c_str());
  if (dimmers.find(name) != nullptr || getDevice(_name) != nullptr) {
    IotsaSerial.printf("LissabonController: cannot add \"%s\": already known\n", name.c_str());
    return false;
  }
  dimmers.push_back_new(name);  // its setName() registers it with the base class
  dimmers.setup();
  updateDisplay(true);
  return true;
}

bool LissabonControllerMod::removeDeviceByName(const std::string& _name) {
  String name(_name.c_str());
  int index = -1;
  for (int i = 0; i < dimmers.size(); i++) {
    auto d = dimmers.at(i);
    if (d->hasName() && d->getUserVisibleName() == name) index = i;
  }
  if (index < 0) return IotsaBLEClientCollectionMod::removeDeviceByName(_name); // not one of our dimmers
  bool wasSelected = (index == selectedDimmerIndex);
  // Deleting a DimmerBLEClient live is safe since it no longer has a task of
  // its own (cwi-dis/iotsa#263); its destructor unregisters it from the base.
  dimmers.remove(index);
  if (index < selectedDimmerIndex) selectedDimmerIndex--;
  if (selectedDimmerIndex >= dimmers.size()) selectedDimmerIndex = dimmers.size() - 1;
  if (selectedDimmerIndex < 0) selectedDimmerIndex = 0;
  if (wasSelected) setDimmerFollowed(selectedDimmerIndex, true);
  saveNeeded = true;
  updateDisplay(true);
  return true;
}

void
LissabonControllerMod::webHandler() {
  IotsaWebServer *server = api.webService->server;
  // xxxjack update settings for remotes?
  bool anyChanged = false;
  anyChanged |= dimmers.formHandler_args(server, "", true);
  anyChanged |= IotsaBLEClientCollectionMod::formHandler_args(server, "", true);
  if (server->hasArg("clearall") && server->arg("iamsure") == "iamsure") {
    clearAllDimmersAndReboot();
  }
  if (anyChanged) configSave();

  String message = "<html><head><title>Lissabon Controller</title></head><body><h1>Lissabon Controller</h1>";
  message += "<h2>Dimmer Settings</h2><form method='post'>";
  dimmers.formHandler_fields(message, "", "", false);
  message += "<input type='submit' name='set' value='Set Dimmers'></form><br>";

  message += "<h2>Dimmer Configurations</h2><form method='post'>";
  dimmers.formHandler_fields(message, "", "", true);
  message += "<input type='submit' name='config' value='Configure Dimmers'></form><br>";

  message += "<br><form method='post'>Remove all: <input type='checkbox' name='iamsure' value='iamsure'>I am sure <input type='submit' name='clearall' value='Remove All'></form><br>";

  IotsaBLEClientCollectionMod::formHandler_fields(message, "BLE Dimmer", "dimmer", true);

  message += "</body></html>";
  server->send(200, "text/html", message);
}

String LissabonControllerMod::formHandler_field_perdevice(const char *_deviceName) {
  String deviceName(_deviceName);
  String rv(deviceName);
  rv += "<form method='post'><input type='hidden' name='add' value='" + deviceName + "'><input type='submit' value='Add'></form>";
  return rv;
}

String LissabonControllerMod::info() {
  String message = "<p>See <a href='/blecontroller'>/blecontroller</a> to change settings or <a href='/api/blecontroller'>/api/blecontroller</a> for REST API.<br>";

  return message;
}

bool LissabonControllerMod::getHandler(const char *path, JsonObject& reply) {
  IotsaBLEClientCollectionMod::getHandler(path, reply);
  dimmers.getHandler(reply);
  return true;
}

bool LissabonControllerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  anyChanged = IotsaBLEClientCollectionMod::putHandler(path, request, reply);
  anyChanged |= dimmers.putHandler(request);
  // This is a hack. We don't implement DELETE so we add a funny value
  bool clearall;
  if (getFromRequest<bool>(request, "clearall", clearall) && clearall) {
    clearAllDimmersAndReboot();
    return true;
  }
  if (anyChanged) {
    configSave();
  }
  return true;
}

void LissabonControllerMod::lateSetup() {
  name = "blecontroller";
  // /blecontroller (the module page, GET + POST) is auto-registered by this
  // get=true api.setup(); it dispatches to webHandler(). Deliberately does
  // not chain to IotsaBLEClientMod::lateSetup() -- this module exposes its
  // own /blecontroller endpoint instead of the base /bleclient one, and
  // getHandler()/putHandler() forward to the base explicitly.
  api.setup("blecontroller", true, true);
}

void LissabonControllerMod::configLoad() {
  IotsaConfigFileLoad cf("/config/blecontroller.cfg");
  cf.get("selectedDimmerIndex", selectedDimmerIndex, selectedDimmerIndex);
  savedSelectedDimmerIndex = selectedDimmerIndex;
  dimmers.configLoad(cf, "");
}

void LissabonControllerMod::configSave() {
  IotsaConfigFileSave cf("/config/blecontroller.cfg");
  IotsaSerial.println("LissabonController: save blecontroller config");
  cf.put("selectedDimmerIndex", selectedDimmerIndex);
  savedSelectedDimmerIndex = selectedDimmerIndex;
  dimmers.configSave(cf, "");
}

void LissabonControllerMod::clearAllDimmersAndReboot() {
  // Deliberately don't touch the live `dimmers` collection here. Deleting a
  // DimmerBLEClient whose connectionTask is still blocked inside a BLE connect
  // attempt calls vTaskDelete() on that task (DimmerBLEClient::~DimmerBLEClient()), which
  // can crash NimBLE's host task later when an async GAP event tries to
  // notify the now-deleted task (observed live: a stuck connect to a
  // just-added, flaky device, followed by "Remove All", panicked ~5s later
  // inside xTaskGenericNotify). Simplest safe fix: persist zero dimmers and
  // reboot -- a fresh boot never constructs them, so there's nothing to tear
  // down. (Since cwi-dis/iotsa#263 DimmerBLEClient has no task of its own
  // any more, so deleting one live may well be safe now -- not retested,
  // the reboot stays.)
  IotsaConfigFileSave cf("/config/blecontroller.cfg");
  cf.put("selectedDimmerIndex", 0);
  cf.put("n_dimmer", 0);
  iotsaController.requestReboot(1000);
}

void LissabonControllerMod::setup() {
  //
  // Let our base class do its setup.
  //
  IotsaBLEClientMod::setup();
  dimmers.setFactory(std::bind(&LissabonControllerMod::dimmerFactory, this, std::placeholders::_1));
  // Allow switching to iotsa config mode over BLE
  batteryMod.allowBLEConfigModeSwitch();
  //
  // Load configuration
  //
  configLoad();
  setDimmerFollowed(selectedDimmerIndex, true); // only the persisted selection gets a live connection (cwi-dis/lissabon#31)
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  _setupDisplay();
  buttons.setup();

  auto unknownCallback = std::bind(&LissabonControllerMod::unknownDimmerBLEClientFound, this, std::placeholders::_1);
  setUnknownDeviceFoundCallback(unknownCallback);
  auto knownCallback = std::bind(&LissabonControllerMod::knownDimmerBLEClientChanged, this, std::placeholders::_1);
  setKnownDeviceChangedCallback(knownCallback);
  // Service filtering is now isInterestingUnknownDevice() below, not a
  // settable field (cwi-dis/iotsa#264) -- setDuplicateNameFilter() was
  // already dead code (set, never read) even before that, so it's just
  // dropped here, not replaced.
  //
  // Setup dimmers by getting current settings from BLE devices
  // xxxjack move to DimmerCollection
  //
  for (auto d : dimmers) {
    d->setup();
  }
}

void LissabonControllerMod::_setupDisplay() {
  if (display == NULL) display = new Display();
  updateDisplay(true);
}


bool LissabonControllerMod::isInterestingUnknownDevice(const NimBLEAdvertisedDevice* device) {
  return device->isAdvertisingService(Lissabon::serviceUUID);
}

void LissabonControllerMod::unknownDimmerBLEClientFound(const NimBLEAdvertisedDevice& deviceAdvertisement) {
  // Nothing to do here -- IotsaBLEClientCollectionMod::onUnknownDeviceSeen()
  // already records this device (name/address/rssi/lastSeen) in
  // unknownDevices before calling us.
  LOG_BLE IotsaSerial.printf("LissabonController: unknownDeviceFound: device \"%s\"\n", deviceAdvertisement.getName().c_str());
}

void LissabonControllerMod::knownDimmerBLEClientChanged(const NimBLEAdvertisedDevice& deviceAdvertisement) {
  std::string name = deviceAdvertisement.getName();
  LOG_BLE IotsaSerial.printf("LissabonController: knownDeviceChanged: device \"%s\"\n", name.c_str());
  dimmerAvailableChanged();
}

void LissabonControllerMod::sleepWakeupNotification(bool sleep)
{
  IotsaSerial.printf("LissabonController: sleep %d\n", (int)sleep);
  display->dim(sleep);
  if (!sleep) {
    buttons.justAwake();
  }
}


void LissabonControllerMod::loop() {
  //
  // Let our baseclass do its loop-y things
  //
  IotsaBLEClientCollectionMod::loop();

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
    DimmerBLEClient* d_ble = reinterpret_cast<DimmerBLEClient*>(dimmers.at(i));
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
    // Only the selected dimmer is ever a refresh candidate (cwi-dis/lissabon#31)
    // -- the round-robin this replaced (cwi-dis/lissabon#30) existed to keep
    // one slow/unreachable dimmer from starving the others' turn, which is
    // moot once there is only ever one dimmer in contention for a connection.
    if (selectedDimmerIndex >= 0 && selectedDimmerIndex < n) {
      DimmerBLEClient* d_ble = reinterpret_cast<DimmerBLEClient*>(dimmers.at(selectedDimmerIndex));
      if (d_ble->available() && !d_ble->dataValid()) {
        IotsaSerial.printf("LissabonController: refresh selected dimmer %d\n", d_ble->num);
        d_ble->refresh();
      }
    }
  }
}
