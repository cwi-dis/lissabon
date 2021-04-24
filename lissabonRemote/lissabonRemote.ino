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
IotsaBLEClientMod bleClientMod(application);

#include "BLEDimmer.h"
#include "DimmerUI.h"

using namespace Lissabon;

class DimmerCollection {
public:
  typedef AbstractDimmer ItemType;
  typedef std::vector<ItemType*> ItemVectorType;
  typedef ItemVectorType::iterator iterator;

  int size() { return dimmers.size(); }
  DimmerUI* push_back(ItemType* dimmer, bool addUI) { 
    dimmers.push_back(dimmer);
    DimmerUI *ui = nullptr;
    if (addUI) ui = new DimmerUI(*dimmer);
    dimmerUIs.push_back(ui);
    return ui;
  }
  ItemType* at(int i) { return dimmers.at(i); }
  DimmerUI* ui_at(int i) { return dimmerUIs.at(i); }
  iterator begin() { return dimmers.begin(); }
  iterator end() { return dimmers.end(); }

  void getHandler(JsonObject& reply) {
    for (auto d : dimmers) {
      String ident = "dimmer" + String(d->num);
      JsonObject dimmerReply = reply.createNestedObject(ident);
      d->getHandler(dimmerReply);
    }
  }

  bool putHandler(const JsonVariant& request) {
    bool anyChanged = false;
    JsonObject reqObj = request.as<JsonObject>();
    for (auto d : dimmers) {
      String ident = "dimmer" + String(d->num);
      JsonVariant dimmerRequest = reqObj[ident];
      if (dimmerRequest) {
        if (d->putHandler(dimmerRequest)) anyChanged = true;
      }
    }
    return anyChanged;
  }

  bool configLoad(IotsaConfigFileLoad& cf, const String& f_name) {
    for (auto d : dimmers) {
      String ident = "dimmer" + String(d->num);
      if (f_name != "") ident = f_name + "." + ident;
      d->configLoad(cf, ident);
    }
  }

  void configSave(IotsaConfigFileSave& cf, const String& f_name) {
    for (auto d : dimmers) {
      String ident = "dimmer" + String(d->num);
      if (f_name != "") ident = f_name + "." + ident;
      d->configSave(cf, ident);
    }
  }

  bool formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) {
    bool anyChanged = false;
    for(auto d : dimmers) {
      String ident(d->num);
      if (f_name != "") ident = f_name + "." + ident;
      anyChanged |= d->formHandler_args(server, ident, includeConfig);
    }
    return anyChanged;
  }

  void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
    for (auto d : dimmers) {
      String ident(d->num);
      if (f_name != "") ident = f_name + "." + ident;
      String name = "Dimmer " + ident;
      d->formHandler_fields(message, name, ident, includeConfig);
    }
  }

  String info() {
    String message;
    for (auto d : dimmers) {
      message += d->info();
    }
    return message;
  }
protected:
  ItemVectorType dimmers;
  std::vector<DimmerUI *> dimmerUIs;
};

