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

IotsaApplication application("Iotsa LEDstrip Lighting Server");
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

#include "iotsaPixelStrip.h"
IotsaPixelstripMod pixelstripMod(application);

#include "NPBColorLib.h"

// Define this to enable support for touchpads to control the led strip (otherwise only BLE/REST/WEB control)
#define WITH_TOUCHPADS

#ifdef WITH_TOUCHPADS
#include "iotsaInput.h"
Touchpad upTouch(12, true, true, true);
Touchpad downTouch(13, true, true, true);
UpDownButtons levelDimmer(upTouch, downTouch, true);
//Touchpad(15, true, false, true);

Input* inputs[] = {
  &levelDimmer
};

IotsaInputMod inputMod(application, inputs, sizeof(inputs)/sizeof(inputs[0]));
#endif // WITH_TOUCHPADS

//
// LED Lighting module. 
//

class IotsaLedstripMod : public IotsaApiMod, public IotsaPixelsource, public IotsaBLEApiProvider {
public:
  using IotsaApiMod::IotsaApiMod;
  void setup();
  void serverSetup();
  String info();
  void configLoad();
  void configSave();
  void loop();
  void setHandler(uint8_t *_buffer, size_t _count, int bpp, IotsaPixelsourceHandler *handler);

protected:
  bool changedBrightness();
  bool changedOnOff();
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
#ifdef IOTSA_WITH_BLE
  IotsaBleApiService bleApi;
  bool blePutHandler(UUIDstring charUUID);
  bool bleGetHandler(UUIDstring charUUID);
  static constexpr UUIDstring serviceUUID = "F3390001-F793-4D0C-91BB-C91EEB92A1A4";
  static constexpr UUIDstring isOnUUID = "F3390002-F793-4D0C-91BB-C91EEB92A1A4";
  static constexpr UUIDstring identifyUUID = "F3390003-F793-4D0C-91BB-C91EEB92A1A4";
  static constexpr UUIDstring brightnessUUID = "F3390004-F793-4D0C-91BB-C91EEB92A1A4";
  static constexpr UUIDstring tempUUID = "F3390005-F793-4D0C-91BB-C91EEB92A1A4";
  static constexpr UUIDstring intervalUUID = "F3390006-F793-4D0C-91BB-C91EEB92A1A4";
#endif // IOTSA_WITH_BLE
private:
  void handler();
  void setHSL(float h, float s, float l);
  void setTI(float temp, float brightness);
  void startAnimation(bool quick=false);
  void identify();
  Adafruit_NeoPixel *strip;
  bool isOn;  // Master boolean: if false there is no light.

  RgbwColor color;
  RgbwColor prevColor;

  Colorspace rgbwSpace;

  HslFColor hslColor;
  bool hslColorIsValid;
  TempFColor tempColor;
  bool tempColorIsValid;

  uint32_t millisStartAnimation;
  int millisAnimationDuration;
  int millisThisAnimationDuration;
//  float whiteTemperature = 4000;  // Color temperature of the white channel
  uint8_t *buffer;
  int count;  // Number of LEDs
  int bpp; // Number of colors per LED (3 or 4)
  IotsaPixelsourceHandler *stripHandler;
  int darkPixels; // Number of unlit pixels between lit pixels
  uint32_t saveAtMillis;
};

void IotsaLedstripMod::setHandler(uint8_t *_buffer, size_t _count, int _bpp, IotsaPixelsourceHandler *_handler) {
  buffer = _buffer;
  count = _count;
  bpp = _bpp;
  stripHandler = _handler;
  if (bpp == 4 && tempColorIsValid) {
    // If we have RGBW pixels we redo the TI calculation, to cater for white pixels
    setTI(tempColor.Temperature, tempColor.Brightness);
    prevColor = color;
  }
  startAnimation();
}

void IotsaLedstripMod::startAnimation(bool quick) {
  millisStartAnimation = millis();
  millisThisAnimationDuration = quick ? 1 : millisAnimationDuration;
  iotsaConfig.postponeSleep(millisThisAnimationDuration+100);
}

