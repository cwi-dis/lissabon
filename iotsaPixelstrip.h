#ifndef _IOTSAPIXELSTRIP_H_
#define _IOTSAPIXELSTRIP_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include <NeoPixelBus.h>

#ifdef IOTSA_WITH_API
#define IotsaPixelstripModBaseMod IotsaApiMod
#else
#define IotsaPixelstripModBaseMod IotsaMod
#endif

#ifndef IOTSA_NPB_FEATURE
#define IOTSA_NPB_FEATURE NeoGrbFeature
#endif
#ifndef IOTSA_NPB_BPP
#define IOTSA_NPB_BPP 3
#endif
#ifndef IOTSA_NPB_METHOD
#define IOTSA_NPB_METHOD Neo800KbpsMethod
#endif
#ifndef IOTSA_NPB_DEFAULT_PIN
#define IOTSA_NPB_DEFAULT_PIN 4  // "Normal" pin for NeoPixel
#endif
#ifndef IOTSA_NPB_DEFAULT_COUNT
#define IOTSA_NPB_DEFAULT_COUNT 1 // Default number of pixels
#endif

typedef NeoPixelBus<IOTSA_NPB_FEATURE,IOTSA_NPB_METHOD> IotsaNeoPixelBus;

class IotsaPixelsourceHandler {
public:
  virtual ~IotsaPixelsourceHandler() {};
  virtual void pixelSourceCallback() = 0;
};

class IotsaPixelsource {
public:
  virtual ~IotsaPixelsource() {}
  virtual void setHandler(uint8_t *_buffer, size_t _count, int bpp, IotsaPixelsourceHandler *handler) = 0;

};

class IotsaPixelstripMod : public IotsaPixelstripModBaseMod, public IotsaPixelsourceHandler {
public:
  IotsaPixelstripMod(IotsaApplication& app)
  : IotsaPixelstripModBaseMod(app),
    source(NULL),
    strip(NULL),
    gammaConverter(NULL)
  {}
  using IotsaPixelstripModBaseMod::IotsaPixelstripModBaseMod;
  void setup();
  void serverSetup();
  void loop();
  String info();
  void setPixelsource(IotsaPixelsource *_source) { source = _source; };
  void pixelSourceCallback();
protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  void configLoad();
  void configSave();
  void setupStrip();
  void handler();
  IotsaPixelsource *source;
  IotsaNeoPixelBus *strip;
  uint8_t *buffer;
  int count;
  int pin;
  float gamma;
#if 1
  NeoGamma<NeoGammaTableMethod> *gammaConverter;
#else
  uint8_t *gammaTable;
#endif
};

#endif
