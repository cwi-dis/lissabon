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
  bool changedIllum();
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
  static constexpr UUIDstring illumUUID = "F3390004-F793-4D0C-91BB-C91EEB92A1A4";
  static constexpr UUIDstring tempUUID = "F3390005-F793-4D0C-91BB-C91EEB92A1A4";
  static constexpr UUIDstring intervalUUID = "F3390006-F793-4D0C-91BB-C91EEB92A1A4";
#endif // IOTSA_WITH_BLE
private:
  void handler();
  void setHSL(float h, float s, float l);
  bool hasHSL();
  void getHSL(float &h, float &s, float &l);
  void setTI(float temp, float illum);
  bool hasTI();
  void startAnimation(bool quick=false);
  void identify();
  Adafruit_NeoPixel *strip;
  bool isOn;  // Master boolean: if false there is no light.
  RgbwFColor color;
  RgbwFColor prevColor;
#if 0
  float r, g, b, w;  // Wanted RGB(W) color
  float rPrev, gPrev, bPrev, wPrev; // Previous RGB(W) color
#endif
  uint32_t millisStartAnimation;
  int millisAnimationDuration;
  int millisThisAnimationDuration;
  float h, s, l;
  bool hslIsSet;
  float temp, illum;
  float whiteTemperature = 4000;  // Color temperature of the white channel
  bool tiIsSet;
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
  if (bpp == 4 && hasTI()) {
    // If we have RGBW pixels we redo the TI calculation, to cater for white pixels
    setTI(temp, illum);
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
      // Formulas from https://en.wikipedia.org/wiki/HSL_and_HSV#HSL_to_RGB
      h = _h;
      s = _s;
      l = _l;
      hslIsSet = true;
      tiIsSet = false;
      float chroma = (1 - abs(2*l - 1)) * s;
      float hprime = fmod(h, 360.0) / 60;
      float x = chroma * (1-abs(fmod(hprime, 2.0)-1));
      float r1, g1, b1;
      r1 = g1 = b1 = 0;
      if (0 <= hprime && hprime <= 1) {
          r1 = chroma;
          g1 = x;
          b1 = 0;
      } else
      if (1 <= hprime && hprime <= 2) {
          r1 = x;
          g1 = chroma;
          b1 = 0;
      } else
      if (2 <= hprime && hprime <= 3) {
          r1 = 0;
          g1 = chroma;
          b1 = x;
      } else
      if (3 <= hprime && hprime <= 4) {
          r1 = 0;
          g1 = x;
          b1 = chroma;
      } else
      if (4 <= hprime && hprime <= 5) {
          r1 = x;
          g1 = 0;
          b1 = chroma;
      } else
      if (5 <= hprime && hprime <= 6) {
          r1 = chroma;
          g1 = 0;
          b1 = x;
      }
      float m = l - (chroma/2);
      color = RgbFColor(r1+m, g1+m, b1+m);
}

bool IotsaLedstripMod::hasHSL()
{
  return hslIsSet;
}

// Helper routine: convert a temperature to RGB
static void _temp2rgb(float temp, float& r, float& g, float& b) {
  // Algorithm from http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code
  // as adapted by Renaud Bédard for https://www.shadertoy.com/view/lsSXW1
  if (temp < 1000) temp = 1000;
  if (temp > 40000) temp = 40000;
  float t = temp / 100;
  if (t < 66) {
    r = 1;
    g = 0.39008157876901960784 * log(t) - 0.63184144378862745098;
  } else {
    r = 1.29293618606274509804 * pow(t-60, -0.1332047592);
    g = 1.12989086089529411765 * pow(t-60, -0.0755148492);
  }
  if (t >= 66) {
    b = 1;
  } else if (t <= 19) {
    b = 0;
  } else {
    b = 0.54320678911019607843 * log(t - 10.0) - 1.19625408914;
  }
}

