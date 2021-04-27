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
#include "LedstripDimmer.h"
#include "DimmerUI.h"


//
// Device can be rebooted or configuration mode can be requested by quickly tapping any button.
// TAP_DURATION sets the maximum time between press-release-press-etc.
#define TAP_COUNT_MODE_CHANGE 4
#define TAP_COUNT_REBOOT 8
#define TAP_DURATION 1000

// Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.
#define WITH_OTA

// Define this to enable support for touchpads to control the led strip (otherwise only BLE/REST/WEB control)
#define WITH_TOUCHPADS

IotsaApplication application("Lissabon LEDstrip");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

#include "iotsaBLEServer.h"
#ifdef IOTSA_WITH_BLE
IotsaBLEServerMod bleserverMod(application);
#else
#warning Building this module without BLE support may be a bit pointless
#endif

#include "iotsaBattery.h"
#define PIN_DISABLESLEEP 0
#define PIN_VBAT 35
#define VBAT_100_PERCENT (12.7/10.0) // 100K and 1M resistors divide by 11, not 10...
#define VBAT_0_PERCENT (10.5/10.0) // 100K and 1M resistors divide by 11, not 10...
IotsaBatteryMod batteryMod(application);

#include "iotsaPixelstrip.h"
IotsaPixelstripMod pixelstripMod(application);


#ifdef WITH_TOUCHPADS
#include "iotsaInput.h"
Touchpad upTouch(32, true, true, true);
Touchpad downTouch(13, true, true, true);
UpDownButtons levelEncoder(upTouch, downTouch, true);
Touchpad upTempTouch(14, true, true, true);
Touchpad downTempTouch(15, true, true, true);
UpDownButtons temperatureEncoder(upTempTouch, downTempTouch, false);
//Touchpad(15, true, false, true);

Input* inputs[] = {
  &levelEncoder,
  &temperatureEncoder
};

IotsaInputMod inputMod(application, inputs, sizeof(inputs)/sizeof(inputs[0]));
#endif // WITH_TOUCHPADS

#include "DimmerBLEServer.h"

using namespace Lissabon;

//
// LED Lighting module. 
//

class LissabonLedstripMod : public IotsaApiMod, public DimmerCallbacks 
{
public:
  LissabonLedstripMod(IotsaApplication& _app, IotsaAuthenticationProvider* _auth=NULL)
  : IotsaApiMod(_app, _auth),
    dimmer(1, pixelstripMod, this),
#ifdef WITH_TOUCHPADS
    dimmerUI(dimmer),
#endif
    dimmerBLEServer(dimmer)
  {
  }
  void setup();
  void serverSetup();
  String info();
  void configLoad();
  void configSave();
  void loop();

protected:
  void uiButtonChanged();
  void dimmerValueChanged();
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
private:
  LedstripDimmer dimmer;
#ifdef WITH_TOUCHPADS
  DimmerUI dimmerUI;
#endif
  DimmerBLEServer dimmerBLEServer;
  void handler();

  uint32_t saveAtMillis = 0;
  uint32_t lastButtonChangeMillis = 0;
  int buttonChangeCount = 0;
};

void LissabonLedstripMod::uiButtonChanged() {
  // Called whenever any button changed state.
  // Used to give visual feedback (led turning off) on presses and releases,
  // and to enable config mod after 4 taps and reboot after 8 taps
  uint32_t now = millis();
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
LissabonLedstripMod::handler() {
  bool anyChanged = false;
  anyChanged |= dimmer.formHandler_args(server, "ledstrip", true);

  if (anyChanged) {
    configSave();
    dimmer.updateDimmer();
  }
  
  
  String message = "<html><head><title>Lissabon Ledstrip</title></head><body><h1>Lissabon Ledstrip</h1>";
  message += "<h2>Settings</h2><form>";
  dimmer.formHandler_fields(message, "ledstrip", "ledstrip", true);
  message += "<input type='submit' name='set' value='Submit'></form>";
  message += "</body></html>";
  server->send(200, "text/html", message);
}

String LissabonLedstripMod::info() {
  // Return some information about this module, for the main page of the web server.
  String message = "<p>";
  message += dimmer.info();
  message += "See <a href=\"/ledstrip\">/ledstrip</a> for setting the LEDstrip level and temperature.";
#ifdef IOTSA_WITH_REST
  message += " Or use REST api at <a href='/api/ledstrip'>/api/ledstrip</a>.";
#endif
#ifdef IOTSA_WITH_BLE
  message += " Or use BLE service " + String(Lissabon::Dimmer::serviceUUIDstring) + ".";
#endif
  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

bool LissabonLedstripMod::getHandler(const char *path, JsonObject& reply) {
  dimmer.getHandler(reply);
  return true;
}

bool LissabonLedstripMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (!reqObj) return false;
  if (dimmer.putHandler(reqObj)) anyChanged = true;
  if (anyChanged) {
    // Should do this only for configuration changes
    configSave();
  }
  if (anyChanged) dimmer.updateDimmer(); // xxxjack or is this called already?
  return anyChanged;

}
void LissabonLedstripMod::serverSetup() {
  // Setup the web server hooks for this module.
#ifdef IOTSA_WITH_WEB
  server->on("/ledstrip", std::bind(&LissabonLedstripMod::handler, this));
#endif // IOTSA_WITH_WEB
  api.setup("/api/ledstrip", true, true);
  name = "ledstrip";
}


void LissabonLedstripMod::configLoad() {
  IotsaConfigFileLoad cf("/config/ledstrip.cfg");
  dimmer.configLoad(cf, "ledstrip");
}

void LissabonLedstripMod::configSave() {
  IotsaConfigFileSave cf("/config/ledstrip.cfg");
  dimmer.configSave(cf, "ledstrip");
}

void LissabonLedstripMod::setup() {
  // Allow switching the dimmer to iotsa config mode over BLE or with taps
  batteryMod.allowBLEConfigModeSwitch();
#ifdef PIN_VBAT
  batteryMod.setPinVBat(PIN_VBAT, VBAT_100_PERCENT, VBAT_0_PERCENT);
#endif
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  configLoad();
#ifdef WITH_TOUCHPADS
  iotsaConfig.allowRCMDescription("tap any touchpad 4 times");
  dimmerUI.setUpDownButtons(levelEncoder);
  dimmerUI.setTemperatureUpDownButtons(temperatureEncoder);
#endif
  dimmer.setup();
  dimmerBLEServer.setup();
  dimmer.updateDimmer();
}

void LissabonLedstripMod::dimmerValueChanged() {
  iotsaConfig.postponeSleep(2000);
  saveAtMillis = millis() + 1000;
}

void LissabonLedstripMod::loop() {
  // See if we have a value to save (because the user has been turning the dimmer)
  if (saveAtMillis > 0 && millis() > saveAtMillis) {
    saveAtMillis = 0;
    IFDEBUG IotsaSerial.println("save ledstrip config");
    configSave();
  }
  dimmer.loop();
}

// Instantiate the Led module, and install it in the framework
LissabonLedstripMod ledstripMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}

