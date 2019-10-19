//
// A "Led" server, which allows control over a single NeoPixel (color,
// duration, on/off pattern). The led can be controlled through a web UI or
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
  void setRGBfromHSL(float h, float s, float l);
  Adafruit_NeoPixel *strip;
  int r, g, b;
  int count;
  float gamma;
  int interval;
};

void IotsaLedstripMod::setRGBfromHSL(float h, float s, float l) {
      // Formulas from https://en.wikipedia.org/wiki/HSL_and_HSV#HSL_to_RGB
      float chroma = (1 - abs(2*l - 1)) * s;
      float hprime = h / 60;
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
      r = int(256 * (r1+m));
      g = int(256 * (g1+m));
      b = int(256 * (b1+m));
}

#ifdef IOTSA_WITH_WEB
void
IotsaLedstripMod::handler() {
  // Handles the page that is specific to the Led module, greets the user and
  // optionally stores a new name to greet the next time.
  String error;
  bool anyChanged = false;
  if (server->hasArg("hsl") && server->arg("hsl").toInt() != 0) {
    // HLS color requested
    if (!server->hasArg("h") || !server->hasArg("s") || !server->hasArg("l")) {
      error = "All three of H, S, L must be specified";
    } else {
      float h = server->arg("h").toFloat();
      float s = server->arg("s").toFloat();
      float l = server->arg("l").toFloat();
      setRGBfromHSL(h, s, l);
      anyChanged = true;
    }
  } else {
    if( server->hasArg("r")) {
      r = int(server->arg("r").toFloat());
      anyChanged = true;
    }
    if( server->hasArg("g")) {
      g = int(server->arg("g").toFloat());
      anyChanged = true;
    }
    if( server->hasArg("b")) {
      b = int(server->arg("b").toFloat());
      anyChanged = true;
    }
  }
  if( server->hasArg("gamma")) {
    gamma = int(server->arg("gamma").toFloat());
    anyChanged = true;
  }
  if( server->hasArg("count")) {
    count = server->arg("count").toInt();
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
  message += "Red (0..255): <input type='text' name='r' value='" + String(r) +"' ><br>";
  message += "Green (0..255): <input type='text' name='g' value='" + String(g) +"' ><br>";
  message += "Blue (0..255): <input type='text' name='b' value='" + String(b) +"' ><br>";
  message += "<input type='checkbox' name='hsl' value='1'>Use HSL in stead of RGB:<br>";
  float h=0, s, l;
  float r1 = r/256.0, g1 = g/256.0, b1 = b/256.0;
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
    s = (maxChroma-minChroma) / (1 + fabs(maxChroma+minChroma-1));
  }
  l = (maxChroma + minChroma) / 2;
  message += "Hue (0..360): <input type='text' name='h' value='" + String(h) +"'><br>";
  message += "Saturation (0..1.0): <input type='text' name='s' value='" + String(s) +"'><br>";
  message += "Lightness (0..1.0): <input type='text' name='l' value='" + String(l) +"'><br>";
  message += "Number of LEDs: <input type='text' name='count' value='" + String(count) +"' ><br>";
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
  reply["count"] = count;
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
  arg = request["count"]|0;
  count = arg.as<int>();
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
  cf.get("r", r, 255);
  cf.get("g", g, 255);
  cf.get("b", b, 255);
  cf.get("count", count, 1);
}

void IotsaLedstripMod::configSave() {
  IotsaConfigFileSave cf("/config/ledstrip.cfg");
  cf.put("r", r);
  cf.put("g", g);
  cf.put("b", b);
  cf.put("count", count);
}

void IotsaLedstripMod::setup() {
  configLoad();
  if (strip) delete strip;
  strip = new Adafruit_NeoPixel(count, NEOPIXEL_PIN, NEOPIXEL_TYPE);
//  strip->clear();
//  strip->show();
  int _r = r;
  int _g = g;
  int _b = b;
  IFDEBUG IotsaSerial.print("r=");
  IFDEBUG IotsaSerial.print(_r);
  IFDEBUG IotsaSerial.print(",g=");
  IFDEBUG IotsaSerial.print(_g);
  IFDEBUG IotsaSerial.print(",b=");
  IFDEBUG IotsaSerial.print(_b);
  IFDEBUG IotsaSerial.print(",count=");
  IFDEBUG IotsaSerial.println(count);
  strip->begin();
  for(int i=0; i<count; i++) {
    strip->setPixelColor(i, _r, _g, _b);
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

