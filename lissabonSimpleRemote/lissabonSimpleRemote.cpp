//
// Simplest possible lissabon remote: toggles a single remote lissabon
// dimmer (e.g. lissabonSimpleLight) on/off over BLE. No brightness level.
//
// Board: esp32dev. Reuses the built-in "PRG"/"BOOT" button (GPIO0) as the
// on/off pushbutton. The onboard LED (GPIO2) mirrors the *actual* on/off
// state of the remote dimmer, as last confirmed over BLE -- not the
// optimistic locally-requested state, and not persisted across reboot.
//
#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaInput.h"
#include "iotsaBLEClient.h"

#define LED_PIN 2     // Onboard LED on esp32dev
#define BUTTON_PIN 0  // PRG/BOOT button on esp32dev

IotsaApplication application("Lissabon Simple Remote");

IotsaBLEClientMod bleClientMod(application);

#include "DimmerBLEClient.h"
#include "DimmerUI.h"

using namespace Lissabon;

class LissabonSimpleRemoteMod : public IotsaModule, public DimmerCallbacks {
public:
  LissabonSimpleRemoteMod(IotsaApplication &_app)
  : IotsaModule(_app),
    dimmer(1, bleClientMod, this),
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
  String webTitle() override { return "Remote"; }
  void formHandler_args(IotsaWebContext& ctx) override { dimmer.formHandler_args(ctx); }
  void formHandler_fields(IotsaWebContext& ctx) override { dimmer.formHandler_fields(ctx); }
#endif
private:
  // Fires on the optimistic local button press, before the remote has
  // confirmed anything -- deliberately not used to drive the LED.
  void dimmerOnOffChanged() override {}
  // Fires once a value has actually been confirmed from the remote device
  // over BLE -- this is what drives the LED.
  void dimmerValueChanged() override;
  void dimmerAvailableChanged() override {}
  DimmerBLEClient dimmer;
  DimmerUI dimmerUI;
};

Button button(BUTTON_PIN, true, true, true);

void LissabonSimpleRemoteMod::dimmerValueChanged() {
  digitalWrite(LED_PIN, dimmer.isOn ? HIGH : LOW);
}

void LissabonSimpleRemoteMod::setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  configLoad();
  dimmerUI.setOnOffButton(button);
  dimmer.followDimmerChanges(true);
  dimmer.setup();
}

void LissabonSimpleRemoteMod::lateSetup() {
  name = "remote";
  // /remote (the module page, GET + POST) is auto-registered by this
  // get=true api.setup().
  api.setup("remote", true, true);
}

String LissabonSimpleRemoteMod::info() {
  return "<p>Simple BLE on/off remote. See <a href='/remote'>/remote</a>, or <a href='/api/remote'>/api/remote</a> for the REST API.</p>";
}

void LissabonSimpleRemoteMod::configLoad() {
  // Loads the target device's name/address (so we know which device to
  // connect to). isOn isn't saved: it comes from the device, once the first
  // BLE sync completes.
  IotsaConfigFileLoad cf("/config/remote.cfg");
  dimmer.configLoad(cf, "dimmer");
}

void LissabonSimpleRemoteMod::configSave() {
  IotsaConfigFileSave cf("/config/remote.cfg");
  dimmer.configSave(cf, "dimmer");
}

void LissabonSimpleRemoteMod::loop() {
  dimmer.loop();
}

// Instantiate the remote module, and install it in the framework
LissabonSimpleRemoteMod remoteMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
