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

#define NEOPIXEL_PIN 4  // "Normal" pin for NeoPixel
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
  Adafruit_NeoPixel *strip;
  int r, g, b;
  int intensity;
  int count;
};

#ifdef IOTSA_WITH_WEB
void
IotsaLedstripMod::handler() {
  // Handles the page that is specific to the Led module, greets the user and
  // optionally stores a new name to greet the next time.
  bool anyChanged = false;
  if( server->hasArg("r")) {
    r = server->arg("r").toInt();
    anyChanged = true;
  }
  if( server->hasArg("g")) {
    g = server->arg("g").toInt();
    anyChanged = true;
  }
  if( server->hasArg("b")) {
    b = server->arg("b").toInt();
    anyChanged = true;
  }
  if( server->hasArg("intensity")) {
    intensity = server->arg("intensity").toInt();
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
  message += "<form method='get'>";
  message += "Red (0..255): <input type='text' name='r' value='" + String(r) +"' ><br>";
  message += "Green (0..255): <input type='text' name='g' value='" + String(g) +"' ><br>";
  message += "Blue (0..255): <input type='text' name='b' value='" + String(b) +"' ><br>";
  message += "Intensity (0..100): <input type='text' name='intensity' value='" + String(intensity) +"' ><br>";
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
  reply["intensity"] = intensity;
  reply["count"] = count;
  return true;
}

bool IotsaLedstripMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  JsonVariant arg = request["r"]|0;
  r = arg.as<int>();
  arg = request["g"]|0;
  g = arg.as<int>();
  arg = request["b"]|0;
  b = arg.as<int>();
  arg = request["intensity"]|0;
  intensity = arg.as<int>();
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
  cf.get("intensity", intensity, 10);
  cf.get("count", count, 1);
}

void IotsaLedstripMod::configSave() {
  IotsaConfigFileSave cf("/config/ledstrip.cfg");
  cf.put("r", r);
  cf.put("g", g);
  cf.put("b", b);
  cf.put("intensity", intensity);
  cf.put("count", count);
}

void IotsaLedstripMod::setup() {
  configLoad();
  if (strip) delete strip;
  strip = new Adafruit_NeoPixel(count, NEOPIXEL_PIN, NEOPIXEL_TYPE);
//  strip->clear();
//  strip->show();
  int _r = (r*intensity)/100;
  int _g = (g*intensity)/100;
  int _b = (b*intensity)/100;
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

