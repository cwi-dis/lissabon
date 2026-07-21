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
#include "iotsaWifi.h"
#include "iotsaConfigFile.h"
#include "iotsaInput.h"
#include "iotsaBLEClient.h"

#define LED_PIN 2     // Onboard LED on esp32dev
#define BUTTON_PIN 0  // PRG/BOOT button on esp32dev

IotsaApplication application("Lissabon Simple Remote");
IotsaWifiMod wifiMod(application);

IotsaBLEClientMod bleClientMod(application);

#include "BLEDimmer.h"
#include "DimmerUI.h"

using namespace Lissabon;

class LissabonSimpleRemoteMod : public IotsaApiMod, public DimmerCallbacks {
public:
  LissabonSimpleRemoteMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaApiMod(_app, _auth),
    dimmer(1, bleClientMod, this),
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
  // Fires on the optimistic local button press, before the remote has
  // confirmed anything -- deliberately not used to drive the LED.
  void dimmerOnOffChanged() override {}
  // Fires once a value has actually been confirmed from the remote device
  // over BLE -- this is what drives the LED.
  void dimmerValueChanged() override;
  void dimmerAvailableChanged() override {}
  BLEDimmer dimmer;
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
  bleClientMod.setServiceFilter(Lissabon::Dimmer::serviceUUID);
  dimmer.followDimmerChanges(true);
  dimmer.setup();
}

void LissabonSimpleRemoteMod::serverSetup() {
  api.setup("/api/remote", true, true);
  name = "remote";
}

String LissabonSimpleRemoteMod::info() {
  return "Simple BLE on/off remote. See <a href='/api/remote'>/api/remote</a> for the REST API.<br>";
}

bool LissabonSimpleRemoteMod::getHandler(const char *path, JsonObject& reply) {
  dimmer.getHandler(reply);
  return true;
}

bool LissabonSimpleRemoteMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = dimmer.putHandler(request);
  if (anyChanged) configSave();
  return anyChanged;
}

void LissabonSimpleRemoteMod::configLoad() {
  // Loads the target device's name/address (so we know which device to
  // connect to). Also loads a default isOn, but that's immediately
  // superseded by the real value once the first BLE sync completes.
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
  application.serverSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
