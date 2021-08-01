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
Button button(0, true, true, true);

Input* inputs[] = {
  &button,
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
    dimmer(1, bleClientMod, this),
    dimmerui(dimmer)
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
  void handler();
  BLEDimmer dimmer;
  DimmerUI dimmerui;
  uint32_t saveAtMillis = 0;
  uint32_t ledOffUntilMillis = 0;
  uint32_t lastButtonChangeMillis = 0;
  int buttonChangeCount = 0;
};



void IotsaBLEDimmerMod::dimmerOnOffChanged() {
  saveAtMillis = millis();
}

void
IotsaBLEDimmerMod::handler() {
  bool anyChanged = false;
  anyChanged |= dimmer.formHandler_args(server, "dimmer", true);
  if (anyChanged) {
    configSave();
  }

  String message = "<html><head><title>BLE Dimmers</title></head><body><h1>BLE Dimmers</h1>";
  message += "<h2>Settings</h2><form method='post'>";
  dimmer.formHandler_fields(message, "dimmer", "dimmer", true);
  message += "<input type='submit' name='set' value='Submit'></form>";

  message += "</body></html>";
  server->send(200, "text/html", message);
}

String IotsaBLEDimmerMod::info() {

  String message = "Built with BLE Dimmer module. See <a href='/bledimmer'>/bledimmer</a> to change settings or <a href='/api/bledimmer'>/api/bledimmer</a> for REST API.<br>";
  message += dimmer.info();
  return message;
}

bool IotsaBLEDimmerMod::getHandler(const char *path, JsonObject& reply) {
  // xxxjack need to distinguish between config and operational parameters
  JsonObject dimmerReply = reply.createNestedObject("dimmer");
  dimmer.getHandler(dimmerReply);
  return true;
}

bool IotsaBLEDimmerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  // xxxjack need to distinguish between config and operational parameters
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  JsonVariant dimmerRequest = reqObj["dimmer"];
  if (dimmerRequest) {
    if (dimmer.putHandler(dimmerRequest)) anyChanged = true;
  }
  if (anyChanged) {
    configSave();
  }
  if (anyChanged) dimmer.updateDimmer();
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
  dimmer.configLoad(cf, "dimmer");
}

void IotsaBLEDimmerMod::configSave() {
  IotsaConfigFileSave cf("/config/bledimmer.cfg");
  dimmer.configSave(cf, "dimmer");
}

void IotsaBLEDimmerMod::setup() {
  configLoad();

  dimmerui.setOnOffButton(button);

  bleClientMod.setServiceFilter(Lissabon::Dimmer::serviceUUID);

  dimmer.setup();

  dimmer.updateDimmer();
}

void IotsaBLEDimmerMod::dimmerValueChanged() {
}

void IotsaBLEDimmerMod::loop() {
  // See whether we have a value to save (because the user has been turning the dimmer)
  if (saveAtMillis > 0 && millis() > saveAtMillis) {
    saveAtMillis = 0;
    configSave();
  }
  dimmer.loop();
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
