#include "iotsa.h"
#include "iotsaPixelstrip.h"
#include "iotsaConfigFile.h"

#define NEOPIXEL_PIN 4  // "Normal" pin for NeoPixel
#define NEOPIXEL_TYPE (NEO_GRB + NEO_KHZ800)
#define NEOPIXEL_COUNT 1 // Default number of pixels
#define NEOPIXEL_BPP 3  // Default number of colors per pixel

// Define TESTMODE_PIN if there is a test-mode switch attached
// to that pin. The LEDs will show a looping pattern if that input is high.
#undef TESTMODE_PIN
//#define TESTMODE_PIN 5

#ifdef IOTSA_WITH_WEB

void
IotsaPixelstripMod::handler() {
  if( server->hasArg("setIndex") && server->hasArg("setValue")) {
    int idx = server->arg("setIndex").toInt();
    int val = server->arg("setValue").toInt();
    if (buffer && idx >= 0 && idx <= count*bpp) {
      buffer[idx] = val;
      pixelSourceCallback();
    }
  } else if (server->hasArg("clear")) {
    if (buffer) {
      memset(buffer, 0, count*bpp);
      pixelSourceCallback();
    }
  } else {
    bool anyChanged = false;
    if( server->hasArg("pin")) {
      if (needsAuthentication()) return;
      pin = server->arg("pin").toInt();
      anyChanged = true;
    }
    if( server->hasArg("stripType")) {
      if (needsAuthentication()) return;
      stripType = server->arg("stripType").toInt();
      anyChanged = true;
    }
    if( server->hasArg("count")) {
      if (needsAuthentication()) return;
      count = server->arg("count").toInt();
      anyChanged = true;
    }
    if( server->hasArg("bpp")) {
      if (needsAuthentication()) return;
      bpp = server->arg("bpp").toInt();
      anyChanged = true;
    }
    if( server->hasArg("gamma")) {
      if (needsAuthentication()) return;
      gamma = server->arg("gamma").toFloat();
      anyChanged = true;
    }
    if (anyChanged) {
      configSave();
      setupStrip();
    }
  }

  String message = "<html><head><title>Pixelstrip module</title></head><body><h1>Pixelstrip module</h1>";
  message += "<h2>Configuration</h2><form method='get'>GPIO pin: <input name='pin' value='" + String(pin) + "'><br>";
  message += "Neopixel type: <input name='stripType' value='" + String(stripType) + "'><br>";
  message += "Number of NeoPixels: <input name='count' value='" + String(count) + "'><br>";
  message += "LEDs per NeoPixel: <input name='bpp' value='" + String(bpp) + "'><br>";
  message += "Gamma (1.0 neutral, 2.2 suggested): <input name='gamma' value='" + String(gamma) + "'><br>";
  message += "<input type='submit'></form>";
  message += "<h2>Set pixel</h2><form method='get'><br>Set pixel <input name='setIndex'> to <input name='setValue'><br>";
  message += "<input type='submit'></form>";
  message += "<h2>Clear All</h2><form method='get'><input type='submit' name='clear'></form>";
  server->send(200, "text/html", message);
}