class LissabonRemoteMod : public IotsaApiMod, public DimmerCallbacks {
public:
  LissabonRemoteMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaRestApiMod(_app, _auth)
  {
    BLEDimmer *dimmer = new BLEDimmer(1, bleClientMod, this);
    dimmers.push_back(dimmer, true);
  #ifdef WITH_SECOND_DIMMER
    dimmer = new BLEDimmer(2, bleClientMod, this);
    dimmers.push_back(dimmer, true);
  #endif
  }
  void setup();
  void serverSetup();
  String info();
  void configLoad();
  void configSave();
  void loop();

protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  void unknownDeviceFound(BLEAdvertisedDevice& deviceAdvertisement);
private:
  void uiButtonChanged();
  void dimmerValueChanged();
  void handler();
  void startScanUnknown();
  void ledOn();
  void ledOff();
  DimmerCollection dimmers;
  uint32_t scanUnknownUntilMillis = 0;
  std::set<std::string> unknownDimmers;
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

void LissabonRemoteMod::startScanUnknown() {
  bleClientMod.findUnknownDevices(true);
  scanUnknownUntilMillis = millis() + 20000;
  iotsaConfig.postponeSleep(21000);
}

void LissabonRemoteMod::uiButtonChanged() {
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
  dimmers.formHandler_args(server, "", true);
#if 0
  String dimmer1name = String(dimmer1.num);
  anyChanged |= dimmer1.formHandler_args(server, dimmer1name, true);
#ifdef WITH_SECOND_DIMMER
  String dimmer2name = String(dimmer2.num);
  anyChanged |= dimmer2.formHandler_args(server, dimmer2name, true);
#endif // WITH_SECOND_DIMMER
#endif
  if (anyChanged) {
    configSave();
  }
  if (server->hasArg("scanUnknown")) startScanUnknown();

  String message = "<html><head><title>BLE Dimmers</title></head><body><h1>BLE Dimmers</h1>";
  message += "<h2>Dimmer Settings</h2><form>";
  dimmers.formHandler_fields(message, "", "", false);
  message += "<input type='submit' name='set' value='Set Dimmers'></form><br>";

  message += "<h2>Dimmer Configuration</h2><form>";
  dimmers.formHandler_fields(message, "", "", true);
  message += "<input type='submit' name='config' value='Configure Dimmers'></form><br>";

  // xxxjack fixed number of dimmers, so no need for "new" form.

  message += "<h2>Available Unknown/new BLE dimmer devices</h2>";
  message += "<form><input type='submit' name='scanUnknown' value='Scan for 20 seconds'></form>";
  if (unknownDimmers.size() == 0) {
    message += "<p>No unassigned BLE dimmer devices seen recently.</p>";
  } else {
    message += "<ul>";
    for (auto it: unknownDimmers) {
      message += "<li>" + String(it.c_str()) + "</li>";
    }
    message += "</ul>";
  }

  message += "<form><input type='submit' name='refresh' value='Refresh'></form>";
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
  dimmers.getHandler(reply);
  if (unknownDimmers.size()) {
    JsonArray unknownReply = reply.createNestedArray("unassigned");
    for (auto it : unknownDimmers) {
      unknownReply.add((char *)it.c_str());
    }
  }
  return true;
}

bool LissabonRemoteMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  // xxxjack need to distinguish between config and operational parameters
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (reqObj["scanUnknown"]|0) startScanUnknown();
  dimmers.putHandler(request);
  if (anyChanged) {
    configSave();
  }
  return anyChanged;
}

void LissabonRemoteMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/bledimmer", std::bind(&LissabonRemoteMod::handler, this));
#endif
  api.setup("/api/bledimmer", true, true);
  name = "bledimmer";
}


void LissabonRemoteMod::configLoad() {
  IotsaConfigFileLoad cf("/config/bledimmer.cfg");
  dimmers.configLoad(cf, "");
}

void LissabonRemoteMod::configSave() {
  IotsaConfigFileSave cf("/config/bledimmer.cfg");
  dimmers.configSave(cf, "");
}

void LissabonRemoteMod::setup() {
  configLoad();
  iotsaConfig.allowRCMDescription("tap any touchpad 4 times");
#ifdef LED_PIN
  pinMode(LED_PIN, OUTPUT);
#endif
  ledOn();
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  dimmers.ui_at(0)->setUpDownButtons(encoder1);
#ifdef WITH_SECOND_DIMMER
  dimmers.ui_at(1)->setUpDownButtons(encoder2);
#endif // WITH_SECOND_DIMMER

  auto callback = std::bind(&LissabonRemoteMod::unknownDeviceFound, this, std::placeholders::_1);
  bleClientMod.setUnknownDeviceFoundCallback(callback);
  bleClientMod.setServiceFilter(Lissabon::Dimmer::serviceUUID);
  ledOff();
}

void LissabonRemoteMod::unknownDeviceFound(BLEAdvertisedDevice& deviceAdvertisement) {
  IFDEBUG IotsaSerial.printf("unknownDeviceFound: iotsaLedstrip/iotsaDimmer \"%s\"\n", deviceAdvertisement.getName().c_str());
  unknownDimmers.insert(deviceAdvertisement.getName());
}

void LissabonRemoteMod::dimmerValueChanged() {
  saveAtMillis = millis() + 1000;
  ledOn();
}

void LissabonRemoteMod::loop() {
#ifdef DEBUG_PRINT_HEAP_SPACE
  { static uint32_t last; if (millis() > last+1000) { iotsaConfig.printHeapSpace(); last = millis(); }}
#endif
  if (scanUnknownUntilMillis != 0 && millis() > scanUnknownUntilMillis) {
    bleClientMod.findUnknownDevices(false);
    scanUnknownUntilMillis = 0;
  }
  // See whether we have a value to save (because the user has been turning the dimmer)
  if (saveAtMillis > 0 && millis() > saveAtMillis) {
    saveAtMillis = 0;
    configSave();
    ledOff();
  }
  for (auto d : dimmers) {
    d->loop();
  }
}

// Instantiate the Led module, and install it in the framework
LissabonRemoteMod remoteMod(application);

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
