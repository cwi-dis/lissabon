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
#endif

#include "iotsaBattery.h"
#define PIN_DISABLESLEEP 0
#define PIN_VBAT 37
#define VBAT_100_PERCENT (12.0/11.0) // 100K and 1M resistors divide by 11, not 10...
IotsaBatteryMod batteryMod(application);

#include "iotsaInput.h"
Input* inputs[] = {
};

IotsaInputMod inputMod(application, inputs, sizeof(inputs)/sizeof(inputs[0]));

//
// LED Lighting module. 
//

class IotsaDimmerMod : public IotsaApiMod, public IotsaBLEApiProvider {
public:
  using IotsaApiMod::IotsaApiMod;
  void setup();
  void serverSetup();
  String info();
  void configLoad();
  void configSave();
  void loop();

protected:
  bool touchedOnOff();
  bool changedValue();
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
#ifdef IOTSA_WITH_BLE
  IotsaBleApiService bleApi;
  bool blePutHandler(UUIDstring charUUID);
  bool bleGetHandler(UUIDstring charUUID);
  static constexpr UUIDstring serviceUUID = "F3390001-F793-4D0C-91BB-C91EEB92A1A4";
  static constexpr UUIDstring isOnUUID = "F3390002-F793-4D0C-91BB-C91EEB92A1A4";
  static constexpr UUIDstring identifyUUID = "F3390003-F793-4D0C-91BB-C91EEB92A1A4";
  static constexpr UUIDstring illumUUID = "F3390004-F793-4D0C-91BB-C91EEB92A1A4";
//  static constexpr UUIDstring tempUUID = "F3390005-F793-4D0C-91BB-C91EEB92A1A4";
//  static constexpr UUIDstring intervalUUID = "F3390006-F793-4D0C-91BB-C91EEB92A1A4";
#endif // IOTSA_WITH_BLE
private:
  void handler();
  void startAnimation();
  void identify();
  bool isOn;
  float illum;
  float illumPrev;
  uint32_t millisStartAnimation;
  int millisAnimationDuration;
};

void IotsaDimmerMod::startAnimation() {
  millisStartAnimation = millis();
  iotsaConfig.postponeSleep(millisAnimationDuration+100);
}


#ifdef IOTSA_WITH_BLE
bool IotsaDimmerMod::blePutHandler(UUIDstring charUUID) {
  bool anyChanged = false;
  if (charUUID == illumUUID) {
      int _illum = bleApi.getAsInt(illumUUID);
      illum = float(_illum)/100.0;
      IFDEBUG IotsaSerial.printf("xxxjack ble: wrote illum %s value %d %f\n", illumUUID, _illum, illum);
      anyChanged = true;
  }
  if (charUUID == isOnUUID) {
    int value = bleApi.getAsInt(isOnUUID);
    isOn = (bool)value;
    anyChanged = true;
  }
  if (charUUID == identifyUUID) {
    int value = bleApi.getAsInt(identifyUUID);
    if (value) identify();
  }
  if (anyChanged) {
    configSave();
    startAnimation();
    return true;
  }
  IotsaSerial.println("IotsaDimmerMod: ble: write unknown uuid");
  return false;
}

bool IotsaDimmerMod::bleGetHandler(UUIDstring charUUID) {
  if (charUUID == illumUUID) {
      int _illum = int(illum*100);
      IFDEBUG IotsaSerial.printf("xxxjack ble: read illum %s value %d\n", charUUID, _illum);
      bleApi.set(illumUUID, (uint8_t)_illum);
      return true;
  }
  if (charUUID == isOnUUID) {
      IFDEBUG IotsaSerial.printf("xxxjack ble: read isOn %s value %d\n", charUUID, isOn);
      bleApi.set(isOnUUID, (uint8_t)isOn);
      return true;
  }
  IotsaSerial.println("IotsaDimmerMod: ble: read unknown uuid");
  return false;
}
#endif // IOTSA_WITH_BLE

#ifdef IOTSA_WITH_WEB
void
IotsaDimmerMod::handler() {
  // Handles the page that is specific to the Led module, greets the user and
  // optionally stores a new name to greet the next time.
  String error;
  bool anyChanged = false;
  if (server->hasArg("identify")) identify();
  if( server->hasArg("animation")) {
    millisAnimationDuration = server->arg("animation").toInt();
    anyChanged = true;
  }
  if( server->hasArg("illuminance")) {
    illum = server->arg("illuminance").toFloat();
    isOn = (illum != 0);
    anyChanged = true;
  }
  if( server->hasArg("isOn")) {
    isOn = server->arg("white").toInt();
    anyChanged = true;
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
  message += "<input type='submit' value='Set'></form></body></html>";
  server->send(200, "text/html", message);
}

String IotsaDimmerMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/dimmer\">/led</a> for setting the light level.";
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
  return true;
}

bool IotsaDimmerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (request["identify"]|0) identify();
  millisAnimationDuration = request["animation"]|millisAnimationDuration;
  if (request.containsKey("illum")) {
    illum = request["illum"];
    isOn = true;
  }
  if (request.containsKey("isOn")) {
    isOn = request["isOn"];
  }
  configSave();
  startAnimation();
  return true;
}

