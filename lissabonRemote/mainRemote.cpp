//
// A Neopixel strip server, which allows control over a strip of neopixels, intended
// to be used as lighting. Color can be set as fraction RGB or HSL, gamma can be changed,
// interval between lit pixels can be changed. Control is through a web UI or
// through REST calls (and/or, depending on Iotsa compile time options, COAP calls).
// The web interface can be disabled by building iotsa with IOTSA_WITHOUT_WEB.
//

#include "iotsa.h"
#include "iotsaLed.h"
#include "iotsaConfigFile.h"
#include <set>

#undef DEBUG_PRINT_HEAP_SPACE

#define LED_PIN 22  // Define to turn on the LED when powered and not sleeping.

//
// Device can be rebooted or configuration mode can be requested by quickly tapping any button.
// TAP_DURATION sets the maximum time between press-release-press-etc.
#define TAP_COUNT_MODE_CHANGE 4
#define TAP_COUNT_REBOOT 8
#define TAP_DURATION 1000

IotsaApplication application("Lissabon Remote");

#include "iotsaBattery.h"
#define PIN_DISABLESLEEP 0
#define PIN_VBAT 37
#define VBAT_100_PERCENT (12.0/11.0) // 100K and 1M resistors divide by 11, not 10...
IotsaBatteryMod batteryMod(application);

#include "iotsaInput.h"
Touchpad touch1down(32, true, true, true);
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

#include "iotsaBLEClientManager.h"

#include "DimmerCollection.h"
#include "DimmerBLEClient.h"
#include "DimmerUI.h"

using namespace Lissabon;

class LissabonRemoteMod : public IotsaBLEClientManagerMod, public DimmerCallbacks {
public:
  LissabonRemoteMod(IotsaApplication &_app)
  : IotsaBLEClientManagerMod(_app)
  {
    DimmerBLEClient *dimmer = new DimmerBLEClient(1, *this, this);
    dimmer->followDimmerChanges(true);
    dimmers.push_back(dimmer);
  #ifdef WITH_SECOND_DIMMER
    dimmer = new DimmerBLEClient(2, *this, this);
    dimmer->followDimmerChanges(true);
    dimmers.push_back(dimmer);
  #endif
  }
  void setup() override;
  void lateSetup() override;
  String info() override;
  void configLoad() override;
  void configSave() override;
  void loop() override;
  // A fixed set of dimmers (constructed above), each controlled by its own
  // buttons. A dimmer without a name isn't registered with the base class,
  // so its page and REST interface don't show it: "add" names the first
  // unnamed dimmer, "remove" clears the name again (cwi-dis/iotsa#183
  // downstream sweep).
  bool addDeviceByName(const std::string& name) override;
  bool removeDeviceByName(const std::string& name) override;

protected:
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "BLE Dimmers"; }
  const char *devicesLabel() override { return "dimmers"; }
#endif
  void unknownDimmerBLEClientFound(const NimBLEAdvertisedDevice& deviceAdvertisement);
  // Only surface other lissabon BLE devices as "unknown/addable" candidates
  // (cwi-dis/iotsa#264).
  bool isInterestingUnknownDevice(const NimBLEAdvertisedDevice* device) override {
    return device->isAdvertisingService(Lissabon::serviceUUID);
  }
private:
  void dimmerOnOffChanged() override;
  void dimmerValueChanged() override;
  void dimmerAvailableChanged() override {};
  void ledOn();
  void ledOff();
  DimmerCollection dimmers;
  std::vector<DimmerUI*> dimmerUIs;
  uint32_t ledOffAtMillis = 0;
  uint32_t ledOffUntilMillis = 0;
  uint32_t lastButtonChangeMillis = 0;
  int buttonChangeCount = 0;
};

void LissabonRemoteMod::ledOn() {
#ifdef LED_PIN
  digitalWrite(LED_PIN, LOW);
#endif
}

void LissabonRemoteMod::ledOff() {
#ifdef LED_PIN
  digitalWrite(LED_PIN, HIGH);
#endif
  
}

void LissabonRemoteMod::dimmerOnOffChanged() {
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
}
bool LissabonRemoteMod::addDeviceByName(const std::string& _name) {
  String name(_name.c_str());
  if (dimmers.find(name) != nullptr) return false;
  for (auto d : dimmers) {
    if (d->hasName()) continue;
    d->setName(name);  // registers it with the base class
    d->setup();
    return true;
  }
  return false;
}

