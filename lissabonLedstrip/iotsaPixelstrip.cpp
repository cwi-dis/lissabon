#include "iotsa.h"
#include "iotsaPixelstrip.h"
#include "iotsaConfigFile.h"

#define STRINGIFY1(x) #x
#define STRINGIFY(x) STRINGIFY1(x)
#define IOTSA_NEOPIXEL_TYPE STRINGIFY(IOTSA_NPB_FEATURE)
#define IOTSA_NEOPIXEL_METHOD STRINGIFY(IOTSA_NPB_METHOD)

#ifdef IOTSA_WITH_WEB

void
IotsaPixelstripMod::handler() {
  if( server->hasArg("setIndex") && server->hasArg("setValue")) {
    // Note: this sets the value for a single LED, not the value for a single NeoPixel (3/4 leds)
    int idx = server->arg("setIndex").toInt();
    int val = server->arg("setValue").toInt();
    if (buffer && idx >= 0 && idx <= count*IOTSA_NPB_BPP) {
      buffer[idx] = val;
      pixelSourceCallback();
    }
  } else if (server->hasArg("clear")) {
    if (buffer) {
      memset(buffer, 0, count*IOTSA_NPB_BPP);
      pixelSourceCallback();
    }
  } else {
    bool anyChanged = false;
    if( server->hasArg("pin")) {
      if (needsAuthentication()) return;
      pin = server->arg("pin").toInt();
      anyChanged = true;
    }
    if( server->hasArg("count")) {
      if (needsAuthentication()) return;
      count = server->arg("count").toInt();
      anyChanged = true;
    }
    if (anyChanged) {
      configSave();
      setupStrip();
    }
  }

  String message = "<html><head><title>Pixelstrip module</title></head><body><h1>Pixelstrip module</h1>";
  message += "<h2>Configuration</h2><form method='get'>GPIO pin: <input name='pin' value='" + String(pin) + "'><br>";
  message += "Number of NeoPixels: <input name='count' value='" + String(count) + "'><br>";
  message += "NeoPixel type: " + String(IOTSA_NEOPIXEL_TYPE) + ", control method: " + String(IOTSA_NEOPIXEL_METHOD) + "<br>";
  message += "LEDs per NeoPixel: " + String(IOTSA_NPB_BPP) + "<br>";
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
}

void IotsaPixelstripMod::setupStrip() {
  IFDEBUG IotsaSerial.printf("setup count=%d bpp=%d pin=%d\n", count, IOTSA_NPB_BPP, pin);
  if (strip) delete strip;
  strip = NULL;
#ifdef IOTSA_NPB_POWER_PIN
  pinMode(IOTSA_NPB_POWER_PIN, OUTPUT);
  powerOff(true);
#else
  strip = new IotsaNeoPixelBus(count, pin);
  strip->Begin();
#endif
  if (buffer) free(buffer);
  buffer = (uint8_t *)malloc(count*IOTSA_NPB_BPP);
  if (buffer == NULL) {
    IotsaSerial.println("No memory");
  }
  memset(buffer, 0, count*IOTSA_NPB_BPP);
  pixelSourceCallback();
  if (source) {
    source->setHandler(buffer, count, IOTSA_NPB_BPP, this);
  }
}

void IotsaPixelstripMod::powerOn(bool force) {
#ifdef IOTSA_NPB_POWER_PIN
  if (isPowerOn && !force) return;
  IFDEBUG IotsaSerial.printf("PixelStrip: poweron via pin %d\n", IOTSA_NPB_POWER_PIN);
  isPowerOn = true;
  //
  // The powerpin should connect to a mosfet or something that enables power to
  // the ledstrip when high (and disables power when low or floating)
  //
  gpio_hold_dis((gpio_num_t)IOTSA_NPB_POWER_PIN);
  digitalWrite(IOTSA_NPB_POWER_PIN, HIGH);
  gpio_hold_en((gpio_num_t)IOTSA_NPB_POWER_PIN);
 //
  // We allocate the strip, which should initialize it
  //
  strip = new IotsaNeoPixelBus(count, pin);
  strip->Begin();
  delay(1);
  //
  // We clear the strip twice, because pixels may come up with random colors
  // and the may be slow coming up.
  //
#endif // IOTSA_NPB_POWER_PIN
}

