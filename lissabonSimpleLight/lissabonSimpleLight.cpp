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
#include "iotsaConfigFile.h"
#include "iotsaInput.h"

#define LED_PIN 2     // Onboard LED on esp32dev
#define BUTTON_PIN 0  // PRG/BOOT button on esp32dev

IotsaApplication application("Lissabon Simple Light");

#include "iotsaRunmode.h"

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
  // Two quick flashes, then back to the current state (loop() restores it).
  void identify() override {
    for (int i = 0; i < 2; i++) {
      digitalWrite(LED_PIN, !isOn ? HIGH : LOW);
      delay(100);
      digitalWrite(LED_PIN, isOn ? HIGH : LOW);
      delay(100);
    }
  }
};

class LissabonSimpleLightMod : public IotsaModule, public DimmerCallbacks {
public:
  LissabonSimpleLightMod(IotsaApplication &_app)
  : IotsaModule(_app),
    dimmer(1, this),
    dimmerBLEServer(dimmer),
    dimmerUI(dimmer)
  {
  }
  void setup();
  void lateSetup() override;
  String info();
  void configLoad();
  void configSave();
  void loop();
protected:
  void getHandler(IotsaApiContext& ctx) override { dimmer.getHandler(ctx); }
  void putHandler(IotsaApiContext& ctx) override { dimmer.putHandler(ctx); }
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Light"; }
  void formHandler_args(IotsaWebContext& ctx) override { dimmer.formHandler_args(ctx); }
  void formHandler_fields(IotsaWebContext& ctx) override { dimmer.formHandler_fields(ctx); }
#endif
private:
  void dimmerOnOffChanged() override {}
  void dimmerValueChanged() override {}
  void dimmerAvailableChanged() override {}
  SimpleLightDimmer dimmer;
  DimmerBLEServer dimmerBLEServer;
  DimmerUI dimmerUI;
  // isOn can also change through the BLE server (a remote turning us on/off),
  // which doesn't run through dimmerOnOffChanged() or the handlers (where
  // the framework saves) -- so we poll for changes in loop() instead.
  bool lastSavedIsOn = false;
};

Button button(BUTTON_PIN, true, true, true);

void LissabonSimpleLightMod::setup() {
  configLoad();
  lastSavedIsOn = dimmer.isOn;
  dimmerUI.setOnOffButton(button);
  dimmer.setup();
  dimmerBLEServer.setup();
  // iotsa's device identify (REST /api/runmode, web, BLE) flashes our light.
  if (IotsaRunmodeMod::instance()) IotsaRunmodeMod::instance()->addIdentifyCallback([this]() { dimmer.identify(); });
}

void LissabonSimpleLightMod::lateSetup() {
  name = "light";
  // /light (the module page, GET + POST) is auto-registered by this
  // get=true api.setup().
  api.setup("light", true, true);
}

String LissabonSimpleLightMod::info() {
  return "<p>Simple on/off light. See <a href='/light'>/light</a>, or <a href='/api/light'>/api/light</a> for the REST API.</p>";
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
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
