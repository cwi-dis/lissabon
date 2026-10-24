#include "iotsa.h"
#include "iotsaPixelstrip.h"
#include "iotsaConfigFile.h"

#define STRINGIFY1(x) #x
#define STRINGIFY(x) STRINGIFY1(x)
#define IOTSA_NEOPIXEL_TYPE STRINGIFY(IOTSA_NPB_FEATURE)
#define IOTSA_NEOPIXEL_METHOD STRINGIFY(IOTSA_NPB_METHOD)

// The pin is wiring, a device setting (for lissabon it should be fixed at
// compile time, cwi-dis/lissabon#39); the length is tuning, per strip.
template<class Ctx> bool IotsaPixelstripMod::_stripArgs(Ctx& ctx) {
  IotsaContext::RightsScope rights(ctx);
  bool changed = false;
  ctx.argsNeedRight("config");
  changed |= ctx.arg("pin", pin, IotsaContext::Saved);
  ctx.argsNeedRight("lighttuning");
  changed |= ctx.arg("count", count, IotsaContext::Saved);
  if (count < 2) {
    IOTSA_LOG("Pixelstrip", "count set to 2 to workaround bug");
    count = 2;
  }
  return changed;
}

#ifdef IOTSA_WITH_WEB
// Writing pixels directly, past the dimmer, is a tool for testing the strip:
// "lighttuning", not everyday lighting.
void IotsaPixelstripMod::formHandler_args(IotsaWebContext& ctx) {
  {
    IotsaContext::RightsScope rights(ctx);
    ctx.argsNeedRight("lighttuning");
    if (_pixelArgs(ctx)) return;
  }
  bool changed = _stripArgs(ctx);
  if (ctx.failed()) return;
  if (changed) setupStrip();
}

bool IotsaPixelstripMod::_pixelArgs(IotsaWebContext& ctx) {
  if (ctx.argAction("set", IotsaContext::Transient)) {
    // Note: this sets the value for a single LED, not the value for a single NeoPixel (3/4 leds)
    int idx = 0;
    int val = 0;
    ctx.arg("setIndex", idx, IotsaContext::Transient);
    ctx.arg("setValue", val, IotsaContext::Transient);
    if (ctx.failed()) return true;
    if (pixelBuffer && idx >= 0 && idx < count*IOTSA_NPB_BPP) {
      pixelBuffer[idx] = val;
      pixelSourceCallback();
    } else {
      ctx.error("No such LED.");
    }
    return true;
  }
  if (ctx.argAction("clear", IotsaContext::Transient)) {
    if (pixelBuffer) {
      memset(pixelBuffer, 0, count*IOTSA_NPB_BPP);
      pixelSourceCallback();
    }
    return true;
  }
  return false;
}

void IotsaPixelstripMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.includesEveryday()) {
    IotsaContext::RightsScope rights(ctx);
    ctx.argsNeedRight("lighttuning");
    {
      IotsaWebContext::Form form(ctx, "Set", "Set pixel", "set");
      ctx.showField("LED", "setIndex", 0, IotsaContext::Transient, "a single LED, not a NeoPixel (3 or 4 LEDs)");
      ctx.showField("Value", "setValue", 0, IotsaContext::Transient, "0 to 255");
    }
    ctx.showButton("clear", "Clear all", IotsaContext::Transient);
  }
  if (ctx.includesSettings()) {
    IotsaContext::RightsScope rights(ctx);
    IotsaWebContext::Form form(ctx);
    ctx.argsNeedRight("config");
    ctx.showField("GPIO pin", "pin", pin, IotsaContext::Saved);
    ctx.argsNeedRight("lighttuning");
    ctx.showField("Number of NeoPixels", "count", count, IotsaContext::Saved);
    ctx.showStatus("NeoPixel type", "pixel_type", IOTSA_NEOPIXEL_TYPE);
    ctx.showStatus("Control method", "pixel_method", IOTSA_NEOPIXEL_METHOD);
    ctx.showStatus("LEDs per NeoPixel", "pixel_bpp", String(IOTSA_NPB_BPP));
  }
}
#endif // IOTSA_WITH_WEB

String IotsaPixelstripMod::info() {
  String message = "<p>Built with pixelstrip module. See <a href=\"/pixelstrip\">/pixelstrip</a> to change NeoPixel strip settings, <a href=\"/api/pixelstrip\">/api/pixelstrip</a> and <a href=\"/api/pixels\">/api/pixels</a> for REST API.</p>";
  return message;
}

void IotsaPixelstripMod::setup() {
  configLoad();
  setupStrip();
}

