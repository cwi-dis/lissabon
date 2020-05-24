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
#include "PWMDimmer.h"
#include "DimmerUI.h"

// CHANGE: Add application includes and declarations here

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

IotsaApplication application("Iotsa Dimmer");
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
#define VBAT_100_PERCENT (12.0/11.0) // 100K and 1M resistors divide by 11, not 10...
IotsaBatteryMod batteryMod(application);

#define PIN_OUTPUT 2
#ifdef ESP32
#define CHANNEL_OUTPUT 0
#endif

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

//
// LED Lighting module. 
//

class IotsaDimmerMod : public IotsaApiMod, public IotsaBLEApiProvider, public DimmerCallbacks {
public:
  IotsaDimmerMod(IotsaApplication& _app, IotsaAUthenticationProvider *_auth=NULL)
  : IotsaApiMod(_app, _auth),
    dimmer(1, PIN_OUTPUT, this)
  #ifdef WITH_UI
    ,
    dimmerUI(dimmer)
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
  PWMDimmer dimmer;
#ifdef WITH_UI
  DimmerUI dimmerUI;
#endif
  //bool touchedOnOff();
  //bool changedValue();
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);

private:
  void setupPwm();
  void handler();
  uint32_t saveAtMillis;
};



#ifdef IOTSA_WITH_WEB
void
IotsaDimmerMod::handler() {
  // Handles the page that is specific to the Led module, greets the user and
  // optionally stores a new name to greet the next time.
  String error;
  bool anyChanged = false;
  if (server->hasArg("identify")) identify();
  if( server->hasArg("animation")) {
    int val = server->arg("animation").toInt();
    if (val != millisAnimationDuration) {
      millisAnimationDuration = val;
      anyChanged = true;
    }
  }
  if (server->hasArg("minLevel")) {
    float val = server->arg("minLevel").toFloat();
    if (val != minLevel) {
      minLevel = val;
      anyChanged = true;
    }
  }
  if (server->hasArg("gamma")) {
    float val = server->arg("gamma").toFloat();
    if (val != gamma) {
      gamma = val;
      anyChanged = true;
    }
  }
#ifdef ESP32
  if (server->hasArg("pwmFrequency")) {
    float val = server->arg("pwmFrequency").toFloat();
    if (val != pwmFrequency) {
      pwmFrequency = val;
      setupPwm();
      anyChanged = true;
    }
  }
#endif
  if( server->hasArg("illuminance")) {
    float val = server->arg("illuminance").toFloat();
    if (val != illum) {
      illum = val;
      isOn = (illum != 0);
      anyChanged = true;
    }
  }
  if( server->hasArg("isOn")) {
    bool val = server->arg("isOn").toInt();
    if (val != isOn) {
      isOn = val;
      anyChanged = true;
    }
  }

  if (anyChanged) {
    configSave();
    startAnimation();
  }
  
  String message = "<html><head><title>Dimmer</title></head><body><h1>Dimmer</h1>";
  if (error != "") {
    message += "<p><em>" + error + "</em></p>";
  }
  message += "<form method='get'><input type='submit' name='identify' value='identify'></form>";
  message += "<form method='get'>";
  message += "<form method='get'><input type='hidden' name='isOn' value='1'><input type='submit' value='Turn On'></form>";
  message += "<form method='get'>";
  message += "<form method='get'><input type='hidden' name='isOn' value='0'><input type='submit' value='Turn Off'></form>";
  message += "<form method='get'>";

  message += "Illuminance (0..1): <input type='text' name='illuminance' value='" + String(illum) +"'><br>";
  message += "Dimmer minimum (0..1): <input type='text' name='minLevel' value='" + String(minLevel) +"'><br>";
  message += "Animation duration (ms): <input type='text' name='animation' value='" + String(millisAnimationDuration) +"'><br>";
  message += "Gamma (1.0 or 2.2): <input type='text' name='gamma' value='" + String(gamma) +"'><br>";
#ifdef ESP32
  message += "PWM Frequency: <input type='text' name='pwmFrequency' value='" + String(pwmFrequency) +"'><br>";
#endif
  message += "<input type='submit' value='Set'></form></body></html>";
  server->send(200, "text/html", message);
}

String IotsaDimmerMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/dimmer\">/dimmer</a> for setting the light level.";
#ifdef IOTSA_WITH_REST
  rv += " Or use REST api at <a href='/api/dimmer'>/api/dimmer</a>.";
