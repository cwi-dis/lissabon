//
// A dimmer server, which allows controling a PWM (pulse-width modulation)
// DC dimmer.
//

#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaLed.h"
#include "iotsaConfigFile.h"
#include "PWMDimmer.h"
#include "DimmerUI.h"

//
// Device can be rebooted or configuration mode can be requested by quickly tapping any button.
// TAP_DURATION sets the maximum time between press-release-press-etc.
#define TAP_COUNT_MODE_CHANGE 4
#define TAP_COUNT_REBOOT 8
#define TAP_DURATION 1000

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

IotsaApplication application("Lissabon Dimmer");
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
//#define PIN_VBAT 37
//#define VBAT_100_PERCENT (12.0/11.0) // 100K and 1M resistors divide by 11, not 10...
IotsaBatteryMod batteryMod(application);

// Pin to which MOSFET is attached. Channel is only relevant for esp32.
#define PIN_PWM_DIMMER 2
#define CHANNEL_PWM_DIMMER 0

#include "iotsaInput.h"
// Define WITH_TOUCHPADS to enable user interface consisting of two touchpads (off/down and on/up)
// #define WITH_TOUCHPADS
// Define WITH_ROTARY to enable user interface consisting of a rotary encoder (up/down) and a button (on/off)
// #define WITH_ROTARY
#ifdef WITH_TOUCHPADS
#define WITH_UI
// Two touchpad pins: off/decrement (long press), on/increment (long press)
Touchpad touchdown(12, true, true, true);
Touchpad touchup(13, true, true, true);
UpDownButtons encoder(touchdown, touchup, true);

Input* inputs[] = {
  &encoder
};
#endif // WITH_TOUCHPADS
#ifdef WITH_ROTARY
#define WITH_UI
// A rotary encoder for increment/decrement and a button for on/off.
Button button(4, true, false, true);
RotaryEncoder encoder(16, 17);

Input* inputs[] = {
  &button,
  &encoder
};
#endif // WITH_ROTARY


#ifdef WITH_UI
IotsaInputMod inputMod(application, inputs, sizeof(inputs)/sizeof(inputs[0]));
#endif

#include "DimmerBLEServer.h"

using namespace Lissabon;

//
// PWM Lighting module. 
//

class LissabonDimmerMod : public IotsaApiMod, public DimmerCallbacks {
public:
  LissabonDimmerMod(IotsaApplication& _app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaApiMod(_app, _auth),
    dimmer(1, PIN_PWM_DIMMER, CHANNEL_PWM_DIMMER, this),
  #ifdef WITH_UI
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
  void handler();
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);

private:
  PWMDimmer dimmer;
#ifdef WITH_UI
  DimmerUI dimmerUI;
#endif
  DimmerBLEServer dimmerBLEServer;
  uint32_t saveAtMillis = 0;
  uint32_t lastButtonChangeMillis = 0;
  int buttonChangeCount = 0;
};

void LissabonDimmerMod::uiButtonChanged() {
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
LissabonDimmerMod::handler() {
  bool anyChanged = false;
  anyChanged |= dimmer.handlerConfigArgs(server);
  dimmer.handlerArgs(server);

  if (anyChanged) {
    configSave();
    dimmer.updateDimmer();
  }
  
  String message = "<html><head><title>Dimmer</title></head><body><h1>Dimmer</h1>";
  message += dimmer.handlerForm();
  message += dimmer.handlerConfigForm();
  message += "</body></html>";
  server->send(200, "text/html", message);
}

String LissabonDimmerMod::info() {
  // Return some information about this module, for the main page of the web server.
  String message = "<p>";
  message += dimmer.info();
  message += "See <a href=\"/dimmer\">/dimmer</a> for setting the light level.";
#ifdef IOTSA_WITH_REST
  message += " Or use REST api at <a href='/api/dimmer'>/api/dimmer</a>.";
#endif
#ifdef IOTSA_WITH_BLE
  message += " Or use BLE service " + String(Lissabon::Dimmer::serviceUUIDstring) + ".";
#endif
  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

bool LissabonDimmerMod::getHandler(const char *path, JsonObject& reply) {
  return dimmer.getHandler(reply);
}

bool LissabonDimmerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  bool configChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (!reqObj) return false;
  if (dimmer.putHandler(reqObj)) anyChanged = true;
  if (dimmer.putConfigHandler(reqObj)) configChanged = true;
  if (configChanged) {
    configSave();
  }
  if (anyChanged) dimmer.updateDimmer(); // xxxjack or is this called already?
  return anyChanged|configChanged;

}

void LissabonDimmerMod::serverSetup() {
  // Setup the web server hooks for this module.
#ifdef IOTSA_WITH_WEB
  server->on("/dimmer", std::bind(&LissabonDimmerMod::handler, this));
#endif // IOTSA_WITH_WEB
  api.setup("/api/dimmer", true, true);
  name = "dimmer";
}


void LissabonDimmerMod::configLoad() {
  IotsaConfigFileLoad cf("/config/pwmdimmer.cfg");
  dimmer.configLoad(cf);
}

void LissabonDimmerMod::configSave() {
  IotsaConfigFileSave cf("/config/pwmdimmer.cfg");
  dimmer.configSave(cf);

}


void LissabonDimmerMod::setup() {
  // Allow switching the dimmer to iotsa config mode over BLE or with taps
  batteryMod.allowBLEConfigModeSwitch();
  // Set pins for measuring battery voltage and disabling sleep.
#ifdef PIN_VBAT
  batteryMod.setPinVBat(PIN_VBAT, VBAT_100_PERCENT);
#endif
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  configLoad();
#ifdef WITH_TOUCHPADS
  iotsaConfig.allowRCMDescription("tap any touchpad 4 times");
  dimmerUI.setUpDownButtons(encoder);
#endif
#ifdef WITH_ROTARY
  iotsaConfig.allowRCMDescription("press button 4 times");
  dimmerUI.setRotaryEncoder(button, encoder);
#endif
  dimmer.setup();
  dimmerBLEServer.setup();
  dimmer.updateDimmer();
}

void LissabonDimmerMod::loop() {
  // See if we have a value to save (because the user has been turning the dimmer)
  if (saveAtMillis > 0 && millis() > saveAtMillis) {
    saveAtMillis = 0;
    configSave();
  }
  dimmer.loop();
}

void LissabonDimmerMod::dimmerValueChanged() {
  iotsaConfig.postponeSleep(2000);
  saveAtMillis = millis() + 1000;
}
// Instantiate the Led module, and install it in the framework
LissabonDimmerMod dimmerMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}