void IotsaLedstripMod::setTI(float _temp, float _illum) {
  temp = _temp;
  if (temp < 1000 || temp > 10000) temp = whiteTemperature; // Cater for coming from RGB or HLS value
  illum = _illum;
  tiIsSet = true;
  hslIsSet = false;
  // Convert the temperature to RGB
  float r, g, b;
  _temp2rgb(temp, r, g, b);
  // Multiply with illumination
  r *= illum;
  g *= illum;
  b *= illum;

  // Extract the white
  // NOTE: here we need to cater for the temperature of the white,
  // in case it isn't 6500K (which probably it isn't), which is the white
  // point of D65 RGB.
  float w = 0;
  if (bpp == 4 && whiteTemperature != 0) {
    // Compute RGB value (in D65 space) of our white LED
    float rWhite, gWhite, bWhite;
    _temp2rgb(whiteTemperature, rWhite, gWhite, bWhite);
    // Determine how much we can maximally subtract from all R, G and B channels
    float maxRfactor = r / rWhite;
    float maxGfactor = g / gWhite;
    float maxBfactor = b / bWhite;
    float factor = min(maxRfactor, min(maxGfactor, maxBfactor));
    // This is our white value, and we subtract the corresponding whiteness from R, G and B
    w = factor;
    r -= rWhite*factor;
    g -= gWhite*factor;
    b -= bWhite*factor;
    // Now we multiply everything by 2, because we want illum==1 to be the maximum amount of light possible
    // (even if that means the color will be slightly off)
    r *= 2;
    g *= 2;
    b *= 2;
    w *= 2;
    if (w > 1) {
      // If we have too much white we use the RGB leds to give us as much as the extra white as possible
      // If we go out of bounds the clamping (below) will fix that.
      float excessWhite = w-1;
      w = 1;
      r += rWhite*excessWhite;
      g += gWhite*excessWhite;
      b += bWhite*excessWhite;

    }
  }
  IFDEBUG IotsaSerial.printf("setTI(%f, %f): r=%f g=%f b=%f w=%f\n", temp, illum, r, g, b, w);
  if (r < 0) r = 0;
  if (r > 1) r = 1;
  if (g < 0) g = 0;
  if (g > 1) g = 1;
  if (b < 0) b = 0;
  if (b > 1) b = 1;
  if (w < 0) w = 0;
  if (w > 1) w = 1;
  color = RgbwFColor(r, g, b, w);
}

bool IotsaLedstripMod::hasTI()
{
  return tiIsSet;
}

