#ifndef _LEDSTRIPDIMMER_H_
#define _LEDSTRIPDIMMER_H_
#include "iotsa.h"
#include "AbstractDimmer.h"
#include "iotsaPixelstrip.h"

#include "NPBColorLib.h"

namespace Lissabon {

class LedstripDimmer : public AbstractDimmer, public IotsaPixelsource {
public:
  LedstripDimmer(int _num, IotsaPixelstripMod& _mod, DimmerCallbacks *_callbacks);
  void setup();
  virtual void updateDimmer() override;
  bool available() override;
  void identify();
  void loop();
  // Overrides
  virtual bool getHandler(JsonObject& reply) override;
  virtual bool putHandler(const JsonVariant& request) override;
  virtual bool putConfigHandler(const JsonVariant& request) override;
  virtual bool handlerArgs(IotsaWebServer *server) override;
  virtual bool handlerConfigArgs(IotsaWebServer *server) override;
  virtual void configLoad(IotsaConfigFileLoad& cf) override;
  virtual void configSave(IotsaConfigFileSave& cf) override;
  virtual String handlerForm() override;
  virtual String handlerConfigForm() override;

  void setHandler(uint8_t *_buffer, size_t _count, int bpp, IotsaPixelsourceHandler *handler);
protected:
  IotsaPixelstripMod& mod;
  bool calibrationMode; // When true, alternate pixels use RGBW and RGB
  int count;  // Number of LEDs
  int bpp; // Number of colors per LED (3 or 4)
  uint8_t *buffer = NULL; // per-pixel 8bit RGBW values (to pass to pixelstrip module)
  float *pixelLevels = NULL; // per-pixel intensities
  IotsaPixelsourceHandler *stripHandler;

  void updateColorspace(float whiteTemperature, float whiteBrightness);
  float maxLevelCorrectColor();
  Colorspace rgbwSpace;

  float focalPoint;  // Where the focus of the light is (0.0 .. 1.0)
  float focalSpread;  // How wide the focus is (0.0 .. 1.0)
  float levelFunc(float x, float spreadOverride=-1); // Relative level at point (0..1)
  float levelFuncCumulative(float x, float spreadOverride=-1); // Cumulative level at point
};
};
#endif // _LEDSTRIPDIMMER_H_