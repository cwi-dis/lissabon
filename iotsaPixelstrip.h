#ifndef _IOTSAPIXELSTRIP_H_
#define _IOTSAPIXELSTRIP_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include <Adafruit_NeoPixel.h>

#ifdef IOTSA_WITH_API
#define IotsaPixelstripModBaseMod IotsaApiMod
#else
#define IotsaPixelstripModBaseMod IotsaMod
#endif

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
    gammaTable(NULL)
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
  Adafruit_NeoPixel *strip;
  uint8_t *buffer;
  int bpp;
  int count;
  int stripType;
  int pin;
  float gamma;
  uint8_t *gammaTable;
};

#endif