void IotsaLedstripMod::setHSL(float _h, float _s, float _l) {
  hslColor = HslFColor(_h, _s, _l);
  hslColorIsValid = true;
  tempColorIsValid = false;
  color = rgbwSpace.toRgbw(hslColor);
}

void IotsaLedstripMod::setTI(float _temp, float _brightness) {
  if (_temp < 1000 || _temp > 10000) _temp = rgbwSpace.WTemperature; // Cater for coming from RGB or HLS value
  tempColor = TempFColor(_temp, _brightness);
  tempColorIsValid = true;
  hslColorIsValid = false;
  color = rgbwSpace.toRgbw(tempColor);

  IFDEBUG IotsaSerial.printf("setTI(%f, %f): r=%d g=%d b=%d w=%d\n", _temp, _brightness, color.R, color.G, color.B, color.W);
}

#ifdef IOTSA_WITH_BLE
bool IotsaLedstripMod::blePutHandler(UUIDstring charUUID) {
  bool anyChanged = false;
  if (charUUID == tempUUID) {
      int _temp = bleApi.getAsInt(tempUUID);
      setTI(_temp, tempColor.Brightness);
      IFDEBUG IotsaSerial.printf("xxxjack ble: wrote temp %s value %d %f\n", tempUUID, _temp, tempColor.Brightness);
      anyChanged = true;
  }
  if (charUUID == brightnessUUID) {
      int _brightness = bleApi.getAsInt(brightnessUUID);
      setTI(tempColor.Temperature, float(_brightness)/100.0);
      IFDEBUG IotsaSerial.printf("xxxjack ble: wrote brightness %s value %d %f\n", brightnessUUID, _brightness, tempColor.Temperature);
      anyChanged = true;
  }
  if (charUUID == intervalUUID) {
      darkPixels = bleApi.getAsInt(intervalUUID);
      IFDEBUG IotsaSerial.printf("xxxjack ble: wrote darkPixels %s value %d\n", intervalUUID, darkPixels);
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
  IotsaSerial.println("iotsaLedstripMod: ble: write unknown uuid");
  return false;
}

bool IotsaLedstripMod::bleGetHandler(UUIDstring charUUID) {
  if (charUUID == tempUUID) {
      int _temp = int(tempColor.Temperature);
      IFDEBUG IotsaSerial.printf("xxxjack ble: read temp %s value %d\n", charUUID, _temp);
      bleApi.set(tempUUID, (uint16_t)_temp);
      return true;
  }
  if (charUUID == brightnessUUID) {
      int _brightness = int(tempColor.Brightness*100);
      IFDEBUG IotsaSerial.printf("xxxjack ble: read brightness %s value %d\n", charUUID, _brightness);
      bleApi.set(brightnessUUID, (uint8_t)_brightness);
      return true;
  }
  if (charUUID == intervalUUID) {
      IFDEBUG IotsaSerial.printf("xxxjack ble: read darkPixels %s value %d\n", charUUID, darkPixels);
      bleApi.set(intervalUUID, (uint8_t)darkPixels);
      return true;
  }
  if (charUUID == isOnUUID) {
      IFDEBUG IotsaSerial.printf("xxxjack ble: read isOn %s value %d\n", charUUID, isOn);
      bleApi.set(isOnUUID, (uint8_t)isOn);
      return true;
  }
  IotsaSerial.println("iotsaLedstripMod: ble: read unknown uuid");
  return false;
}
#endif // IOTSA_WITH_BLE

#ifdef IOTSA_WITH_WEB
void
IotsaLedstripMod::handler() {
  // Handles the page that is specific to the Led module, greets the user and
  // optionally stores a new name to greet the next time.
  String error;
  bool anyChanged = false;
  if (server->hasArg("identify")) identify();
  if( server->hasArg("animation")) {
    millisAnimationDuration = server->arg("animation").toInt();
    anyChanged = true;
  }
  if( server->hasArg("darkPixels")) {
    darkPixels = server->arg("darkPixels").toInt();
    anyChanged = true;
  }
  bool whiteChanged = false;
  float whiteTemperature = rgbwSpace.WTemperature;
  float whiteBrightness = rgbwSpace.WBrightness;
  if( server->hasArg("whiteTemperature")) {
    whiteTemperature = server->arg("whiteTemperature").toFloat();
    whiteChanged = true;
    anyChanged = true;
  }
  if( server->hasArg("whiteBrightness")) {
    whiteBrightness = server->arg("whiteBrightness").toFloat();
    whiteChanged = true;
    anyChanged = true;
  }
  if (whiteChanged) {
    rgbwSpace = Colorspace(whiteTemperature, whiteBrightness, false, false);
    if (hslColorIsValid) setHSL(hslColor.H, hslColor.S, hslColor.L);
    if (tempColorIsValid) setTI(tempColor.Temperature, tempColor.Brightness);
  }
  String set = "rgb";
  if (server->hasArg("set")) set = server->arg("set");
  if (set == "hsl") {
    // HLS color requested
    if (!server->hasArg("h") || !server->hasArg("s") || !server->hasArg("l")) {
      error = "All three of H, S, L must be specified";
    } else {
      float h = server->arg("h").toFloat();
      float s = server->arg("s").toFloat();
      float l = server->arg("l").toFloat();
      setHSL(h, s, l);
      anyChanged = true;
    }
  } else if (set == "temp") {
    float temp = server->arg("temperature").toFloat();
    float brightness = server->arg("brightness").toFloat();
    setTI(temp, brightness);
    anyChanged = true;
  } else {
    float r=color.R/255.0;
    float g=color.G/255.0;
    float b=color.B/255.0;
    float w=color.W/255.0;
    if( server->hasArg("r")) {
      hslColorIsValid = false;
      tempColorIsValid = false;
      r = server->arg("r").toFloat();
      anyChanged = true;
    }
    if( server->hasArg("g")) {
      hslColorIsValid = false;
      tempColorIsValid = false;
      g = server->arg("g").toFloat();
      anyChanged = true;
    }
    if( server->hasArg("b")) {
      hslColorIsValid = false;
      tempColorIsValid = false;
      b = server->arg("b").toFloat();
      anyChanged = true;
    }
    if( bpp == 4 && server->hasArg("w")) {
      hslColorIsValid = false;
      tempColorIsValid = false;
      w = server->arg("w").toFloat();
      anyChanged = true;
    }
    color = RgbwColor(r, g, b, w);
  }
  isOn = true;
  if( server->hasArg("isOn")) {
    isOn = server->arg("isOn").toInt();
    anyChanged = true;
  }


  if (anyChanged) {
    configSave();
    startAnimation();
  }
  
  String message = "<html><head><title>Ledstrip Server</title></head><body><h1>Ledstrip Server</h1>";
  if (error != "") {
    message += "<p><em>" + error + "</em></p>";
  }
  message += "<form method='get'><input type='submit' name='identify' value='identify'></form>";
  message += "<form method='get'>";
  message += "<form method='get'><input type='hidden' name='isOn' value='1'><input type='submit' value='Turn On'></form>";
  message += "<form method='get'>";
  message += "<form method='get'><input type='hidden' name='isOn' value='0'><input type='submit' value='Turn Off'></form>";
  message += "<form method='get'>";

  String checked = "";
  if (!hslColorIsValid && !tempColorIsValid) checked = " checked";
  message += "<input type='radio' name='set' value=''" + checked + ">Set RGB value:<br>";
  message += "Red (0..1): <input type='text' name='r' value='" + String(color.R/255.0) +"' ><br>";
  message += "Green (0..1): <input type='text' name='g' value='" + String(color.G/255.0) +"' ><br>";
  message += "Blue (0..1): <input type='text' name='b' value='" + String(color.B/255.0) +"' ><br>";
  if (bpp == 4) 
    message += "White (0..1): <input type='text' name='w' value='" + String(color.W/255.0) +"' ><br>";

  checked = "";
  if (hslColorIsValid) {
    checked = " checked";
  }
  message += "<input type='radio' name='set' value='hsl'" + checked + ">Use HSL value:<br>";
  message += "Hue (0..360): <input type='text' name='h' value='" + String(hslColor.H) +"'><br>";
  message += "Saturation (0..1): <input type='text' name='s' value='" + String(hslColor.S) +"'><br>";
  message += "Lightness (0..1): <input type='text' name='l' value='" + String(hslColor.L) +"'><br>";

  checked = "";
  if (tempColorIsValid) {
    checked = " checked";
  }
  message += "<input type='radio' name='set' value='temp'" + checked + ">Set Temperature:<br>";
  message += "Temperature (1000..40000): <input type='text' name='temperature' value='" + String(tempColor.Temperature) +"'><br>";
  message += "Brightness (0..1): <input type='text' name='brightness' value='" + String(tempColor.Brightness) +"'><br>";
  message += "<br>";
  message += "Dark pixels between lit pixels: <input type='text' name='darkPixels' value='" + String(darkPixels) +"' ><br>";
  message += "Animation duration (ms): <input type='text' name='animation' value='" + String(millisAnimationDuration) +"' ><br>";
  if (bpp == 4) {
    message += "White LED temperature: <input type='text' name='whiteTemperature' value='" + String(whiteTemperature) +"' ><br>";
    message += "White LED brightness: <input type='text' name='whiteBrightness' value='" + String(whiteBrightness) +"' ><br>";
  }
  message += "<input type='submit'></form></body></html>";
  server->send(200, "text/html", message);
}

String IotsaLedstripMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/ledstrip\">/led</a> for setting the color of the LEDs and their count.";
#ifdef IOTSA_WITH_REST
  rv += " Or use REST api at <a href='/api/ledstrip'>/api/ledstrip</a>.";
#endif
#ifdef IOTSA_WITH_BLE
  rv += " Or use BLE service " + String(serviceUUID) + ".";
#endif
  rv += "</p>";
  return rv;
}
#endif // IOTSA_WITH_WEB