void IotsaDimmerMod::serverSetup() {
  // Setup the web server hooks for this module.
#ifdef IOTSA_WITH_WEB
  server->on("/ledstrip", std::bind(&IotsaDimmerMod::handler, this));
#endif // IOTSA_WITH_WEB
  api.setup("/api/ledstrip", true, true);
  name = "ledstrip";
}


void IotsaDimmerMod::configLoad() {
  IotsaConfigFileLoad cf("/config/dimmer.cfg");
  int value;
  cf.get("isOn", value, 0);
  isOn = value;
  cf.get("illum", illum, 0.0);
}

void IotsaDimmerMod::configSave() {
  IotsaConfigFileSave cf("/config/dimmer.cfg");

  cf.put("illum", illum);
  cf.put("isOn", isOn);
}

void IotsaDimmerMod::setup() {
#if 0
  pads[0].setCallback(std::bind(&IotsaDimmerMod::touchedOn, this));
  pads[1].setCallback(std::bind(&IotsaDimmerMod::touchedOff, this));
  pads[2].setCallback(std::bind(&IotsaDimmerMod::touchedProgram, this));
#ifdef PIN_VBAT
  batteryMod.setPinVBat(PIN_VBAT, VBAT_100_PERCENT);
#endif
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  configLoad();
  rPrev = r;
  gPrev = g;
  bPrev = b;
  wPrev = w;
  startAnimation();
#endif
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
#if 0
  // Quick return if we have nothing to do
  if (millisStartAnimation == 0) return;
  // Determine how far along the animation we are, and terminate the animation when done (or if it looks preposterous)
  float progress = float(millis()-millisStartAnimation) / float(millisAnimationDuration);
  if (progress < 0 || progress > 1) {
    progress = 1;
    millisStartAnimation = 0;
    rPrev = r;
    gPrev = g;
    bPrev = b;
    wPrev = w;
    IFDEBUG IotsaSerial.printf("IotsaLedstrip: r=%f, g=%f, b=%f, w=%f count=%d darkPixels=%d\n", r, g, b, w, count, darkPixels);
  }
  float curR = r*progress + rPrev*(1-progress);
  float curG = g*progress + gPrev*(1-progress);
  float curB = b*progress + bPrev*(1-progress);
  float curW = w*progress + wPrev*(1-progress);
  int _r, _g, _b, _w;
  _r = curR*256;
  _g = curG*256;
  _b = curB*256;
  _w = curW*256;
  if (_r>255) _r = 255;
  if (_g>255) _g = 255;
  if (_b>255) _b = 255;
  if (_w>255) _w = 255;
#if 0
  IFDEBUG IotsaSerial.printf("IotsaLedstrip::loop: r=%d, g=%d, b=%d, w=%d, count=%d, progress=%f\n", _r, _g, _b, _w, count, progress);
#endif
  if (buffer != NULL && count != 0 && stripHandler != NULL) {
    bool change = false;
    uint8_t *p = buffer;
    for (int i=0; i<count; i++) {
      int wtdR = _r;
      int wtdG = _g;
      int wtdB = _b;
      int wtdW = _w;
      if (darkPixels > 0 && i % (darkPixels+1) != 0) {
        wtdR = wtdG = wtdB = wtdW = 0;
      }
#if 0
      if (bpp == 4) {
        wtdW = wtdR;
        if (wtdG < wtdW) wtdW = wtdG;
        if (wtdB < wtdW) wtdW = wtdB;
        wtdR -= wtdW;
        wtdG -= wtdW;
        wtdB -= wtdW;
      }
#endif
      if (*p != wtdR) {
        *p = wtdR;
        change = true;
      }
      p++;
      if (*p != wtdG) {
        *p = wtdG;
        change = true;
      }
      p++;
      if (*p != wtdB) {
        *p = wtdB;
        change = true;
      }
      p++;
      if (bpp == 4) {
        if (*p != wtdW) {
          *p = wtdW;
          change = true;
        }
        p++;
      }
    }
    if (change) {
      stripHandler->pixelSourceCallback();
    }
  }
#endif
}

bool IotsaDimmerMod::touchedOnOff() {
#if 0
  IFDEBUG IotsaSerial.println("IotsaLedstrip: all on");
  if (buffer) {
    memset(buffer, 255, count*bpp);
    stripHandler->pixelSourceCallback();
  }
#endif
  return true;
}

bool IotsaDimmerMod::changedValue() {
#if 0
  IFDEBUG IotsaSerial.println("IotsaLedstrip: all off");
  if (buffer) {
    memset(buffer, 0, count*bpp);
    stripHandler->pixelSourceCallback();
  }
#endif
  return true;
}

void IotsaDimmerMod::identify() {
#if 0
  if (!buffer) return;
    memset(buffer, 255, count*bpp);
    stripHandler->pixelSourceCallback();
    delay(100);
    memset(buffer, 0, count*bpp);
    stripHandler->pixelSourceCallback();
    delay(100);
    memset(buffer, 255, count*bpp);
    stripHandler->pixelSourceCallback();
    delay(100);
    memset(buffer, 0, count*bpp);
    stripHandler->pixelSourceCallback();
    delay(100);
    startAnimation();
#endif
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

