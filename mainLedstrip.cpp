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
#include "iotsaPixelStrip.h"

// CHANGE: Add application includes and declarations here

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

IotsaApplication application("Iotsa LED Server");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

IotsaPixelstripMod pixelstripMod(application);
//
// LED Lighting module. 
//
#define NSTEP 100

class IotsaLedstripMod : public IotsaApiMod, public IotsaPixelsource {
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
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
private:
  void handler();
  void setHSL(float h, float s, float l);
  bool hasHSL();
  void getHSL(float &h, float &s, float &l);
  void setTI(float temp, float illum);
  bool hasTI();
  Adafruit_NeoPixel *strip;
  float r, g, b, w;  // Wanted RGB color
  float h, s, l;
  bool hslIsSet;
  float temp, illum;
  bool tiIsSet;
  uint8_t *buffer;
  int count;  // Number of LEDs
  int bpp; // Number of colors per LED (3 or 4)
  IotsaPixelsourceHandler *stripHandler;
  int interval; // Number of unlit pixels between lit pixels
  int nStep;  // Number of steps to take between old and new color
  float rPrev, gPrev, bPrev, wPrev;
};

void IotsaLedstripMod::setHandler(uint8_t *_buffer, size_t _count, int _bpp, IotsaPixelsourceHandler *_handler) {
  buffer = _buffer;
  count = _count;
  bpp = _bpp;
  stripHandler = _handler;
  nStep = NSTEP;
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
      r = r1+m;
      g = g1+m;
      b = b1+m;
      if (bpp == 4) {
        w = min(r, min(g, b));
        r -= w;
        g -= w;
        b -= w;
        if (w < 0) w = 0;
        if (w > 1) w = 1;

      }
      if (r < 0) r = 0;
      if (r >= 1) r = 1;
      if (g < 0) g = 0;
      if (g >= 1) g = 1;
      if (b < 0) b = 0;
      if (b >= 1) b = 1;
}

bool IotsaLedstripMod::hasHSL()
{
  return hslIsSet;
}

void IotsaLedstripMod::setTI(float _temp, float _illum) {
  temp = _temp;
  illum = _illum;
  tiIsSet = true;
  hslIsSet = false;
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
  if (bpp == 4) {
    w = min(r, min(g, b));
    r -= w;
    g -= w;
    b -= w;
  }
  if (r < 0) r = 0;
  if (r > 1) r = 1;
  r *= illum;
  if (g < 0) g = 0;
  if (g > 1) g = 1;
  g *= illum;
  if (b < 0) b = 0;
  if (b > 1) b = 1;
  b *= illum;
  if (bpp == 4) {
    if (w < 0) w = 0;
    if (w > 1) w = 1;
    w *= illum;
  }
}


bool IotsaLedstripMod::hasTI()
{
  return tiIsSet;
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
    float temp = server->arg("temperature").toFloat();
    float illum = server->arg("illuminance").toFloat();
    setTI(temp, illum);
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
    if( bpp == 4 && server->hasArg("w")) {
      w = server->arg("w").toFloat();
      anyChanged = true;
    }
  }
  if( server->hasArg("interval")) {
    interval = server->arg("interval").toInt();
    anyChanged = true;
  }

  if (anyChanged) {
    configSave();
    nStep = NSTEP;
  }
  
  String message = "<html><head><title>Ledstrip Server</title></head><body><h1>Ledstrip Server</h1>";
  if (error != "") {
    message += "<p><em>" + error + "</em></p>";
  }
  message += "<form method='get'>";

  String checked = "";
  if (!hasHSL() && !hasTI()) checked = " checked";
  message += "<input type='radio' name='set' value=''" + checked + ">Set RGB value:<br>";
  message += "Red (0..1): <input type='text' name='r' value='" + String(r) +"' ><br>";
  message += "Green (0..1): <input type='text' name='g' value='" + String(g) +"' ><br>";
  message += "Blue (0..1): <input type='text' name='b' value='" + String(b) +"' ><br>";
  if (bpp == 4) 
    message += "White (0..1): <input type='text' name='w' value='" + String(w) +"' ><br>";

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
  if (bpp == 4) reply["w"] = w;
  if (hasHSL()) {
    reply["h"] = h;
    reply["s"] = s;
    reply["l"] = l;
  }
  if (hasTI()) {
    reply["temperature"] = temp;
    reply["illuminance"] = illum;
  }
  reply["interval"] = interval;
  return true;
}

