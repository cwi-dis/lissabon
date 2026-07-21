//
// Simplest possible lissabon light: a single on/off LED, controlled locally
// by a pushbutton and remotely by any lissabon BLE dimmer client (see
// lissabonSimpleRemote). No brightness level -- just on/off.
//
// Board: esp32dev. Reuses the built-in "PRG"/"BOOT" button (GPIO0) as the
// on/off pushbutton, and drives the onboard LED (GPIO2). GPIO2 can equally
// well drive a MOSFET gate instead, to switch a real load -- the onboard
// LED is just the zero-extra-hardware default.
//
#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaConfigFile.h"
#include "iotsaInput.h"

#define LED_PIN 2     // Onboard LED on esp32dev
#define BUTTON_PIN 0  // PRG/BOOT button on esp32dev

IotsaApplication application("Lissabon Simple Light");
IotsaWifiMod wifiMod(application);

#include "iotsaBLEServer.h"
IotsaBLEServerMod bleserverMod(application);

#include "AbstractDimmer.h"
#include "DimmerUI.h"
#include "DimmerBLEServer.h"

using namespace Lissabon;

//
// The simplest possible dimmer: just switches the LED on or off, no level.
// Needs DIMMER_WITHOUT_LEVEL (set in platformio.ini) so AbstractDimmer
// doesn't grow the brightness/gamma/animation fields it doesn't use.
//
class SimpleLightDimmer : public AbstractDimmer {
public:
  using AbstractDimmer::AbstractDimmer;
  void setup() override {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, isOn ? HIGH : LOW);
  }
  bool available() override { return true; }
  void loop() override {
    digitalWrite(LED_PIN, isOn ? HIGH : LOW);
  }
};

class LissabonSimpleLightMod : public IotsaApiMod, public DimmerCallbacks {
public:
  LissabonSimpleLightMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaApiMod(_app, _auth),
    dimmer(1, this),
    dimmerBLEServer(dimmer),
    dimmerUI(dimmer)
  {
  }
  void setup();
  void serverSetup();
  String info();
  void configLoad();
  void configSave();
  void loop();
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
private:
  void dimmerOnOffChanged() override {}
  void dimmerValueChanged() override {}
  void dimmerAvailableChanged() override {}
  SimpleLightDimmer dimmer;
  DimmerBLEServer dimmerBLEServer;
  DimmerUI dimmerUI;
  // isOn can also change through the BLE server (a remote turning us on/off),
  // which doesn't run through dimmerOnOffChanged() or putHandler() below --
  // so we poll for changes in loop() instead of hooking either of those.
  bool lastSavedIsOn = false;
};

Button button(BUTTON_PIN, true, true, true);

void LissabonSimpleLightMod::setup() {
  configLoad();
  lastSavedIsOn = dimmer.isOn;
  dimmerUI.setOnOffButton(button);
  dimmer.setup();
  dimmerBLEServer.setup();
}

void LissabonSimpleLightMod::serverSetup() {
  api.setup("/api/light", true, true);
  name = "light";
}

String LissabonSimpleLightMod::info() {
  return "Simple on/off light. See <a href='/api/light'>/api/light</a> for the REST API.<br>";
}

bool LissabonSimpleLightMod::getHandler(const char *path, JsonObject& reply) {
  dimmer.getHandler(reply);
  return true;
}

bool LissabonSimpleLightMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  return dimmer.putHandler(request);
}

void LissabonSimpleLightMod::configLoad() {
  IotsaConfigFileLoad cf("/config/light.cfg");
  dimmer.configLoad(cf, "dimmer");
}

void LissabonSimpleLightMod::configSave() {
  IotsaConfigFileSave cf("/config/light.cfg");
  dimmer.configSave(cf, "dimmer");
}

void LissabonSimpleLightMod::loop() {
  dimmer.loop();
  if (dimmer.isOn != lastSavedIsOn) {
    lastSavedIsOn = dimmer.isOn;
    configSave();
  }
}

// Instantiate the light module, and install it in the framework
LissabonSimpleLightMod lightMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