#ifdef IOTSA_WITH_BLE
bool IotsaLedstripMod::blePutHandler(UUIDstring charUUID) {
  bool anyChanged = false;
  if (charUUID == tempUUID) {
      int _temp = bleApi.getAsInt(tempUUID);
      setTI(_temp, illum);
      IFDEBUG IotsaSerial.printf("xxxjack ble: wrote temp %s value %d %f\n", tempUUID, _temp, temp);
      anyChanged = true;
  }
  if (charUUID == illumUUID) {
      int _illum = bleApi.getAsInt(illumUUID);
      setTI(temp, float(_illum)/100.0);
      IFDEBUG IotsaSerial.printf("xxxjack ble: wrote illum %s value %d %f\n", illumUUID, _illum, illum);
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
      int _temp = int(temp);
      IFDEBUG IotsaSerial.printf("xxxjack ble: read temp %s value %d\n", charUUID, _temp);
      bleApi.set(tempUUID, (uint16_t)_temp);
      return true;
  }
  if (charUUID == illumUUID) {
      int _illum = int(illum*100);
      IFDEBUG IotsaSerial.printf("xxxjack ble: read illum %s value %d\n", charUUID, _illum);
      bleApi.set(illumUUID, (uint8_t)_illum);
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
  if( server->hasArg("white")) {
    whiteTemperature = server->arg("white").toFloat();
    anyChanged = true;
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
    float illum = server->arg("illuminance").toFloat();
    setTI(temp, illum);
    anyChanged = true;
  } else {
    float r=color.R;
    float g=color.G;
    float b=color.B;
    float w=color.W;
    if( server->hasArg("r")) {
      hslIsSet = false;
      tiIsSet = false;
      r = server->arg("r").toFloat();
      anyChanged = true;
    }
    if( server->hasArg("g")) {
      hslIsSet = false;
      tiIsSet = false;
      g = server->arg("g").toFloat();
      anyChanged = true;
    }
    if( server->hasArg("b")) {
      hslIsSet = false;
      tiIsSet = false;
      b = server->arg("b").toFloat();
      anyChanged = true;
    }
    if( bpp == 4 && server->hasArg("w")) {
      hslIsSet = false;
      tiIsSet = false;
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
  if (!hasHSL() && !hasTI()) checked = " checked";
  message += "<input type='radio' name='set' value=''" + checked + ">Set RGB value:<br>";
  message += "Red (0..1): <input type='text' name='r' value='" + String(color.R) +"' ><br>";
  message += "Green (0..1): <input type='text' name='g' value='" + String(color.G) +"' ><br>";
  message += "Blue (0..1): <input type='text' name='b' value='" + String(color.B) +"' ><br>";
  if (bpp == 4) 
    message += "White (0..1): <input type='text' name='w' value='" + String(color.W) +"' ><br>";

  checked = "";
  if (hasHSL()) {
    checked = " checked";
  } else {
    h = 0;
    s = 0;
    l = 0;
  }
  message += "<input type='radio' name='set' value='hsl'" + checked + ">Use HSL value:<br>";
  message += "Hue (0..360): <input type='text' name='h' value='" + String(h) +"'><br>";
  message += "Saturation (0..1): <input type='text' name='s' value='" + String(s) +"'><br>";
  message += "Lightness (0..1): <input type='text' name='l' value='" + String(l) +"'><br>";

  checked = "";
  if (hasTI()) {
    checked = " checked";
  } else {
    temp = 0;
    illum = 0;
  }
  message += "<input type='radio' name='set' value='temp'" + checked + ">Set Temperature:<br>";
  message += "Temperature (1000..40000): <input type='text' name='temperature' value='" + String(temp) +"'><br>";
  message += "Illuminance (0..1): <input type='text' name='illuminance' value='" + String(illum) +"'><br>";
  message += "<br>";
  message += "Dark pixels between lit pixels: <input type='text' name='darkPixels' value='" + String(darkPixels) +"' ><br>";
  message += "Animation duration (ms): <input type='text' name='animation' value='" + String(millisAnimationDuration) +"' ><br>";
  if (bpp == 4) {
    message += "White LED temperature: <input type='text' name='white' value='" + String(whiteTemperature) +"' ><br>";
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
  reply["r"] = color.R;
  reply["g"] = color.G;
  reply["b"] = color.B;
  if (bpp == 4) reply["w"] = color.W;
  if (hasHSL()) {
    reply["h"] = h;
    reply["s"] = s;
    reply["l"] = l;
  }
  if (hasTI()) {
    reply["temperature"] = temp;
    reply["illuminance"] = illum;
  }
  reply["darkPixels"] = darkPixels;
  reply["isOn"] = isOn;
  reply["animation"] = millisAnimationDuration;
  if (bpp == 4) reply["white"] = whiteTemperature;
  return true;
}

bool IotsaLedstripMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (request["identify"]|0) identify();
  darkPixels = request["darkPixels"]|darkPixels;
  millisAnimationDuration = request["animation"]|millisAnimationDuration;
  whiteTemperature = request["white"]|whiteTemperature;
  hslIsSet = request.containsKey("h")||request.containsKey("s")||request.containsKey("l");
  tiIsSet = request.containsKey("temperature")||request.containsKey("illuminance");
  if (hslIsSet) {
    isOn = true;
    h = request["h"]|0;
    s = request["s"]|0;
    l = request["l"]|0;
    setHSL(h, s, l);
  } else
  if (tiIsSet) {
    isOn = true;
    temp = request["temp"]|0;
    illum = request["illum"]|0;
    setTI(temp, illum);
  } else {
    // By default look at RGB values (using current values as default)
    isOn = true;
    float r = color.R;
    float g = color.G;
    float b = color.B;
    float w = color.W;
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
  cf.get("white", whiteTemperature, 4000);
  float r, g, b, w;
  cf.get("r", r, 0.0);
  cf.get("g", g, 0.0);
  cf.get("b", b, 0.0);
  cf.get("w", w, 0.0);
  color = RgbwFColor(r, g, b, w);

  cf.get("h", h, 0.0);
  cf.get("s", s, 0.0);
  cf.get("l", l, 0.0);
  if (h > 0 || s > 0 || l > 0) setHSL(h, s, l);

  cf.get("temp", temp, 0.0);
  cf.get("illum", illum, 0.0);
  if (temp > 0 || illum > 0) setTI(temp, illum);
}

void IotsaLedstripMod::configSave() {
  IotsaConfigFileSave cf("/config/ledstrip.cfg");
  cf.put("isOn", isOn);
  cf.put("darkPixels", darkPixels);
  cf.put("animation", millisAnimationDuration);
  cf.put("white", whiteTemperature);

  cf.put("r", color.R);
  cf.put("g", color.G);
  cf.put("b", color.B);
  cf.put("w", color.W);
  
  if (hasHSL()) {
    IotsaSerial.println("xxxjack saving hsl");
    cf.put("h", h);
    cf.put("s", s);
    cf.put("l", l);
  }
  if (hasTI()) {
    IotsaSerial.println("xxxjack saving ti");
    cf.put("temp", temp);
    cf.put("illum", illum);
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
  levelDimmer.bindVar(illum, 0.0, 1.0, 0.01);
  levelDimmer.setCallback(std::bind(&IotsaLedstripMod::changedIllum, this));
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

  static BLE2904 illum2904;
  illum2904.setFormat(BLE2904::FORMAT_UINT8);
  illum2904.setUnit(0x27AD);
  static BLE2901 illum2901("Illumination");
  bleApi.addCharacteristic(illumUUID, BLE_READ|BLE_WRITE, &illum2904, &illum2901);

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
    IFDEBUG IotsaSerial.printf("IotsaLedstrip: isOn=%d r=%f, g=%f, b=%f, w=%f count=%d darkPixels=%d\n", isOn, color.R, color.G, color.B, color.W, count, darkPixels);
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

bool IotsaLedstripMod::changedIllum() {
  isOn = true;
  setTI(temp, illum);
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