#endif
#ifdef IOTSA_WITH_BLE
  rv += " Or use BLE service " + String(serviceUUID) + ".";
#endif
  rv += "</p>";
  return rv;
}
#endif // IOTSA_WITH_WEB

bool IotsaDimmerMod::getHandler(const char *path, JsonObject& reply) {
  reply["illuminance"] = illum;
  reply["isOn"] = isOn;
  reply["animation"] = millisAnimationDuration;
  reply["gamma"] = gamma;
#ifdef ESP32
  reply["pwmFrequency"] = pwmFrequency;
#endif
  return true;
}

bool IotsaDimmerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (request["identify"]|0) identify();
  millisAnimationDuration = request["animation"]|millisAnimationDuration;
  if (request.containsKey("minLevel")) {
    minLevel = request["minLevel"];
  }
  if (request.containsKey("gamma")) {
    gamma = request["gamma"];
  }
  if (request.containsKey("illum")) {
    illum = request["illum"];
    isOn = true;
  }
  if (request.containsKey("isOn")) {
    isOn = request["isOn"];
  }
#ifdef ESP32
  if (request.containsKey("pwmFrequency")) {
    pwmFrequency = request["pwmFrequency"];
    setupPwm();
  }
#endif
  configSave();
  startAnimation();
  return true;
}

void IotsaDimmerMod::serverSetup() {
  // Setup the web server hooks for this module.
#ifdef IOTSA_WITH_WEB
  server->on("/dimmer", std::bind(&IotsaDimmerMod::handler, this));
#endif // IOTSA_WITH_WEB
  api.setup("/api/dimmer", true, true);
  name = "dimmer";
}


void IotsaDimmerMod::configLoad() {
  IotsaConfigFileLoad cf("/config/dimmer.cfg");
  int value;
  cf.get("isOn", value, 0);
  isOn = value;
  cf.get("illum", illum, 0.0);
  cf.get("animation", millisAnimationDuration, 500);
  cf.get("minLevel", minLevel, 0.1);
  cf.get("gamma", gamma, 1.0);
#ifdef ESP32
  cf.get("pwmFrequency", pwmFrequency, 5000.0);
#endif
}

void IotsaDimmerMod::configSave() {
  IotsaConfigFileSave cf("/config/dimmer.cfg");

  cf.put("illum", illum);
  cf.put("isOn", isOn);
  cf.put("animation", millisAnimationDuration);
  cf.put("minLevel", minLevel);
  cf.put("gamma", gamma);
#ifdef ESP32
  cf.put("pwmFrequency", pwmFrequency);
#endif
}

void IotsaDimmerMod::setupPwm() {
#ifdef ESP32
  ledcSetup(CHANNEL_OUTPUT, pwmFrequency, 8);
  ledcAttachPin(PIN_OUTPUT, CHANNEL_OUTPUT);
#else
  pinMode(PIN_OUTPUT, OUTPUT);
#endif
}

void IotsaDimmerMod::setup() {
  // Allow switching the dimmer to iotsa config mode over BLE.
  batteryMod.allowBLEConfigModeSwitch();
#ifdef PIN_VBAT
  batteryMod.setPinVBat(PIN_VBAT, VBAT_100_PERCENT);
#endif
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  configLoad();
  setupPwm();
  illumPrev = isOn ? illum : 0;
  startAnimation();
#ifdef WITH_UI
  encoder.setCallback(std::bind(&IotsaDimmerMod::changedValue, this));
#ifdef WITH_TOUCHPADS
  // Bind up/down buttons to variable illum, ranging from minLevel to 1.0 in 25 steps
  encoder.bindVar(illum, minLevel, 1.0, 0.02);
  encoder.bindStateVar(isOn);
  encoder.setStateCallback(std::bind(&IotsaDimmerMod::touchedOnOff, this));
#endif // WITH_TOUCHPADS
#ifdef WITH_ROTARY
  button.setCallback(std::bind(&IotsaDimmerMod::touchedOnOff, this));
  // Bind button to isOn (toggling it on every press)
  button.bindVar(isOn, true);
  // Bind rotary encoder to variable illum, ranging from minLevel to 1.0 in 100 steps
  encoder.bindVar(illum, minLevel, 1.0, 0.01);
  // And if the rotary encoder does more than 2 steps per second we speed up
  encoder.setAcceleration(500);
#endif // WITH_ROTARY
#endif // WITH_UI

#ifdef IOTSA_WITH_BLE
  // Set default advertising interval to be between 200ms and 600ms
  IotsaBLEServerMod::setAdvertisingInterval(300, 900);

  bleApi.setup(serviceUUID, this);
  static BLE2904 isOn2904;
  isOn2904.setFormat(BLE2904::FORMAT_UINT8);
  isOn2904.setUnit(0x2700);
  static BLE2901 isOn2901("On/Off");
  bleApi.addCharacteristic(isOnUUID, BLE_READ|BLE_WRITE, &isOn2904, &isOn2901);

  static BLE2904 illum2904;
  illum2904.setFormat(BLE2904::FORMAT_UINT8);
  illum2904.setUnit(0x27AD);
  static BLE2901 illum2901("Illumination");
  bleApi.addCharacteristic(illumUUID, BLE_READ|BLE_WRITE, &illum2904, &illum2901);

  static BLE2904 identify2904;
  identify2904.setFormat(BLE2904::FORMAT_UINT8);
  identify2904.setUnit(0x2700);
  static BLE2901 identify2901("Identify");
  bleApi.addCharacteristic(identifyUUID, BLE_WRITE, &identify2904, &identify2901);
#endif
}