void IotsaPixelstripMod::setupStrip() {
  IOTSA_LOG_DEBUG("Pixelstrip", "setup count=%d bpp=%d pin=%d", count, IOTSA_NPB_BPP, pin);
  if (strip) delete strip;
  strip = NULL;
#ifdef IOTSA_NPB_POWER_PIN
  pinMode(IOTSA_NPB_POWER_PIN, OUTPUT);
  powerOff(true);
#else
  strip = new IotsaNeoPixelBus(count, pin);
  strip->Begin();
#endif
  if (pixelBuffer) free(pixelBuffer);
  pixelBuffer = (uint8_t *)malloc(count*IOTSA_NPB_BPP);
  if (pixelBuffer == NULL) {
    IOTSA_LOG("Pixelstrip", "No memory");
  }
  memset(pixelBuffer, 0, count*IOTSA_NPB_BPP);
  pixelSourceCallback();
  if (source) {
    source->setHandler(pixelBuffer, count, IOTSA_NPB_BPP, this);
  }
}

void IotsaPixelstripMod::powerOn(bool force) {
#ifdef IOTSA_NPB_POWER_PIN
  if (isPowerOn && !force) return;
  IOTSA_LOG_DEBUG("Pixelstrip", "poweron via pin %d", IOTSA_NPB_POWER_PIN);
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
  pinMode(pin, OUTPUT);
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
  IOTSA_LOG_DEBUG("Pixelstrip", "poweroff via pin %d", IOTSA_NPB_POWER_PIN);
  isPowerOn = false;
  //
  // We delete the strip, which should set the pin back to an input
  // (and therefore float it)
  //
  delete strip;
  strip = nullptr;
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


void IotsaPixelstripMod::getHandler(IotsaApiContext& ctx) {
  if (ctx.path() == "/api/pixelstrip") {
    ctx.show("pin", pin);
    ctx.show("count", count);
    ctx.show("pixel_type", IOTSA_NEOPIXEL_TYPE);
    ctx.show("pixel_bpp", IOTSA_NPB_BPP);
    ctx.show("pixel_method", IOTSA_NEOPIXEL_METHOD);
  } else if (ctx.path() == "/api/pixels") {
    JsonArray data = ctx.reply()["data"].to<JsonArray>();
    if (pixelBuffer) {
      for(int i=0; i<count*IOTSA_NPB_BPP; i++) {
        data.add(pixelBuffer[i]);
      }
    }
  }
}

// /api/pixels: clear, then data (one value per LED) from start on.
void IotsaPixelstripMod::putHandler(IotsaApiContext& ctx) {
  if (ctx.path() == "/api/pixelstrip") {
    bool changed = _stripArgs(ctx);
    if (ctx.failed()) return;
    if (changed) setupStrip();
  } else if (ctx.path() == "/api/pixels") {
    ctx.argsNeedRight("lighttuning");
    bool clear = false;
    int start = 0;
    ctx.arg("clear", clear, IotsaContext::Transient);
    ctx.arg("start", start, IotsaContext::Transient);
    bool haveData = ctx.argAction("data", IotsaContext::Transient);
    if (ctx.failed()) return;
    if (pixelBuffer == NULL) {
      ctx.error(503, "no pixel buffer");
      return;
    }
    if (clear) memset(pixelBuffer, 0, count*IOTSA_NPB_BPP);
    if (haveData) {
      JsonArray data = ctx.request()["data"].as<JsonArray>();
      for(JsonArray::iterator it=data.begin(); it!=data.end(); ++it) {
        if (start < 0 || start >= count*IOTSA_NPB_BPP) {
          ctx.error(400, "data: past the last LED");
          break;
        }
        pixelBuffer[start++] = it->as<int>();
      }
    }
    pixelSourceCallback();
  }
}

void IotsaPixelstripMod::pixelSourceCallback() {
  if (pixelBuffer == NULL) {
    return;
  }
  uint8_t *ptr = pixelBuffer;
  // IFDEBUG IotsaSerial.println("Show ");
  bool anyOn = false;
  for (int i=0; i<count*IOTSA_NPB_BPP; i++) {
    if (ptr[i] != 0) {
      anyOn = true;
      //IotsaSerial.printf("xxxjack pixel %d of %d is 0x%x\n", i,count*IOTSA_NPB_BPP, ptr[i]);
      break;
    }
  }
  if (!anyOn && !_keepPowered) {
    powerOff();
    return;
  }
  powerOn();
  if (strip == nullptr) {
    IOTSA_LOG("Pixelstrip", "strip is NULL");
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

void IotsaPixelstripMod::lateSetup() {
  name = "pixelstrip";
  // /pixelstrip (the module page) is auto-registered by this get=true api.setup().
  api.setup("pixelstrip", true, true);
  // /api/pixels is a second data endpoint on the same module -- no separate
  // web page (webPage=false); getHandler/putHandler dispatch on ctx.path().
  api.setup("pixels", true, true, false, false);
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
