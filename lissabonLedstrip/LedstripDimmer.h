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
  int count;  // Number of LEDs
  int bpp; // Number of colors per LED (3 or 4)
  uint8_t *pixelBuffer = NULL; // per-pixel 8bit RGBW values (to pass to pixelstrip module)
  float *pixelLevels = NULL; // per-pixel relative intensities
  IotsaPixelsourceHandler *stripHandler;

  void updateColorspace(float whiteTemperature, float whiteBrightness);
  void calcLevel();
  void calcPixelLevels(float wantedLevel);
  float maxLevelCorrectColor();
  String colorDump();
  Colorspace rgbwSpace;
  RgbwFColor correctRgbwColor;
  float calibrationData[8]; // for calibration: 2 sets of RGBW values, for alternating pixels.
  bool inCalibrationMode = false; // Use calibrationData in stead of correctRgbwColor and
  
  float focalPoint;  // Where the focus of the light is (0.0 .. 1.0)
  float focalSpread;  // How wide the focus is (0.0 .. 1.0)
  float levelFuncCumulative(int left, int right, float spreadFactor); // Cumulative level between pixels [left, right)
};
};
#endif // _LEDSTRIPDIMMER_H_