bool IotsaLedstripMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  hslIsSet = request.containsKey("h")||request.containsKey("s")||request.containsKey("l");
  tiIsSet = request.containsKey("temperature")||request.containsKey("illuminance");
  if (hslIsSet) {
    h = request["h"]|0;
    s = request["s"]|0;
    l = request["l"]|0;
    setHSL(h, s, l);
  } else
  if (tiIsSet) {
    temp = request["temp"]|0;
    illum = request["illum"]|0;
    setTI(temp, illum);
  } else {
    // By default look at RGB values (using current values as default)
    r = request["r"]|r;
    g = request["g"]|g;
    b = request["b"]|b;
    if (bpp == 4) {
      w = request["w"]|w;
    }
  }

  interval = request["interval"]|interval;
  configSave();
  nStep = NSTEP;
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
  cf.get("r", r, 0.0);
  cf.get("g", g, 0.0);
  cf.get("b", b, 0.0);
  cf.get("w", w, 0.0);

  cf.get("h", h, 0.0);
  cf.get("s", s, 0.0);
  cf.get("l", l, 0.0);
  if (h > 0 || s > 0 || l > 0) hslIsSet = true;

  cf.get("temp", w, 0.0);
  cf.get("illum", w, 0.0);
  if (temp > 0 || illum > 0) tiIsSet = true;

  cf.get("interval", interval, 0);
}

void IotsaLedstripMod::configSave() {
  IotsaConfigFileSave cf("/config/ledstrip.cfg");
  cf.put("r", r);
  cf.put("g", g);
  cf.put("b", b);
  cf.put("w", w);
  
  if (hslIsSet) {
    cf.put("h", h);
    cf.put("s", s);
    cf.put("l", l);
  }
  if (tiIsSet) {
    cf.put("temp", temp);
    cf.put("illum", illum);
  }
  cf.put("interval", interval);
}

void IotsaLedstripMod::setup() {
  configLoad();
  rPrev = gPrev = bPrev = 0;
  nStep = NSTEP;
}

void IotsaLedstripMod::loop() {
  if (nStep <= 0) {
    nStep = 0;
    rPrev = r;
    gPrev = g;
    bPrev = b;
    wPrev = w;
  } else {
    nStep--;
  //  strip->clear();
  //  strip->show();
    float curR = ((rPrev*nStep) + (r*(NSTEP-nStep)))/NSTEP;
    float curG = ((gPrev*nStep) + (g*(NSTEP-nStep)))/NSTEP;
    float curB = ((bPrev*nStep) + (b*(NSTEP-nStep)))/NSTEP;
    float curW = ((wPrev*nStep) + (w*(NSTEP-nStep)))/NSTEP;
    int _r, _g, _b, _w;
    _r = curR*256;
    _g = curG*256;
    _b = curB*256;
    _w = curW*256;
    if (_r>255) _r = 255;
    if (_g>255) _g = 255;
    if (_b>255) _b = 255;
    if (_w>255) _w = 255;
    IFDEBUG IotsaSerial.print("r=");
    IFDEBUG IotsaSerial.print(_r);
    IFDEBUG IotsaSerial.print(",g=");
    IFDEBUG IotsaSerial.print(_g);
    IFDEBUG IotsaSerial.print(",b=");
    IFDEBUG IotsaSerial.print(_b);
    IFDEBUG IotsaSerial.print(",w=");
    IFDEBUG IotsaSerial.print(_w);
    IFDEBUG IotsaSerial.print(",count=");
    IFDEBUG IotsaSerial.print(count);
    IFDEBUG IotsaSerial.print(",nStep=");
    IFDEBUG IotsaSerial.println(nStep);
    if (buffer != NULL && count != 0 && stripHandler != NULL) {
      bool change = false;
      uint8_t *p = buffer;
      for (int i=0; i<count; i++) {
        int wtdR = _r;
        int wtdG = _g;
        int wtdB = _b;
        int wtdW = _w;
        if (interval > 1 && i % interval != 0) {
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
  }
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

