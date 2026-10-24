// Not the same as iotsaDMXLedstrip's iotsaPixelstrip (cwi-dis/iotsaDMXLedstrip),
// although the hardware is much the same. This one is ambient lighting: the
// LEDs together give everyday light (a strip on the ceiling), driven through a
// dimmer. iotsaDMXLedstrip's is for stage effects: each LED feeds a fibre
// strand, and DMX or REST drives effects many frames a second. So the rights
// differ: here the look needs "lighttuning"; there the pixel data is open and
// show setup needs "dmx".
//
#ifndef _IOTSAPIXELSTRIP_H_
#define _IOTSAPIXELSTRIP_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include <NeoPixelBus.h>

#ifndef IOTSA_NPB_FEATURE
#define IOTSA_NPB_FEATURE NeoGrbFeature
#endif

const int IOTSA_NPB_BPP = (IOTSA_NPB_FEATURE::PixelSize);

#ifndef IOTSA_NPB_METHOD
#define IOTSA_NPB_METHOD Neo800KbpsMethod
#ifdef ESP32
// A hack-ish solution to float the output pin when we powerdown the strip.
#define IOTSA_NPB_FLOAT_OUTPUT_PIN() (rmt_set_idle_level(RMT_CHANNEL_6, true, RMT_IDLE_LEVEL_HIGH))
#define IOTSA_NPB_ENABLE_OUTPUT_PIN() (rmt_set_idle_level(RMT_CHANNEL_6, true, RMT_IDLE_LEVEL_LOW))
#endif
#endif

#ifndef IOTSA_NPB_FLOAT_OUTPUT_PIN
#define IOTSA_NPB_FLOAT_OUTPUT_PIN()
#endif
#ifndef IOTSA_NPB_ENABLE_OUTPUT_PIN
#define IOTSA_NPB_ENABLE_OUTPUT_PIN()
#endif

#ifndef IOTSA_NPB_DEFAULT_PIN
#define IOTSA_NPB_DEFAULT_PIN 4  // "Normal" pin for NeoPixel
#endif

#ifndef IOTSA_NPB_DEFAULT_COUNT
#define IOTSA_NPB_DEFAULT_COUNT 2 // Default number of pixels, Note: count=1 will trigger bug in NPB.
#endif

typedef NeoPixelBus<IOTSA_NPB_FEATURE,IOTSA_NPB_METHOD> IotsaNeoPixelBus;

class IotsaPixelsourceHandler {
public:
  virtual ~IotsaPixelsourceHandler() {};
  virtual void pixelSourceCallback() = 0;
  virtual void powerOn(bool force=false) = 0;
  virtual void powerOff(bool force=false) = 0;
};

class IotsaPixelsource {
public:
  virtual ~IotsaPixelsource() {}
  virtual void setHandler(uint8_t *_buffer, size_t _count, int bpp, IotsaPixelsourceHandler *handler) = 0;

};

class IotsaPixelstripMod : public IotsaModule, public IotsaPixelsourceHandler {
public:
  IotsaPixelstripMod(IotsaApplication& app)
  : IotsaModule(app),
    source(NULL),
    strip(NULL)
  {}
  using IotsaModule::IotsaModule;
  void setup();
  void lateSetup() override;
  void loop();
  String info();
  void setPixelsource(IotsaPixelsource *_source) { source = _source; };
  void pixelSourceCallback();
  void powerOn(bool force=false);
  void powerOff(bool force=false);
  // Keep the strip powered even while every pixel is (momentarily) all-zero,
  // e.g. the dark phase of a status-indicator blink/breathe/gap cycle
  // (cwi-dis/lissabon#28) -- without this, pixelSourceCallback() would power-
  // cycle the strip on every such phase.
  void keepPowered(bool keep) { _keepPowered = keep; }
protected:
  // Two endpoints: /api/pixelstrip (the settings), /api/pixels (the pixel data).
  void getHandler(IotsaApiContext& ctx) override;
  void putHandler(IotsaApiContext& ctx) override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Pixelstrip"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
  void configLoad();
  void configSave();
  void setupStrip();
  // pin and count, for both contexts. Returns true if one changed.
  template<class Ctx> bool _stripArgs(Ctx& ctx);
#ifdef IOTSA_WITH_WEB
  // "set pixel" and "clear all". Returns true if it was one of those.
  bool _pixelArgs(IotsaWebContext& ctx);
#endif
  IotsaPixelsource *source;
  IotsaNeoPixelBus *strip;
  uint8_t *pixelBuffer;
  int count;
  int pin;
  bool _keepPowered = false;
#ifdef IOTSA_NPB_POWER_PIN
  bool isPowerOn;
#endif
};

#endif
