//
// A Neopixel strip server, which allows control over a strip of neopixels, intended
// to be used as lighting. Color can be set as fraction RGB or HSL, gamma can be changed,
// interval between lit pixels can be changed. Control is through a web UI or
// through REST calls (and/or, depending on Iotsa compile time options, COAP calls).
// The web interface can be disabled by building iotsa with IOTSA_WITHOUT_WEB.
//
// This is the application that is usually shipped with new iotsa boards.
//

#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaLed.h"
#include "iotsaConfigFile.h"

// CHANGE: Add application includes and declarations here

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

IotsaApplication application("Iotsa LED Server");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

//
// LED module. 
//

#define NEOPIXEL_PIN 13  // "Normal" pin for NeoPixel
#define NEOPIXEL_TYPE (NEO_GRB + NEO_KHZ800)

class IotsaLedstripMod : public IotsaApiMod {
public:
  using IotsaApiMod::IotsaApiMod;
  void setup();
  void serverSetup();
  String info();
  void configLoad();
  void configSave();
  void loop();
protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
private:
  void handler();
  void setHSL(float h, float s, float l);
  void getHSL(float &h, float &s, float &l);
  void setTempL(float t, float l);
  Adafruit_NeoPixel *strip;
  float r, g, b;
  int count;
  float gamma;
  int interval;
};

void IotsaLedstripMod::setHSL(float h, float s, float l) {
      // Formulas from https://en.wikipedia.org/wiki/HSL_and_HSV#HSL_to_RGB
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
      r = r1+m;
      g = g1+m;
      b = b1+m;
      if (r < 0) r = 0;
      if (r >= 1) r = 1;
      if (g < 0) g = 0;
      if (g >= 1) g = 1;
      if (b < 0) b = 0;
      if (b >= 1) b = 1;
}

void IotsaLedstripMod::getHSL(float &h, float &s, float &l)
{
  h = 0;
  float r1 = r, g1 = g, b1 = b;
  float minChroma = fmin(fmin(r1, g1), b1);
  float maxChroma = fmax(fmax(r1, g1), b1);
  if (minChroma == maxChroma) {
    h = 0;
  } else if (maxChroma == r1) {
    h = 60 * ( 0 + (g1-b1) / (maxChroma-minChroma));
  } else if (maxChroma == g1) {
    h = 60 * ( 2 + (b1-r1) / (maxChroma-minChroma));
  } else if (maxChroma == b1) {
    h = 60 * ( 4 + (r1-g1) / (maxChroma-minChroma));
  }
  if (h < 0) h += 360.0;
  if (maxChroma == 0 || minChroma == 1) {
    s = 0;
  } else {
    s = (maxChroma-minChroma) / (1 - fabs(maxChroma+minChroma-1));
  }
  l = (maxChroma + minChroma) / 2;
}

void IotsaLedstripMod::setTempL(float t, float l) {
  // Algorithm from http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code
  // as adapted by Renaud Bédard for https://www.shadertoy.com/view/lsSXW1
  if (t < 1000) t = 1000;
  if (t > 40000) t = 40000;
  t = t / 100;
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
  if (r < 0) r = 0;
  if (r > 1) r = 1;
  if (g < 0) g = 0;
  if (g > 1) g = 1;
  if (b < 0) b = 0;
  if (b > 1) b = 1;
  r *= l;
  g *= l;
  b *= l;
}

#ifdef IOTSA_WITH_WEB
void
IotsaLedstripMod::handler() {
  // Handles the page that is specific to the Led module, greets the user and
  // optionally stores a new name to greet the next time.
  String error;
  bool anyChanged = false;
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
    float t = server->arg("temp").toFloat();
    float l = server->arg("l").toFloat();
    setTempL(t, l);
    anyChanged = true;
  } else {
    if( server->hasArg("r")) {
      r = server->arg("r").toFloat();
      anyChanged = true;
    }
    if( server->hasArg("g")) {
      g = server->arg("g").toFloat();
      anyChanged = true;
    }
    if( server->hasArg("b")) {
      b = server->arg("b").toFloat();
      anyChanged = true;
    }
  }
  if( server->hasArg("gamma")) {
    gamma = server->arg("gamma").toFloat();
    anyChanged = true;
  }
  if( server->hasArg("count")) {
    count = server->arg("count").toInt();
    anyChanged = true;
  }
  if( server->hasArg("interval")) {
    interval = server->arg("interval").toInt();
    anyChanged = true;
  }

  if (anyChanged) {
    configSave();
    setup();
  }
  
  String message = "<html><head><title>Ledstrip Server</title></head><body><h1>Ledstrip Server</h1>";
  if (error != "") {
    message += "<p><em>" + error + "</em></p>";
  }
  message += "<form method='get'>";

  String checked = "";
  if (set == "" || set == "rgb") checked = " checked";
  message += "<input type='radio' name='set' value=''" + checked + ">Set RGB value:<br>";
  message += "Red (0..1): <input type='text' name='r' value='" + String(r) +"' ><br>";
  message += "Green (0..1): <input type='text' name='g' value='" + String(g) +"' ><br>";
  message += "Blue (0..1): <input type='text' name='b' value='" + String(b) +"' ><br>";

  checked = "";
  if (set == "hsl") checked = " checked";
  message += "<input type='radio' name='set' value='hsl'" + checked + ">Use HSL value:<br>";
  float h, s, l;
  getHSL(h, s, l);
  message += "Hue (0..360): <input type='text' name='h' value='" + String(h) +"'><br>";
  message += "Saturation (0..1): <input type='text' name='s' value='" + String(s) +"'><br>";
  message += "Lightness (0..1): <input type='text' name='l' value='" + String(l) +"'><br>";

  checked = "";
  if (set == "temp") checked = " checked";
  message += "<input type='radio' name='set' value='temp'" + checked + ">Set Temperature:<br>";
  message += "Temperature (1000..40000): <input type='text' name='temp'><br>";
  message += "(also set Lightness, above)<br>";

  message += "Gamma: <input type='text' name='gamma' value='" + String(gamma) +"' ><br>";

  message += "Number of LEDs: <input type='text' name='count' value='" + String(count) +"' ><br>";
  message += "Dark interval: <input type='text' name='interval' value='" + String(interval) +"' ><br>";
  message += "<input type='submit'></form></body></html>";
  server->send(200, "text/html", message);
}

String IotsaLedstripMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/ledstrip\">/led</a> for setting the color of the LEDs and their count.</p>";
  return rv;
}
#endif // IOTSA_WITH_WEB

bool IotsaLedstripMod::getHandler(const char *path, JsonObject& reply) {
  reply["r"] = r;
  reply["g"] = g;
  reply["b"] = b;
  reply["gamma"] = gamma;
  reply["count"] = count;
  reply["interval"] = interval;
  return true;
}

bool IotsaLedstripMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  // xxxjack HSL to be done
  JsonVariant arg = request["r"]|0;
  r = arg.as<int>();
  arg = request["g"]|0;
  g = arg.as<int>();
  arg = request["b"]|0;
  b = arg.as<int>();
  arg = request["gamma"]|0;
  gamma = arg.as<float>();
  arg = request["count"]|0;
  count = arg.as<int>();
  arg = request["interval"]|0;
  interval = arg.as<int>();
  configSave();
  setup();
  return true;
}

void IotsaLedstripMod::serverSetup() {
  // Setup the web server hooks for this module.
#ifdef IOTSA_WITH_WEB
  server->on("/ledstrip", std::bind(&IotsaLedstripMod::handler, this));
#endif // IOTSA_WITH_WEB
  api.setup("/api/ledstrip", true, true);
  name = "led";
}


void IotsaLedstripMod::configLoad() {
  IotsaConfigFileLoad cf("/config/ledstrip.cfg");
  cf.get("r", r, 1.0);
  cf.get("g", g, 1.0);
  cf.get("b", b, 1.0);
  cf.get("gamma", gamma, 1.0);
  cf.get("count", count, 1);
  cf.get("interval", interval, 0);
}

void IotsaLedstripMod::configSave() {
  IotsaConfigFileSave cf("/config/ledstrip.cfg");
  cf.put("r", r);
  cf.put("g", g);
  cf.put("b", b);
  cf.put("gamma", gamma);
  cf.put("count", count);
  cf.put("interval", interval);
}

void IotsaLedstripMod::setup() {
  configLoad();
  if (strip) delete strip;
  strip = new Adafruit_NeoPixel(count, NEOPIXEL_PIN, NEOPIXEL_TYPE);
//  strip->clear();
//  strip->show();
  int _r = r*256;
  int _g = g*256;
  int _b = b*256;
  if (gamma == 1.0) {
    // gamma correction
    _r = r*256;
    _g = g*256;
    _b = b*256;
  } else {
    _r = powf(r, gamma) * 256;
    _g = powf(g, gamma) * 256;
    _b = powf(b, gamma) * 256;
  }
  if (_r>255) _r = 255;
  if (_g>255) _g = 255;
  if (_b>255) _b = 255;
  IFDEBUG IotsaSerial.print("r=");
  IFDEBUG IotsaSerial.print(_r);
  IFDEBUG IotsaSerial.print(",g=");
  IFDEBUG IotsaSerial.print(_g);
  IFDEBUG IotsaSerial.print(",b=");
  IFDEBUG IotsaSerial.print(_b);
  IFDEBUG IotsaSerial.print(",count=");
  IFDEBUG IotsaSerial.println(count);
  strip->begin();
  int i = 0;
  while (i < count) {
    strip->setPixelColor(i, _r, _g, _b);
    i++;
    for (int j=0; j<interval; j++) {
      strip->setPixelColor(i, 0, 0, 0);
      i++;
    }
  }
  strip->show();
}

void IotsaLedstripMod::loop() {

}

// Instantiate the Led module, and install it in the framework
IotsaLedstripMod ledstripMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}