bool LissabonRemoteMod::removeDeviceByName(const std::string& _name) {
  AbstractDimmer *d = dimmers.find(String(_name.c_str()));
  if (d == nullptr) return false;
  d->setName("");  // unregisters it
  return true;
}

String LissabonRemoteMod::info() {

  String message = "<p>";
  message += dimmers.info();
  message += "See <a href='/bledimmer'>/bledimmer</a> to change settings or <a href='/api/bledimmer'>/api/bledimmer</a> for REST API.<br>";
  message += "</p>";
  return message;
}

void LissabonRemoteMod::lateSetup() {
  name = "bledimmer";
  // /bledimmer (the module page, GET + POST) is auto-registered by this
  // get=true api.setup(). Deliberately does not chain to
  // IotsaBLEClientMod::lateSetup() -- this module exposes its own
  // /bledimmer endpoint instead of the base /bleclient one.
  api.setup("bledimmer", true, true);
}

void LissabonRemoteMod::configLoad() {
  // IotsaBLEClientMod::configLoad();
  IotsaConfigFileLoad cf("/config/bledimmer.cfg");
  dimmers.configLoad(cf, "");
}

void LissabonRemoteMod::configSave() {
  // IotsaBLEClientMod::configSave();
  IotsaConfigFileSave cf("/config/bledimmer.cfg");
  dimmers.configSave(cf, "");
}

void LissabonRemoteMod::setup() {
  //
  // Let our base class do its setup.
  //
  IotsaBLEClientMod::setup();
  //
  // Load configuration
  //
  configLoad();
  //
  // Setup LED (fur user feedback) and pin for disabling sleep
  //
  iotsaController.allowRCMDescription("tap any touchpad 4 times");
#ifdef LED_PIN
  pinMode(LED_PIN, OUTPUT);
#endif
  ledOn();
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  //
  // Setup buttons for first (and optional second) dimmer
  //
  DimmerUI *ui = new DimmerUI(*dimmers.at(0));
  ui->setUpDownButtons(encoder1);
  dimmerUIs.push_back(ui);
#ifdef WITH_SECOND_DIMMER
  ui = new DimmerUI(*dimmers.at(1));
  ui->setUpDownButtons(encoder2);
  dimmerUIs.push_back(ui);
#endif // WITH_SECOND_DIMMER
  //
  // Setup callback so we are informaed of unknown dimmers.
  //
  auto callback = std::bind(&LissabonRemoteMod::unknownDimmerBLEClientFound, this, std::placeholders::_1);
  setUnknownDeviceFoundCallback(callback);
  // Service filtering is now isInterestingUnknownDevice() above, not a
  // settable field (cwi-dis/iotsa#264) -- setDuplicateNameFilter() was
  // already dead code (set, never read) even before that, so it's just
  // dropped here, not replaced.
  //
  // Setup dimmers by getting current settings from BLE devices
  //
  dimmers.setup();
  ledOff();
}

void LissabonRemoteMod::unknownDimmerBLEClientFound(const NimBLEAdvertisedDevice& deviceAdvertisement) {
  // Nothing to do here -- IotsaBLEClientManagerMod::onUnknownDeviceSeen()
  // already records this device (name/address/rssi/lastSeen) in
  // unknownDevices before calling us.
  IFDEBUG IotsaSerial.printf("unknownDimmerBLEClientFound: iotsaLedstrip/iotsaDimmer \"%s\"\n", deviceAdvertisement.getName().c_str());
}

// The values aren't saved (they are the dimmer's), so this only lights the
// LED for a moment.
void LissabonRemoteMod::dimmerValueChanged() {
  ledOffAtMillis = millis() + 1000;
  ledOn();
}

void LissabonRemoteMod::loop() {
#ifdef DEBUG_PRINT_HEAP_SPACE
  { static uint32_t last; if (millis() > last+1000) { iotsaStatus.printHeapSpace(); last = millis(); }}
#endif
  //
  // Let our baseclass do its loop-y things
  //
  IotsaBLEClientManagerMod::loop();
  
  if (ledOffAtMillis > 0 && millis() > ledOffAtMillis) {
    ledOffAtMillis = 0;
    ledOff();
  }
  dimmers.loop();
}

// Instantiate our remote control module, and install it in the framework
LissabonRemoteMod remoteMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
#if 0
  heap_caps_check_integrity_all(true);
#endif
  application.loop();
}
