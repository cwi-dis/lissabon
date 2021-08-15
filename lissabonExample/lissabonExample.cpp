//
// A Neopixel strip server, which allows control over a strip of neopixels, intended
// to be used as lighting. Color can be set as fraction RGB or HSL, gamma can be changed,
// interval between lit pixels can be changed. Control is through a web UI or
// through REST calls (and/or, depending on Iotsa compile time options, COAP calls).
// The web interface can be disabled by building iotsa with IOTSA_WITHOUT_WEB.
//
#include <Arduino.h>
#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaLed.h"
#include "iotsaConfigFile.h"
#include <set>

IotsaApplication application("Lissabon Sample");
IotsaWifiMod wifiMod(application);

#include "iotsaInput.h"
Touchpad touch1down(13, true, true, true);
Touchpad touch1up(14, true, true, true);
UpDownButtons encoder1(touch1down, touch1up, true);

Input* inputs[] = {
  &encoder1,
};

IotsaInputMod touchMod(application, inputs, sizeof(inputs)/sizeof(inputs[0]));

#include "iotsaBLEClient.h"
IotsaBLEClientMod bleClientMod(application);

#include "BLEDimmer.h"
#include "DimmerUI.h"

#ifdef WITH_BLESERVER
#include "DimmerBLEServer.h"
#endif

using namespace Lissabon;

class IotsaBLEDimmerMod : public IotsaApiMod, public DimmerCallbacks {
public:
  IotsaBLEDimmerMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaRestApiMod(_app, _auth),
    dimmer1(1, bleClientMod, this),
  #ifdef WITH_BLESERVER
    dimmer1BLEServer(dimmer1),
  #endif
    dimmer1ui(dimmer1)
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
  void dimmerOnOffChanged() override;
  void dimmerValueChanged() override;
  void dimmerAvailableChanged(bool available, bool connected) override {};
  void handler();
  BLEDimmer dimmer1;
#ifdef WITH_BLESERVER
  DimmerBLEServer dimmer1BLEServer;
#endif
  DimmerUI dimmer1ui;
  uint32_t saveAtMillis = 0;
  uint32_t ledOffUntilMillis = 0;
  uint32_t lastButtonChangeMillis = 0;
  int buttonChangeCount = 0;
};



void IotsaBLEDimmerMod::dimmerOnOffChanged() {

}

void
IotsaBLEDimmerMod::handler() {
  bool anyChanged = false;
  anyChanged |= dimmer1.formHandler_args(server, "dimmer", true);
  if (anyChanged) {
    configSave();
    dimmer1.updateDimmer();
  }

  String message = "<html><head><title>BLE Dimmers</title></head><body><h1>BLE Dimmers</h1>";
  message += "<h2>Settings</h2><form method='post'>";
  dimmer1.formHandler_fields(message, "dimmer", "dimmer", true);
  message += "<input type='submit' name='set' value='Submit'></form>";

  message += "</body></html>";
  server->send(200, "text/html", message);
}

String IotsaBLEDimmerMod::info() {

  String message = "Built with BLE Dimmer module. See <a href='/bledimmer'>/bledimmer</a> to change settings or <a href='/api/bledimmer'>/api/bledimmer</a> for REST API.<br>";
  message += dimmer1.info();
  return message;
}

bool IotsaBLEDimmerMod::getHandler(const char *path, JsonObject& reply) {
  // xxxjack need to distinguish between config and operational parameters
  JsonObject dimmer1Reply = reply.createNestedObject("dimmer1");
  dimmer1.getHandler(dimmer1Reply);
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
  if (anyChanged) {
    configSave();
  }
  if (anyChanged) dimmer1.updateDimmer(); // xxxjack or is this called already?
  return anyChanged;
}

void IotsaBLEDimmerMod::serverSetup() {
  server->on("/bledimmer", std::bind(&IotsaBLEDimmerMod::handler, this));
  server->on("/bledimmer", HTTP_POST, std::bind(&IotsaBLEDimmerMod::handler, this));
  api.setup("/api/bledimmer", true, true);
  name = "bledimmer";
}


void IotsaBLEDimmerMod::configLoad() {
  IotsaConfigFileLoad cf("/config/bledimmer.cfg");
  dimmer1.configLoad(cf, "dimmer");
}

void IotsaBLEDimmerMod::configSave() {
  IotsaConfigFileSave cf("/config/bledimmer.cfg");
  dimmer1.configSave(cf, "dimmer");
}

void IotsaBLEDimmerMod::setup() {
  configLoad();

  dimmer1ui.setUpDownButtons(encoder1);

  bleClientMod.setServiceFilter(Lissabon::Dimmer::serviceUUID);

  dimmer1.setup();

#ifdef WITH_BLESERVER
  dimmer1BLEServer.setup();
#endif

  dimmer1.updateDimmer();
}

void IotsaBLEDimmerMod::dimmerValueChanged() {
  saveAtMillis = millis() + 1000;
}

void IotsaBLEDimmerMod::loop() {
  // See whether we have a value to save (because the user has been turning the dimmer)
  if (saveAtMillis > 0 && millis() > saveAtMillis) {
    saveAtMillis = 0;
    configSave();
  }
  dimmer1.loop();
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
  application.loop();
}