bool IotsaLedstripMod::getHandler(const char *path, JsonObject& reply) {
  reply["r"] = color.R/255.0;
  reply["g"] = color.G/255.0;
  reply["b"] = color.B/255.0;
  if (bpp == 4) reply["w"] = color.W/255.0;
  if (hslColorIsValid) {
    reply["h"] = hslColor.H;
    reply["s"] = hslColor.S;
    reply["l"] = hslColor.L;
  }
  if (tempColorIsValid) {
    reply["temperature"] = tempColor.Temperature;
    reply["brightness"] = tempColor.Brightness;
  }
  reply["darkPixels"] = darkPixels;
  reply["isOn"] = isOn;
  reply["animation"] = millisAnimationDuration;
  if (bpp == 4) {
    reply["whiteTemperature"] = rgbwSpace.WTemperature;
    reply["whiteBrightness"] = rgbwSpace.WBrightness;
  }
  return true;
}

bool IotsaLedstripMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (request["identify"]|0) identify();
  darkPixels = request["darkPixels"]|darkPixels;
  millisAnimationDuration = request["animation"]|millisAnimationDuration;
  float whiteTemperature = request["whiteTemperature"]|rgbwSpace.WTemperature;
  float whiteBrightness = request["whiteBrightness"]|rgbwSpace.WBrightness;
  rgbwSpace = Colorspace(whiteTemperature, whiteBrightness, false, false);
  hslColorIsValid = request.containsKey("h")||request.containsKey("s")||request.containsKey("l");
  tempColorIsValid = request.containsKey("temperature")||request.containsKey("brightness");
  if (hslColorIsValid) {
    isOn = true;
    float h = request["h"]|0;
    float s = request["s"]|0;
    float l = request["l"]|0;
    setHSL(h, s, l);
  } else
  if (tempColorIsValid) {
    isOn = true;
    float temp = request["temp"]|0;
    float brightness = request["brightness"]|0;
    setTI(temp, brightness);
  } else {
    // By default look at RGB values (using current values as default)
    isOn = true;
    float r = color.R/255.0;
    float g = color.G/255.0;
    float b = color.B/255.0;
    float w = color.W/255.0;
    r = request["r"]|r;
    g = request["g"]|g;
    b = request["b"]|b;
    if (bpp == 4) {
      w = request["w"]|w;
    }
    color = RgbwColor(r, g, b, w);
  }
  if (request.containsKey("isOn")) {
    isOn = request["isOn"];
  }

  configSave();
  startAnimation();
  return true;
}

