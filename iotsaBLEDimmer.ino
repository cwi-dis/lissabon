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

IotsaApplication application("Iotsa BLE Dimmer");
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
Touchpad touch1down(12, true, true, true);
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

class IotsaBLEDimmerMod : public IotsaApiMod, public DimmerCallbacks {
public:
  IotsaBLEDimmerMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaRestApiMod(_app, _auth),
    dimmer1(1, bleClientMod, this),
    dimmer1ui(dimmer1)
#ifdef WITH_SECOND_DIMMER
    , 
    dimmer2(2, bleClientMod, this),
    dimmer2ui(dimmer2)
#endif
  {
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
  BLEDimmer dimmer1;
  DimmerUI dimmer1ui;
#ifdef WITH_SECOND_DIMMER
  BLEDimmer dimmer2;
  DimmerUI dimmer2ui;
#endif
  std::set<std::string> unknownDimmers;
  uint32_t saveAtMillis = 0;
  uint32_t ledOffUntilMillis = 0;
  uint32_t lastButtonChangeMillis = 0;
  int buttonChangeCount = 0;
};



void IotsaBLEDimmerMod::uiButtonChanged() {
  // Called whenever any button changed state.
  // Used to give visual feedback (led turning off) on presses and releases,
  // and to enable config mod after 4 taps and reboot after 8 taps
  uint32_t now = millis();
#ifdef LED_PIN
  digitalWrite(LED_PIN, HIGH);
  ledOffUntilMillis = now + 100;
#endif
  if (lastButtonChangeMillis > 0 && now < lastButtonChangeMillis + TAP_DURATION) {
    // A button change that was quick enough for a tap
    lastButtonChangeMillis = now;
    buttonChangeCount++;
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
  }
}
#ifdef IOTSA_WITH_WEB
void
IotsaBLEDimmerMod::handler() {
  bool anyChanged = false;
  anyChanged |= dimmer1.handlerConfigArgs(server);
  dimmer1.handlerArgs(server);
#ifdef WITH_SECOND_DIMMER
  anyChanged |= dimmer2.handlerConfigArgs(server);
  dimmer2.handlerArgs(server);
#endif // WITH_SECOND_DIMMER
  if (anyChanged) {
    configSave();
  }

  String message = "<html><head><title>BLE Dimmers</title></head><body><h1>BLE Dimmers</h1>";
  message += dimmer1.handlerForm();

#ifdef WITH_SECOND_DIMMER
  message += dimmer2.handlerForm();
#endif // WITH_SECOND_DIMMER
  message += dimmer1.handlerConfigForm();
#ifdef WITH_SECOND_DIMMER
  message += dimmer2.handlerConfigForm();
#endif // WITH_SECOND_DIMMER
  message += "<h2>Available BLE dimmer devices</h2>";
  if (unknownDimmers.size() == 0) {
    message += "<p>No unassigned BLE dimmer devices seen recently.</p>";
  } else {
    message += "<ul>";
    for (auto it: unknownDimmers) {
      message += "<li>" + String(it.c_str()) + "</li>";
    }
    message += "</ul>";
  }
  message += "</body></html>";
  server->send(200, "text/html", message);
}

String IotsaBLEDimmerMod::info() {

  String message = "Built with BLE Dimmer module. See <a href='/bledimmer'>/bledimmer</a> to change settings or <a href='/api/bledimmer'>/api/bledimmer</a> for REST API.<br>";
  message += dimmer1.info();
#ifdef WITH_SECOND_DIMMER
  message += dimmer2.info();
#endif // WITH_SECOND_DIMMER
  return message;
}
#endif // IOTSA_WITH_WEB

bool IotsaBLEDimmerMod::getHandler(const char *path, JsonObject& reply) {
  // xxxjack need to distinguish between config and operational parameters
  JsonObject dimmer1Reply = reply.createNestedObject("dimmer1");
  dimmer1.getHandler(dimmer1Reply);
#ifdef WITH_SECOND_DIMMER
  JsonObject dimmer2Reply = reply.createNestedObject("dimmer2");
  dimmer2.getHandler(dimmer2Reply);
#endif
  if (unknownDimmers.size()) {
    JsonArray unknownReply = reply.createNestedArray("unassigned");
    for (auto it : unknownDimmers) {
      unknownReply.add((char *)it.c_str());
    }
  }
  return true;
}

bool IotsaBLEDimmerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  // xxxjack need to distinguish between config and operational parameters
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  JsonVariant dimmer1Request = reqObj["dimmer1"];
  if (dimmer1Request) {
    if (dimmer1.putHandler(dimmer1Request)) anyChanged = true;
  }
#ifdef WITH_SECOND_DIMMER
  JsonVariant dimmer2Request = reqObj["dimmer2"];
  if (dimmer2Request) {
    if (dimmer2.putHandler(dimmer2Request)) anyChanged = true;
  }
#endif
  if (anyChanged) {
    configSave();
  }
  return anyChanged;
}

void IotsaBLEDimmerMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/bledimmer", std::bind(&IotsaBLEDimmerMod::handler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/bledimmer", true, true);
  name = "bledimmer";
#endif
}


void IotsaBLEDimmerMod::configLoad() {
  IotsaConfigFileLoad cf("/config/bledimmer.cfg");
  dimmer1.configLoad(cf);
#ifdef WITH_SECOND_DIMMER
  dimmer2.configLoad(cf);
#endif // WITH_SECOND_DIMMER
}

void IotsaBLEDimmerMod::configSave() {
  IotsaConfigFileSave cf("/config/bledimmer.cfg");
  dimmer1.configSave(cf);
#ifdef WITH_SECOND_DIMMER
  dimmer2.configSave(cf);
#endif // WITH_SECOND_DIMMER
}

void IotsaBLEDimmerMod::setup() {
  configLoad();
  iotsaConfig.allowRCMDescription("tap any touchpad 4 times");
#ifdef LED_PIN
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
#endif
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  dimmer1ui.setEncoder(encoder1);
  bool wantUnknownDevices = dimmer1.name == "";
#ifdef WITH_SECOND_DIMMER
  dimmer2ui.setEncoder(encoder2);
  if (dimmer2.name == "") wantUnknownDevices = true;
#endif // WITH_SECOND_DIMMER

  auto callback = std::bind(&IotsaBLEDimmerMod::unknownDeviceFound, this, std::placeholders::_1);
  bleClientMod.setUnknownDeviceFoundCallback(callback);
  bleClientMod.setServiceFilter(serviceUUID);
  bleClientMod.findUnknownDevices(wantUnknownDevices);
}

void IotsaBLEDimmerMod::unknownDeviceFound(BLEAdvertisedDevice& deviceAdvertisement) {
  IFDEBUG IotsaSerial.printf("unknownDeviceFound: iotsaLedstrip/iotsaDimmer \"%s\"\n", deviceAdvertisement.getName().c_str());
  unknownDimmers.insert(deviceAdvertisement.getName());
}

void IotsaBLEDimmerMod::dimmerValueChanged() {
  saveAtMillis = millis() + 1000;
}

void IotsaBLEDimmerMod::loop() {
#ifdef DEBUG_PRINT_HEAP_SPACE
  { static uint32_t last; if (millis() > last+1000) { iotsaConfig.printHeapSpace(); last = millis(); }}
#endif
  // See whether we have a value to save (because the user has been turning the dimmer)
  if (saveAtMillis > 0 && millis() > saveAtMillis) {
    saveAtMillis = 0;
    configSave();
  }
#ifdef LED_PIN
  // See whether we have to turn on the LED again (after a short off-period as feedback for user interaction)
  if (ledOffUntilMillis > 0 && millis() > ledOffUntilMillis) {
    digitalWrite(LED_PIN, LOW);
    ledOffUntilMillis = 0;
  }
#endif
  dimmer1.loop();
#ifdef WITH_SECOND_DIMMER
  dimmer2.loop();
#endif
}

// Instantiate the Led module, and install it in the framework
IotsaBLEDimmerMod bleDimmerControllerMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  heap_caps_check_integrity_all(true);
  application.loop();
}