String IotsaPixelstripMod::info() {
  String message = "<p>Built with pixelstrip module. See <a href=\"/pixelstrip\">/pixelstrip</a> to change NeoPixel strip settings, <a href=\"/api/pixelstrip\">/api/pixelstrip</a> and <a href=\"/api/pixels\">/api/pixels</a> for REST API.</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

void IotsaPixelstripMod::setup() {
  configLoad();
  setupStrip();
#ifdef TESTMODE_PIN
  pinMode(TESTMODE_PIN, INPUT);
#endif
}

void IotsaPixelstripMod::setupStrip() {
  IFDEBUG IotsaSerial.printf("setup count=%d bpp=%d pin=%d striptype=%d\n", count, bpp, pin, stripType);
  if (strip) delete strip;
  if (buffer) free(buffer);
  buffer = (uint8_t *)malloc(count*bpp);
  if (buffer == NULL) {
    IotsaSerial.println("No memory");
  }
  memset(buffer, 0, count*bpp);
  if (gammaTable) free(gammaTable);
  gammaTable = NULL;
  if (gamma != 0 && gamma != 1) {
    gammaTable = (uint8_t *)malloc(256);
    if (gammaTable == NULL) {
      IotsaSerial.println("No memory");
    }
    for(int i=0; i<256; i++) {
      float gval = powf((float)i/256.0, gamma);
      int ival = (int)(gval * 256);
      if (ival < 0) ival = 0;
      if (ival > 255) ival = 255;
      gammaTable[i] = ival;
    }
  }
  strip = new Adafruit_NeoPixel(count, pin, stripType);
  pixelSourceCallback();
  if (source) {
    source->setHandler(buffer, count, bpp, this);
  }
}

#ifdef IOTSA_WITH_API
bool IotsaPixelstripMod::getHandler(const char *path, JsonObject& reply) {
  if (strcmp(path, "/api/pixelstrip") == 0) {
    reply["pin"] = pin;
    reply["stripType"] = stripType;
    reply["count"] = count;
    reply["bpp"] = bpp;
    reply["gamma"] = gamma;
    return true;
  } else if (strcmp(path, "/api/pixels") == 0) {
    JsonArray& data = reply.createNestedArray("data");
    if (buffer) {
      for(int i=0; i<count*bpp; i++) {
        data.add(buffer[i]);
      }
    }
    return true;
  }
  return false;
}

bool IotsaPixelstripMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
    JsonObject& reqObj = request.as<JsonObject>();
  if (strcmp(path, "/api/pixelstrip") == 0) {
    bool anyChanged = false;
    if (reqObj.containsKey("pin")) {
      pin = reqObj.get<int>("pin");
      anyChanged = true;
    }
    if (reqObj.containsKey("stripType")) {
      stripType = reqObj.get<int>("stripType");
      anyChanged = true;
    }
    if (reqObj.containsKey("count")) {
      count = reqObj.get<int>("count");
      anyChanged = true;
    }
    if (reqObj.containsKey("bpp")) {
      bpp = reqObj.get<int>("bpp");
      anyChanged = true;
    }
    if (reqObj.containsKey("gamma")) {
      gamma = reqObj.get<int>("gamma");
      anyChanged = true;
    }
    if (anyChanged) {
      configSave();
      setupStrip();
    }
    return anyChanged;
  } else if (strcmp(path, "/api/pixels") == 0) {
    if (buffer == NULL) return false;
    if (reqObj.containsKey("clear") && reqObj.get<bool>("clear")) {
      memset(buffer, 0, count*bpp);
    }
    int start = 0;
    if (reqObj.containsKey("start")) {
      start = reqObj.get<int>("start");
    }
    JsonArray& data = reqObj.get<JsonArray>("data");
    for(JsonArray::iterator it=data.begin(); it!=data.end(); ++it) {
      if (start >= count*bpp) return false;
      int value = it->as<int>();
      buffer[start++] = value;
    }
    pixelSourceCallback();
    return true;
  }
  return false;
}
#endif // IOTSA_WITH_API

void IotsaPixelstripMod::pixelSourceCallback() {
  if (buffer == NULL) {
    return;
  }
  uint8_t *ptr = buffer;
  IFDEBUG IotsaSerial.println("Show ");
  strip->begin();
  for (int i=0; i < count; i++) {
    uint32_t color = 0;
    for (int b=0; b<bpp; b++) {
      int cval = *ptr++;
      if (gammaTable) cval = gammaTable[cval];
      color = color << 8 | cval;
    }
    IFDEBUG IotsaSerial.print(color, HEX);
    IFDEBUG IotsaSerial.print(' ');
    strip->setPixelColor(i, color);
  }
  strip->show();
  IFDEBUG IotsaSerial.println(" called");
}

void IotsaPixelstripMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/pixelstrip", std::bind(&IotsaPixelstripMod::handler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/pixelstrip", true, true);
  api.setup("/api/pixels", true, true);
  name = "pixelstrip";
#endif
}

void IotsaPixelstripMod::configLoad() {
  IotsaConfigFileLoad cf("/config/pixelstrip.cfg");
  cf.get("pin", pin, NEOPIXEL_PIN);
  cf.get("stripType", stripType, NEOPIXEL_TYPE);
  cf.get("count", count, NEOPIXEL_COUNT);
  cf.get("bpp", bpp, NEOPIXEL_BPP);
  cf.get("gamma", gamma, 1.0);
}

void IotsaPixelstripMod::configSave() {
  IotsaConfigFileSave cf("/config/pixelstrip.cfg");
  cf.put("pin", pin);
  cf.put("stripType", stripType);
  cf.put("count", count);
  cf.put("bpp", bpp);
  cf.put("gamma", gamma);
}

void IotsaPixelstripMod::loop() {
#ifdef TESTMODE_PIN
  if (digitalRead(TESTMODE_PIN) && buffer != 0) {
    int value = (millis() >> 3) & 0xff;  // Loop intensities every 2 seconds, approximately
    int bits = (millis() >> 11) % ((1 << bpp)-1); // Cycle over which colors to light
    if (bits == 0) bits = ((1 << bpp)-1);
    for (int i=0; i < count; i++) {
      for (int b=0; b < bpp; b++) {
        int thisValue = 0;
        if (bits & (1<<((b+i) % bpp))) {
          thisValue = value;
        }
        buffer[i*bpp+b] = thisValue;
      }
    }
    pixelSourceCallback();
  }
#endif
}
