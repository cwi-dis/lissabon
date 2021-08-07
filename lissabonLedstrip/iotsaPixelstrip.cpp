#include "iotsa.h"
#include "iotsaPixelstrip.h"
#include "iotsaConfigFile.h"


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
    if( server->hasArg("gamma")) {
      if (needsAuthentication()) return;
      gamma = server->arg("gamma").toFloat();
      if (gamma > 1.0) gamma = 2.2;
      if (gamma < 1.0) gamma = 1.0;
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
#define STRINGIFY1(x) #x
#define STRINGIFY(x) STRINGIFY1(x)
#define IOTSA_NEOPIXEL_TYPE STRINGIFY(IOTSA_NPB_FEATURE)
  message += "NeoPixel type: " + String(IOTSA_NEOPIXEL_TYPE) + "<br>";
  message += "LEDs per NeoPixel: " + String(IOTSA_NPB_BPP) + "<br>";
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
  IFDEBUG IotsaSerial.printf("setup count=%d bpp=%d pin=%d\n", count, IOTSA_NPB_BPP, pin);
  if (strip) delete strip;
  strip = NULL;
  strip = new IotsaNeoPixelBus(count, pin);
  strip->Begin();
#ifdef IOTSA_NPB_POWER_PIN
  pinMode(IOTSA_NPB_POWER_PIN, OUTPUT);
  powerOff(true);
#endif
  if (buffer) free(buffer);
  buffer = (uint8_t *)malloc(count*IOTSA_NPB_BPP);
  if (buffer == NULL) {
    IotsaSerial.println("No memory");
  }
  memset(buffer, 0, count*IOTSA_NPB_BPP);
#if 1
  if (gammaConverter) delete gammaConverter;
  if (gamma > 1) {
    // Ignore gamma value, just use pre-defined 2.2 value from IotsaNeoPixelBus
    gammaConverter = new NeoGamma<NeoGammaTableMethod>();
  }
#else
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
#endif
  pixelSourceCallback();
  if (source) {
    source->setHandler(buffer, count, IOTSA_NPB_BPP, this);
  }
}

void IotsaPixelstripMod::powerOn(bool force) {
#ifdef IOTSA_NPB_POWER_PIN
  if (isPowerOn && !force) return;
  IFDEBUG IotsaSerial.println("poweron");
  isPowerOn = true;
  //
  // The powerpin should connect to a mosfet or something that enables power to
  // the ledstrip when high (and disables power when low or floating)
  //
  gpio_hold_dis((gpio_num_t)IOTSA_NPB_POWER_PIN);
  digitalWrite(IOTSA_NPB_POWER_PIN, HIGH);
  gpio_hold_en((gpio_num_t)IOTSA_NPB_POWER_PIN);
  //
  // We also ensure the ledstrip DATA output pin will carry the data signal.
  // This is a huge hack: We re-set the matrix to connect the pixelstrip output,
  // which uses the i2s in our case, to the right GPIO pin.
  //
  gpio_reset_pin((gpio_num_t)pin);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, 0);
  gpio_matrix_out(pin, I2S1O_DATA_OUT23_IDX, false, false);
  delay(1);
  //
  // We clear the strip twice, because pixels may come up with random colors
  // and the may be slow coming up.
  //
  if (strip) {
    strip->ClearTo(RgbColor(0));
    strip->Show();
    strip->Dirty();
    strip->Show();
  }
#endif // IOTSA_NPB_POWER_PIN
}

void IotsaPixelstripMod::powerOff(bool force) {
#ifdef IOTSA_NPB_POWER_PIN
  if (!isPowerOn && !force) return;
  delay(1);
  IFDEBUG IotsaSerial.println("poweroff");
  isPowerOn = false;
  //
  // We also try to float the ledstrip DATA output pin (to forestall leaking
  // current from data to very dimly power the leds).
  // See the comment in powerOn to see what a hack this is.
  //
  gpio_reset_pin((gpio_num_t)pin);
  pinMode(pin, INPUT);
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
    reply["bpp"] = IOTSA_NPB_BPP;
    reply["gamma"] = gamma;
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
    if (reqObj.containsKey("gamma")) {
      gamma = reqObj["gamma"];
      if (gamma > 1.0) gamma = 2.2;
      if (gamma < 1.0) gamma = 1.0;
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
  for (int i=0; i < count; i++) {
    uint8_t r = *ptr++;
    uint8_t g = *ptr++;
    uint8_t b = *ptr++;
    if (r||g||b) anyOn = true;
    if (IOTSA_NPB_BPP == 4) {
      uint8_t w = *ptr++;
      if (w) anyOn = true;
      RgbwColor color = RgbwColor(r, g, b, w);
      if (gammaConverter) color = gammaConverter->Correct(color);
      strip->SetPixelColor(i, color);
    } else {
      RgbColor color = RgbColor(r, g, b);
      if (gammaConverter) color = gammaConverter->Correct(color);
      strip->SetPixelColor(i, color);
    }
  }
  if (anyOn) powerOn();
  strip->Show();
  if (!anyOn) powerOff();
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
  cf.get("gamma", gamma, 1.0);
}

void IotsaPixelstripMod::configSave() {
  IotsaConfigFileSave cf("/config/pixelstrip.cfg");
  cf.put("pin", pin);
  cf.put("count", count);
  cf.put("gamma", gamma);
}

void IotsaPixelstripMod::loop() {
#ifdef TESTMODE_PIN
  if (digitalRead(TESTMODE_PIN) && buffer != 0) {
    int value = (millis() >> 3) & 0xff;  // Loop intensities every 2 seconds, approximately
    int bits = (millis() >> 11) % ((1 << IOTSA_NPB_BPP)-1); // Cycle over which colors to light
    if (bits == 0) bits = ((1 << IOTSA_NPB_BPP)-1);
    for (int i=0; i < count; i++) {
      for (int b=0; b < IOTSA_NPB_BPP; b++) {
        int thisValue = 0;
        if (bits & (1<<((b+i) % IOTSA_NPB_BPP))) {
          thisValue = value;
        }
        buffer[i*IOTSA_NPB_BPP+b] = thisValue;
      }
    }
    pixelSourceCallback();
  }
#endif
}