void IotsaLedstripMod::serverSetup() {
  // Setup the web server hooks for this module.
#ifdef IOTSA_WITH_WEB
  server->on("/ledstrip", std::bind(&IotsaLedstripMod::handler, this));
#endif // IOTSA_WITH_WEB
  api.setup("/api/ledstrip", true, true);
  name = "ledstrip";
}


void IotsaLedstripMod::configLoad() {
  IotsaConfigFileLoad cf("/config/ledstrip.cfg");
  int value;
  cf.get("isOn", value, 1);
  isOn = value;
  cf.get("darkPixels", darkPixels, 0);
  cf.get("animation", millisAnimationDuration, 500);
  float whiteTemperature, whiteBrightness;
  cf.get("whiteTemperature", whiteTemperature, 4000);
  cf.get("whiteBrightness", whiteBrightness, 1.0);
  rgbwSpace = Colorspace(whiteTemperature, whiteBrightness, false, false);
  float r, g, b, w;
  cf.get("r", r, 0.0);
  cf.get("g", g, 0.0);
  cf.get("b", b, 0.0);
  cf.get("w", w, 0.0);
  color = RgbwFColor(r, g, b, w);

  float h, s, l;
  cf.get("h", h, 0.0);
  cf.get("s", s, 0.0);
  cf.get("l", l, 0.0);
  if (h > 0 || s > 0 || l > 0) setHSL(h, s, l);

  float temp, brightness;
  cf.get("temp", temp, 0.0);
  cf.get("brightness", brightness, 0.0);
  if (temp > 0 || brightness > 0) setTI(temp, brightness);
}

