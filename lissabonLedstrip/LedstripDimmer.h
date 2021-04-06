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
  virtual void getHandler(JsonObject& reply) override;
  virtual bool putHandler(const JsonVariant& request) override;
  virtual bool formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) override;
  virtual bool configLoad(IotsaConfigFileLoad& cf, const String& name) override;
  virtual void configSave(IotsaConfigFileSave& cf, const String& name) override;
  virtual void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) override;
  virtual void formHandler_TD(String& message, bool includeConfig);

  void setHandler(uint8_t *_buffer, size_t _count, int bpp, IotsaPixelsourceHandler *handler);
protected:
  IotsaPixelstripMod& mod;
  typedef enum {calibration_normal, calibration_rgb, calibration_alternating, calibration_hard} CalibrationMode;
  CalibrationMode calibrationMode; // When true, alternate pixels use RGBW and RGB
  float calibrationData[8];
  int count;  // Number of LEDs
  int bpp; // Number of colors per LED (3 or 4)
  uint8_t *buffer = NULL; // per-pixel 8bit RGBW values (to pass to pixelstrip module)
  float *pixelLevels = NULL; // per-pixel intensities
  IotsaPixelsourceHandler *stripHandler;

  void updateColorspace(float whiteTemperature, float whiteBrightness);
  float maxLevelCorrectColor();
  String colorDump();
  Colorspace rgbwSpace;

  float focalPoint;  // Where the focus of the light is (0.0 .. 1.0)
  float focalSpread;  // How wide the focus is (0.0 .. 1.0)
  float levelFunc(float x, float spreadOverride=-1); // Relative level at point (0..1)
  float levelFuncCumulative(float x, float spreadOverride=-1); // Cumulative level at point
};
};
#endif // _LEDSTRIPDIMMER_H_