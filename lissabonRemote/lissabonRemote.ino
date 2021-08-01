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

#undef DEBUG_PRINT_HEAP_SPACE

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.
#define LED_PIN 22  // Define to turn on the LED when powered and not sleeping.

//
// Device can be rebooted or configuration mode can be requested by quickly tapping any button.
// TAP_DURATION sets the maximum time between press-release-press-etc.
#define TAP_COUNT_MODE_CHANGE 4
#define TAP_COUNT_REBOOT 8
#define TAP_DURATION 1000

IotsaApplication application("Lissabon Remote");
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

#include "iotsaBLEClient.h"

#include "DimmerCollection.h"
#include "BLEDimmer.h"
#include "DimmerUI.h"

using namespace Lissabon;

class LissabonRemoteMod : public IotsaBLEClientMod, public DimmerCallbacks {
public:
  LissabonRemoteMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaBLEClientMod(_app, _auth)
  {
    BLEDimmer *dimmer = new BLEDimmer(1, *this, this);
    dimmer->followDimmerChanges(true);
    dimmers.push_back(dimmer);
  #ifdef WITH_SECOND_DIMMER
    dimmer = new BLEDimmer(2, *this, this);
    dimmer->followDimmerChanges(true);
    dimmers.push_back(dimmer);
  #endif
  }
  void setup() override;
  void serverSetup() override;
  String info() override;
  void configLoad() override;
  void configSave() override;
  void loop() override;

protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  void unknownBLEDimmerFound(BLEAdvertisedDevice& deviceAdvertisement);
private:
  void dimmerOnOffChanged() override;
  void dimmerValueChanged() override;
  void handler();
  void ledOn();
  void ledOff();
  DimmerCollection dimmers;
  std::vector<DimmerUI*> dimmerUIs;
  uint32_t saveAtMillis = 0;
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
}
#ifdef IOTSA_WITH_WEB
void
LissabonRemoteMod::handler() {
  bool anyChanged = false;
  // xxxjack this also saves the config file if a non-config setting has been changed. Oh well...
  anyChanged |= dimmers.formHandler_args(server, "", true);
  anyChanged |= IotsaBLEClientMod::formHandler_args(server, "", true);
  if (anyChanged) {
    configSave();
  }
  String message = "<html><head><title>BLE Dimmers</title></head><body><h1>BLE Dimmers</h1>";
  message += "<h2>Dimmer Settings</h2><form method='post'>";
  dimmers.formHandler_fields(message, "", "", false);
  message += "<input type='submit' name='set' value='Set Dimmers'></form><br>";

  message += "<h2>Dimmer Configuration</h2><form method='post'>";
  dimmers.formHandler_fields(message, "", "", true);
  message += "<input type='submit' name='config' value='Configure Dimmers'></form><br>";

  IotsaBLEClientMod::formHandler_fields(message, "BLE Dimmer", "dimmer", true);

  message += "</body></html>";
  server->send(200, "text/html", message);
}

String LissabonRemoteMod::info() {

  String message = "<p>";
  message += dimmers.info();
  message += "See <a href='/bledimmer'>/bledimmer</a> to change settings or <a href='/api/bledimmer'>/api/bledimmer</a> for REST API.<br>";
  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

bool LissabonRemoteMod::getHandler(const char *path, JsonObject& reply) {
  // xxxjack need to distinguish between config and operational parameters
  IotsaBLEClientMod::getHandler(path, reply);
  dimmers.getHandler(reply);
  return true;
}

bool LissabonRemoteMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  // xxxjack need to distinguish between config and operational parameters
  bool anyChanged = false;
  anyChanged = IotsaBLEClientMod::putHandler(path, request, reply);
  anyChanged |= dimmers.putHandler(request);
  if (anyChanged) {
    configSave();
  }
  return anyChanged;
}

void LissabonRemoteMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/bledimmer", std::bind(&LissabonRemoteMod::handler, this));
  server->on("/bledimmer", HTTP_POST,std::bind(&LissabonRemoteMod::handler, this));
#endif
  api.setup("/api/bledimmer", true, true);
  name = "bledimmer";
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
  iotsaConfig.allowRCMDescription("tap any touchpad 4 times");
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
  auto callback = std::bind(&LissabonRemoteMod::unknownBLEDimmerFound, this, std::placeholders::_1);
  setUnknownDeviceFoundCallback(callback);
  setDuplicateNameFilter(true);
  setServiceFilter(Lissabon::Dimmer::serviceUUID);
  //
  // Setup dimmers by getting current settings from BLE devices
  //
  dimmers.setup();
  ledOff();
}

// xxxjack move to IotsaBLEClientMod?
void LissabonRemoteMod::unknownBLEDimmerFound(BLEAdvertisedDevice& deviceAdvertisement) {
  IFDEBUG IotsaSerial.printf("unknownBLEDimmerFound: iotsaLedstrip/iotsaDimmer \"%s\"\n", deviceAdvertisement.getName().c_str());
  unknownDevices.insert(deviceAdvertisement.getName());
}

void LissabonRemoteMod::dimmerValueChanged() {
  saveAtMillis = millis() + 1000;
  ledOn();
}

void LissabonRemoteMod::loop() {
#ifdef DEBUG_PRINT_HEAP_SPACE
  { static uint32_t last; if (millis() > last+1000) { iotsaConfig.printHeapSpace(); last = millis(); }}
#endif
  //
  // Let our baseclass do its loop-y things
  //
  IotsaBLEClientMod::loop();
  
  //
  // See whether we have a value to save (because the user has been turning the dimmer)
  //
  if (saveAtMillis > 0 && millis() > saveAtMillis) {
    saveAtMillis = 0;
    configSave();
    ledOff();
  }
  dimmers.loop();
}

// Instantiate our remote control module, and install it in the framework
LissabonRemoteMod remoteMod(application);

#if 1
// For debugging configuration: allow backing up of all files
#include "iotsaFilesBackup.h"
IotsaFilesBackupMod backupMod(application);
#endif

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
#if 0
  heap_caps_check_integrity_all(true);
#endif
  application.loop();
}