void IotsaDimmerMod::loop() {
  // See if we have a value to save (because the user has been turning the dimmer)
  if (saveAtMillis > 0 && millis() > saveAtMillis) {
    saveAtMillis = 0;
    configSave();
  }
  // Quick return if we have nothing to do
  if (millisAnimationStart == 0 || millisAnimationEnd == 0) {
    // The dimmer shouldn't sleep if it is controlling the PWM output
    if (illum > 0 && isOn) iotsaConfig.postponeSleep(100);
    return;
  }
  // Determine how far along the animation we are, and terminate the animation when done (or if it looks preposterous)
  uint32_t thisDur = millisAnimationEnd - millisAnimationStart;
  if (thisDur == 0) thisDur = 1;
  float progress = float(millis() - millisAnimationStart) / float(thisDur);
  float wantedIllum = illum;
  if (!isOn) wantedIllum = 0;
  if (progress < 0) progress = 0;
  if (progress >= 1) {
    progress = 1;
    millisAnimationStart = 0;
    illumPrev = wantedIllum;

    IFDEBUG IotsaSerial.printf("IotsaDimmer: wantedIllum=%f illum=%f\n", wantedIllum, illum);
  }
  float curIllum = wantedIllum*progress + illumPrev*(1-progress);
  if (curIllum < 0) curIllum = 0;
  if (curIllum > 1) curIllum = 1;
  if (gamma && gamma != 1.0) curIllum = powf(curIllum, gamma);
#ifdef ESP32
  ledcWrite(CHANNEL_OUTPUT, int(255*curIllum));
#else
  analogWrite(PIN_OUTPUT, int(255*curIllum));
#endif
}

bool IotsaDimmerMod::touchedOnOff() {
  // Start the animation to get to the wanted value
  startAnimation();
  // And prepare for saving (because we don't want to wear out the Flash chip)
  iotsaConfig.postponeSleep(2000);
  saveAtMillis = millis() + 2000;
  return true;
}

bool IotsaDimmerMod::changedValue() {
  // Changing the dimmer automatically turns on the lamp as well
  isOn = true;
  // Start the animation to get to the wanted value
  startAnimation();
  // And prepare for saving (because we don't want to wear out the Flash chip)
  iotsaConfig.postponeSleep(2000);
  saveAtMillis = millis() + 2000;
  return true;
}

void IotsaDimmerMod::identify() {
#ifdef ESP32
  ledcWrite(CHANNEL_OUTPUT, 128);
  delay(100);
  ledcWrite(CHANNEL_OUTPUT, 0);
  delay(100);
  ledcWrite(CHANNEL_OUTPUT, 128);
  delay(100);
  ledcWrite(CHANNEL_OUTPUT, 0);
  delay(100);
#else
  analogWrite(PIN_OUTPUT, 128);
  delay(100);
  analogWrite(PIN_OUTPUT, 0);
  delay(100);
  analogWrite(PIN_OUTPUT, 128);
  delay(100);
  analogWrite(PIN_OUTPUT, 0);
  delay(100);
#endif
  startAnimation();
}

// Instantiate the Led module, and install it in the framework
IotsaDimmerMod dimmerMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}