void IotsaPixelstripMod::powerOff(bool force) {
#ifdef IOTSA_NPB_POWER_PIN
  if (!isPowerOn && !force) return;
  delay(1);
  IFDEBUG IotsaSerial.printf("PixelStrip: poweroff via pin %d\n", IOTSA_NPB_POWER_PIN);
  isPowerOn = false;
  //
  // We delete the strip, which should set the pin back to an input
  // (and therefore float it)
  //
  delete strip;
  strip = nullptr;
  //
  // The powerpin should connect to a mosfet or something that enables power to
  // the ledstrip when high (and disables power when low or floating)
  //
  gpio_hold_dis((gpio_num_t)IOTSA_NPB_POWER_PIN);
  digitalWrite(IOTSA_NPB_POWER_PIN, LOW);
  gpio_hold_en((gpio_num_t)IOTSA_NPB_POWER_PIN);
#endif // IOTSA_NPB_POWER_PIN
}


#ifdef IOTSA_WITH_API
bool IotsaPixelstripMod::getHandler(const char *path, JsonObject& reply) {
  if (strcmp(path, "/api/pixelstrip") == 0) {
    reply["pin"] = pin;
    reply["count"] = count;
    reply["pixel_type"] = IOTSA_NEOPIXEL_TYPE;
    reply["pixel_bpp"] = IOTSA_NPB_BPP;
    reply["pixel_method"] = IOTSA_NEOPIXEL_METHOD;
    return true;
  } else if (strcmp(path, "/api/pixels") == 0) {
    JsonArray data = reply.createNestedArray("data");
    if (buffer) {
      for(int i=0; i<count*IOTSA_NPB_BPP; i++) {
        data.add(buffer[i]);
      }
    }
    return true;
  }
  return false;
}

bool IotsaPixelstripMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
    JsonObject reqObj = request.as<JsonObject>();
  if (strcmp(path, "/api/pixelstrip") == 0) {
    bool anyChanged = false;
    if (reqObj.containsKey("pin")) {
      pin = reqObj["pin"];
      anyChanged = true;
    }
    if (reqObj.containsKey("count")) {
      count = reqObj["count"];
      if (count < 2) {
        IotsaSerial.println("count set to 2 to workaround bug");
        count = 2;
      }
      anyChanged = true;
    }
    if (anyChanged) {
      configSave();
      setupStrip();
    }
    return anyChanged;
  } else if (strcmp(path, "/api/pixels") == 0) {
    if (buffer == NULL) return false;
    if (reqObj.containsKey("clear") && reqObj["clear"]) {
      memset(buffer, 0, count*IOTSA_NPB_BPP);
    }
    int start = 0;
    if (reqObj.containsKey("start")) {
      start = reqObj["start"];
    }
    JsonArray data = reqObj["data"];
    for(JsonArray::iterator it=data.begin(); it!=data.end(); ++it) {
      if (start >= count*IOTSA_NPB_BPP) return false;
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
  // IFDEBUG IotsaSerial.println("Show ");
  bool anyOn = false;
  for (int i=0; i<count*IOTSA_NPB_BPP; i++) {
    if (ptr[i] != 0) {
      anyOn = true;
      IotsaSerial.printf("xxxjack pixel %d of %d is 0x%x\n", i,count*IOTSA_NPB_BPP, ptr[i]);
      break;
    }
  }
  if (!anyOn) {
    powerOff();
    return;
  }
  powerOn();
  if (strip == nullptr) {
    IotsaSerial.println("IotsaPixelStrip: strip is NULL");
    return;
  }
  for (int i=0; i < count; i++) {
    uint8_t r = *ptr++;
    uint8_t g = *ptr++;
    uint8_t b = *ptr++;
    if (IOTSA_NPB_BPP == 4) {
      uint8_t w = *ptr++;
      RgbwColor color = RgbwColor(r, g, b, w);
      strip->SetPixelColor(i, color);
    } else {
      RgbColor color = RgbColor(r, g, b);
      strip->SetPixelColor(i, color);
    }
  }
  strip->Show();
  // IFDEBUG IotsaSerial.println(" called");
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
  cf.get("pin", pin, IOTSA_NPB_DEFAULT_PIN);
  cf.get("count", count, IOTSA_NPB_DEFAULT_COUNT);
}

void IotsaPixelstripMod::configSave() {
  IotsaConfigFileSave cf("/config/pixelstrip.cfg");
  cf.put("pin", pin);
  cf.put("count", count);
}

void IotsaPixelstripMod::loop() {
}