void IotsaLedstripMod::configSave() {
  IotsaConfigFileSave cf("/config/ledstrip.cfg");
  cf.put("isOn", isOn);
  cf.put("darkPixels", darkPixels);
  cf.put("animation", millisAnimationDuration);
  cf.put("whiteTemperature", rgbwSpace.WTemperature);
  cf.put("whiteBrightness", rgbwSpace.WBrightness);

  cf.put("r", color.R/255.0f);
  cf.put("g", color.G/255.0f);
  cf.put("b", color.B/255.0f);
  cf.put("w", color.W/255.0f);
  
  if (hslColorIsValid) {
    IotsaSerial.println("xxxjack saving hsl");
    cf.put("h", hslColor.H);
    cf.put("s", hslColor.S);
    cf.put("l", hslColor.L);
  }
  if (tempColorIsValid) {
    IotsaSerial.println("xxxjack saving ti");
    cf.put("temp", tempColor.Temperature);
    cf.put("brightness", tempColor.Brightness);
  }
}

void IotsaLedstripMod::setup() {
#ifdef PIN_VBAT
  batteryMod.setPinVBat(PIN_VBAT, VBAT_100_PERCENT);
#endif
#ifdef PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(PIN_DISABLESLEEP);
#endif
  configLoad();
  prevColor = color;
  startAnimation();
#ifdef WITH_TOUCHPADS
  levelDimmer.bindVar(tempColor.Brightness, 0.0, 1.0, 0.01);
  levelDimmer.setCallback(std::bind(&IotsaLedstripMod::changedBrightness, this));
  levelDimmer.bindStateVar(isOn);
  levelDimmer.setStateCallback(std::bind(&IotsaLedstripMod::changedOnOff, this));
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

  static BLE2904 temp2904;
  temp2904.setFormat(BLE2904::FORMAT_UINT16);
  temp2904.setUnit(0x2700);
  static BLE2901 temp2901("Color Temperature");
  bleApi.addCharacteristic(tempUUID, BLE_READ|BLE_WRITE, &temp2904, &temp2901);

  static BLE2904 brightness2904;
  brightness2904.setFormat(BLE2904::FORMAT_UINT8);
  brightness2904.setUnit(0x27AD);
  static BLE2901 brightness2901("Brightness");
  bleApi.addCharacteristic(brightnessUUID, BLE_READ|BLE_WRITE, &brightness2904, &brightness2901);

  static BLE2904 interval2904;
  interval2904.setFormat(BLE2904::FORMAT_UINT8);
  interval2904.setUnit(0x2700);
  static BLE2901 interval2901("Interval");
  bleApi.addCharacteristic(intervalUUID, BLE_READ|BLE_WRITE, &interval2904, &interval2901);

  static BLE2904 identify2904;
  identify2904.setFormat(BLE2904::FORMAT_UINT8);
  identify2904.setUnit(0x2700);
  static BLE2901 identify2901("Identify");
  bleApi.addCharacteristic(identifyUUID, BLE_WRITE, &identify2904, &identify2901);
#endif
}

void IotsaLedstripMod::loop() {
  // See if we have a value to save (because the user has been turning the dimmer)
  if (saveAtMillis > 0 && millis() > saveAtMillis) {
    saveAtMillis = 0;
    configSave();
  }
  // Quick return if we have nothing to do
  if (millisStartAnimation == 0) return;
  // Determine how far along the animation we are, and terminate the animation when done (or if it looks preposterous)
  float progress = float(millis()-millisStartAnimation) / float(millisThisAnimationDuration);
  RgbwFColor wanted = RgbwFColor(0);
  if (isOn) wanted = color;

  if (progress < 0 || progress > 1) {
    progress = 1;
    millisStartAnimation = 0;
    prevColor = wanted;
    IFDEBUG IotsaSerial.printf("IotsaLedstrip: isOn=%d r=%d, g=%d, b=%d, w=%d count=%d darkPixels=%d\n", isOn, color.R, color.G, color.B, color.W, count, darkPixels);
  }
  RgbwFColor cur = RgbwFColor::LinearBlend(wanted, prevColor, progress);
  RgbwColor pixelColor(cur);

#if 0
  IFDEBUG IotsaSerial.printf("IotsaLedstrip::loop: r=%d, g=%d, b=%d, w=%d, count=%d, progress=%f\n", _r, _g, _b, _w, count, progress);
#endif
  if (buffer != NULL && count != 0 && stripHandler != NULL) {
    bool change = false;
    uint8_t *p = buffer;
    for (int i=0; i<count; i++) {
      RgbwColor thisPixelColor = pixelColor;
      if (darkPixels > 0 && i % (darkPixels+1) != 0) {
        thisPixelColor = RgbwColor(0);
      }
      if (*p != thisPixelColor.R) {
        *p = thisPixelColor.R;
        change = true;
      }
      p++;
      if (*p != thisPixelColor.G) {
        *p = thisPixelColor.G;
        change = true;
      }
      p++;
      if (*p != thisPixelColor.B) {
        *p = thisPixelColor.B;
        change = true;
      }
      p++;
      if (bpp == 4) {
        if (*p != thisPixelColor.W) {
          *p = thisPixelColor.W;
          change = true;
        }
        p++;
      }
    }
    if (change) {
      stripHandler->pixelSourceCallback();
    }
  }
}

bool IotsaLedstripMod::changedBrightness() {
  isOn = true;
  setTI(tempColor.Temperature, tempColor.Brightness);
  startAnimation(true);
  // And prepare for saving (because we don't want to wear out the Flash chip)
  iotsaConfig.postponeSleep(2000);
  saveAtMillis = millis() + 2000;
  return true;
}

bool IotsaLedstripMod::changedOnOff() {
  // Start the animation to get to the wanted value
  IotsaSerial.printf("xxxjack changedOnOff %d\n", isOn);
  startAnimation();
  // And prepare for saving (because we don't want to wear out the Flash chip)
  iotsaConfig.postponeSleep(2000);
  saveAtMillis = millis() + 2000;
  return true;
}

void IotsaLedstripMod::identify() {
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
}
// Instantiate the Led module, and install it in the framework
IotsaLedstripMod ledstripMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  pixelstripMod.setPixelsource(&ledstripMod);
  application.setup();
  application.serverSetup();
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}

